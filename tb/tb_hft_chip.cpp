#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include "Vhft_chip.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "orderbook.h"
#include "arb_trader.h"
#include "momentum_trader.h"
#include "ema_trader.h"

// Whole-chip TB. The model reproduces the full datapath cycle-accurately: 4
// orderbooks -> registered copies -> 3 traders -> fixed-priority arbitration
// (arb>mom>ema) -> registered output. The chip only exposes trades at its
// pads, so the model tracks all internal state to predict the trade bus.
//
// latencies modeled:
//   inputs register at the pad boundary (+1 cycle before the core reacts)
//   orderbook output registers (+1 cycle after a book-changing msg)
//   hft_chip registers a copy of the book (+1 cycle before a trader sees it)
//   private fills reach traders the same cycle the book sees the input
//   output bus registers (+1 cycle after a trader decides)

static const int ARB_THRESHOLD = 2;     // matches rtl/arb_trader.sv default
enum { MSG_PUBLIC = 0, MSG_PRIVATE = 1 };
static const int WARMUP_CYCLES = 20;        // scenario CSVs skip the cold-start blip before quotes settle
static const int FILL_COOLDOWN_CYCLES = 3;  // cycles to wait after sending a fill before sending another

static int g_errors = 0, g_checks = 0;
static long g_cov_valid = 0, g_cov_arb = 0, g_cov_mom = 0, g_cov_ema = 0, g_cov_oberr = 0;
static VerilatedVcdC* g_tfp = nullptr;
static vluint64_t g_time = 0;
static void dump() { if (g_tfp) g_tfp->dump(g_time); g_time++; }

// ---------------- composite model ----------------
struct OutBus { bool valid; uint8_t market; ob_side_t side; price_t price; qty_t qty; bool error; };

struct InMsg { bool valid; uint8_t msg_type; uint8_t market; ob_op_t op; ob_side_t side; price_t price; qty_t qty; };

struct ChipModel {
    orderbook_t   ob[4];
    book_t        copy[4];
    arb_model_t   arb;
    mom_model_t   mom;
    ema_model_t   ema;
    InMsg         in_q;    // registered chip inputs (pad -> core boundary register)
    OutBus        out;     // registered chip output (what the pads present this cycle)
    bool          last_arb_want, last_mom_want, last_ema_want;  // pre-arbitration valids, this cycle
};

static book_t book_of(const orderbook_t* o) {
    book_t b;
    memcpy(b.bid_prices, o->bid_prices, sizeof b.bid_prices);
    memcpy(b.bid_qtys,   o->bid_qtys,   sizeof b.bid_qtys);
    memcpy(b.ask_prices, o->ask_prices, sizeof b.ask_prices);
    memcpy(b.ask_qtys,   o->ask_qtys,   sizeof b.ask_qtys);
    return b;
}

static void model_init(ChipModel* m) {
    for (int k = 0; k < 4; k++) { orderbook_init(&m->ob[k]); m->copy[k] = book_of(&m->ob[k]); }
    arb_init(&m->arb); mom_init(&m->mom); ema_init_model(&m->ema);
    m->in_q = InMsg{ false, 0, 0, Insert, Bid, 0, 0 };
    m->out = OutBus{ false, 0, Bid, 0, 0, false };
    m->last_arb_want = m->last_mom_want = m->last_ema_want = false;
}

// advances the model one clock under input in_new. Inputs are registered at
// the pad boundary, so the core acts on the previous cycle's input (m->in_q),
// while in_new just gets latched for next cycle.
static void model_advance(ChipModel* m, const InMsg& in_new) {
    const InMsg in = m->in_q;   // registered input the core sees this cycle
    bool pub  = in.valid && in.msg_type == MSG_PUBLIC;
    bool priv = in.valid && in.msg_type == MSG_PRIVATE;
    bool vob[4]; for (int k = 0; k < 4; k++) vob[k] = pub && in.market == k;
    bool fill_arb = priv && in.market < 2;
    bool fill_mom = priv && in.market == 2;
    bool fill_ema = priv && in.market == 3;

    // registered copy <= current orderbook output (snapshot BEFORE this cycle's update)
    book_t copy_next[4];
    for (int k = 0; k < 4; k++) copy_next[k] = book_of(&m->ob[k]);

    // orderbook update (registered): visible next cycle
    orderbook_t ob_next[4];
    for (int k = 0; k < 4; k++) {
        ob_next[k] = m->ob[k];
        if (vob[k]) orderbook_update(&ob_next[k], in.op, in.side, in.price, in.qty);
    }

    // traders read the CURRENT registered copies
    trade_out_t oa = arb_step(&m->arb, &m->copy[0], &m->copy[1], fill_arb, in.qty, in.side, ARB_THRESHOLD);
    bool vmom = (m->mom.state == MOM_TRADE);
    bool gmom = vmom && !oa.valid;
    trade_out_t om = mom_step(&m->mom, &m->copy[2], fill_mom, in.qty, in.side, gmom);
    bool vema = (m->ema.state == EMA_TRADE);
    bool gema = vema && !oa.valid && !vmom;
    trade_out_t oe = ema_step(&m->ema, &m->copy[3], fill_ema, in.qty, in.side, gema);

    if (oa.valid) g_cov_arb++; if (om.valid) g_cov_mom++; if (oe.valid) g_cov_ema++;
    m->last_arb_want = oa.valid; m->last_mom_want = om.valid; m->last_ema_want = oe.valid;
    for (int k = 0; k < 4; k++) if (m->ob[k].error) { g_cov_oberr++; break; }

    // output register: fixed priority arb > mom > ema (payload don't-care when !valid)
    OutBus o;
    o.valid = oa.valid || om.valid || oe.valid;
    if      (oa.valid) { o.market = oa.market ? 1 : 0; o.side = oa.side; o.price = oa.price; o.qty = oa.qty; }
    else if (om.valid) { o.market = 2; o.side = om.side; o.price = om.price; o.qty = om.qty; }
    else if (oe.valid) { o.market = 3; o.side = oe.side; o.price = oe.price; o.qty = oe.qty; }
    else               { o.market = 0; o.side = Bid; o.price = 0; o.qty = 0; }
    // orderbook errors are registered (use CURRENT ob), trader errors combinational
    o.error = m->ob[0].error || m->ob[1].error || m->ob[2].error || m->ob[3].error
              || oa.error || om.error || oe.error;

    // commit
    for (int k = 0; k < 4; k++) { m->ob[k] = ob_next[k]; m->copy[k] = copy_next[k]; }
    m->in_q = in_new;   // boundary input register captures the new input
    m->out = o;
}

// ---------------- DUT plumbing ----------------
static void drive(Vhft_chip* dut, const InMsg& in) {
    dut->valid_i        = in.valid;
    dut->message_type_i = in.msg_type;
    dut->market_i       = in.market;
    dut->op_i           = (uint8_t)in.op;
    dut->side_i         = (uint8_t)in.side;
    dut->price_i        = in.price;
    dut->qty_i          = in.qty;
}

static bool compare(Vhft_chip* dut, const OutBus& e, const char* label) {
    g_checks++;
    bool ok = ((bool)dut->valid_o == e.valid) && ((bool)dut->error_o == e.error)
              && ((bool)dut->spare0_o == false);
    if (e.valid)
        ok = ok && ((uint8_t)dut->market_o == e.market)
                && ((int)dut->side_o == (int)e.side)
                && ((uint16_t)dut->price_o == e.price)
                && ((uint8_t)dut->qty_o == e.qty);
    if (!ok) {
        printf("FAIL [%s]\n  DUT: v=%d mkt=%d s=%d p=%u q=%u err=%d sp=%d\n"
               "  EXP: v=%d mkt=%d s=%d p=%u q=%u err=%d\n", label,
               dut->valid_o, dut->market_o, dut->side_o, dut->price_o, dut->qty_o, dut->error_o, dut->spare0_o,
               e.valid, e.market, e.side, e.price, e.qty, e.error);
        g_errors++;
    }
    return ok;
}

static void clk_edge(Vhft_chip* dut) {
    dut->clk_i = 1; dut->eval(); dump();
    dut->clk_i = 0; dut->eval(); dump();
}

static void reset(Vhft_chip* dut, ChipModel* m) {
    InMsg z{ false, 0, 0, Insert, Bid, 0, 0 };
    drive(dut, z);
    dut->rst_ni = 0; dut->clk_i = 0; dut->eval();
    clk_edge(dut); clk_edge(dut);
    dut->rst_ni = 1; dut->eval();
    model_init(m);
}

static bool cycle(Vhft_chip* dut, ChipModel* m, const InMsg& in, const char* label) {
    drive(dut, in);
    dut->eval();
    bool ok = compare(dut, m->out, label);   // DUT registered output == model.out
    if (m->out.valid) g_cov_valid++;
    model_advance(m, in);
    clk_edge(dut);
    return ok;
}

// ---------------- constrained-random stimulus ----------------
// generates a legal public book op for market mk so the orderbook doesn't
// latch a (sticky, chip-global) error; mirrors orderbook.c's legality rules
static InMsg gen_public(ChipModel* m, uint8_t mk, std::mt19937& rng) {
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    InMsg in{ true, MSG_PUBLIC, mk, Insert, rnd(0,1) ? Ask : Bid, 0, 0 };
    // inputs register at the pad boundary, so this op lands one cycle later,
    // after the in-flight input (m->in_q) has already updated the book -
    // validate against that post-update state so we never emit a stale-illegal op
    orderbook_t scratch = m->ob[mk];
    const InMsg& fl = m->in_q;
    if (fl.valid && fl.msg_type == MSG_PUBLIC && fl.market == mk)
        orderbook_update(&scratch, fl.op, fl.side, fl.price, fl.qty);
    const price_t* p = in.side == Bid ? scratch.bid_prices : scratch.ask_prices;
    const qty_t*   q = in.side == Bid ? scratch.bid_qtys   : scratch.ask_qtys;
    int occ[N], nocc = 0;
    for (int i = 0; i < N; i++) if (q[i]) occ[nocc++] = i;

    if (nocc > 0 && rnd(0, 1)) {                 // remove an existing level
        int j = occ[rnd(0, nocc - 1)];
        in.op = Remove; in.price = p[j]; in.qty = (qty_t)rnd(1, q[j]);
    } else {                                     // insert
        // avoid the side's sentinel price (empty levels hold DEFAULT_BID/ASK);
        // inserting at the sentinel collides with empty levels -> spurious error
        in.op = Insert;
        in.price = in.side == Bid ? (price_t)rnd(1, PRICE_MAX)
                                  : (price_t)rnd(0, PRICE_MAX - 1);
        int found = -1;
        for (int i = 0; i < nocc; i++) if (p[occ[i]] == in.price) { found = occ[i]; break; }
        if (found >= 0) {
            int room = QTY_MAX - q[found];
            if (room <= 0) { in.op = Remove; in.qty = (qty_t)rnd(1, q[found]); }
            else           { in.qty = (qty_t)rnd(1, room); }
        } else {
            in.qty = (qty_t)rnd(1, QTY_MAX);
        }
    }
    return in;
}

static InMsg gen_fill(uint8_t market, std::mt19937& rng) {
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    return InMsg{ true, MSG_PRIVATE, market, Insert, rnd(0,1) ? Ask : Bid, 0, (qty_t)rnd(1, QTY_MAX) };
}

static int run_random(Vhft_chip* dut, ChipModel* m, uint32_t seed, int ncyc) {
    printf("=== random: seed=%u ncyc=%d ===\n", seed, ncyc);
    std::mt19937 rng(seed);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    reset(dut, m);

    for (int i = 0; i < ncyc; i++) {
        InMsg in;
        // drain outstanding orders aggressively so a trader's pending count never
        // wraps into a spurious sticky error; only ever fill when pending>0
        int pa = m->arb.pending, pm = m->mom.pending, pe = m->ema.pending;
        int pmax = pa; if (pm > pmax) pmax = pm; if (pe > pmax) pmax = pe;

        if (pmax > 0 && (pmax >= 2 || rnd(0, 4) < 4)) {        // ~80% drain when any pending
            if (pa == pmax)      in = gen_fill(rnd(0, 1), rng);   // arb: market 0 or 1
            else if (pm == pmax) in = gen_fill(2, rng);
            else                 in = gen_fill(3, rng);
        } else if (rnd(0, 19) == 0) {
            in = InMsg{ false, 0, 0, Insert, Bid, 0, 0 };         // 5% idle (hold)
        } else {
            in = gen_public(m, (uint8_t)rnd(0, 3), rng);          // book activity
        }

        char label[40]; snprintf(label, sizeof label, "rand#%d", i);
        if (!cycle(dut, m, in, label)) {
            printf("  (first mismatch at cycle %d)\n", i);
            return 1;
        }
    }
    return 0;
}

// ---------------- shared single-market scenario helpers ----------------
// insert-before-remove quoting FSM: always quote a fresh level before removing
// the old one, so the market's book never goes empty. Shared by the ema and
// momentum scenarios, each of which drives exactly one market.
struct QuoteFSM {
    enum Phase { PH_INSERT_BID, PH_REMOVE_BID, PH_INSERT_ASK, PH_REMOVE_ASK };
    int phase = PH_INSERT_BID;
    price_t bid_price = 0, ask_price = 0, old_bid_price = 0, old_ask_price = 0;
    qty_t   bid_qty = 0,   ask_qty = 0,   old_bid_qty = 0,   old_ask_qty = 0;
};

static InMsg quote_step(QuoteFSM& q, uint8_t market, price_t bid_target, qty_t bid_qty, price_t ask_target, qty_t ask_qty) {
    InMsg in{ false, 0, 0, Insert, Bid, 0, 0 };
    switch (q.phase) {
    case QuoteFSM::PH_INSERT_BID:
        q.old_bid_price = q.bid_price; q.old_bid_qty = q.bid_qty;
        q.bid_price = bid_target; q.bid_qty = bid_qty;
        in = InMsg{ true, MSG_PUBLIC, market, Insert, Bid, q.bid_price, q.bid_qty };
        q.phase = QuoteFSM::PH_REMOVE_BID;
        break;
    case QuoteFSM::PH_REMOVE_BID:
        if (q.old_bid_qty > 0)
            in = InMsg{ true, MSG_PUBLIC, market, Remove, Bid, q.old_bid_price, q.old_bid_qty };
        q.old_bid_qty = 0;
        q.phase = QuoteFSM::PH_INSERT_ASK;
        break;
    case QuoteFSM::PH_INSERT_ASK:
        q.old_ask_price = q.ask_price; q.old_ask_qty = q.ask_qty;
        q.ask_price = ask_target; q.ask_qty = ask_qty;
        in = InMsg{ true, MSG_PUBLIC, market, Insert, Ask, q.ask_price, q.ask_qty };
        q.phase = QuoteFSM::PH_REMOVE_ASK;
        break;
    case QuoteFSM::PH_REMOVE_ASK:
        if (q.old_ask_qty > 0)
            in = InMsg{ true, MSG_PUBLIC, market, Remove, Ask, q.old_ask_price, q.old_ask_qty };
        q.old_ask_qty = 0;
        q.phase = QuoteFSM::PH_INSERT_BID;
        break;
    }
    return in;
}

// drains a resting order by sending a private fill against it, once one is
// outstanding (pending>0) and a cooldown has elapsed. Shared by the ema and
// momentum scenarios, whose models share the same pending/order_qty/order_side
// field layout.
static bool gen_drain_fill(uint8_t market, uint8_t pending, qty_t order_qty, ob_side_t order_side,
                            int& fill_cooldown, std::mt19937& rng, InMsg& out) {
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    if (fill_cooldown > 0) fill_cooldown--;
    if (fill_cooldown != 0 || pending == 0 || rnd(0, 4) >= 4) return false;
    qty_t cap = order_qty > 0 ? order_qty : 1;   // bound to the actual resting order
    out = InMsg{ true, MSG_PRIVATE, market, Insert, order_side, 0, (qty_t)rnd(1, cap) };
    fill_cooldown = FILL_COOLDOWN_CYCLES;
    return true;
}

// QuoteFSM, but for arb: two markets, alternating turns
struct ArbQuoteFSM {
    enum Phase { PH_INSERT_BID, PH_REMOVE_BID, PH_INSERT_ASK, PH_REMOVE_ASK };
    int     phase[2]         = { PH_INSERT_BID, PH_INSERT_BID };
    price_t bid_price[2]     = { 0, 0 }, ask_price[2]     = { 0, 0 };
    price_t old_bid_price[2] = { 0, 0 }, old_ask_price[2] = { 0, 0 };
    qty_t   bid_qty[2]       = { 0, 0 }, ask_qty[2]       = { 0, 0 };
    qty_t   old_bid_qty[2]   = { 0, 0 }, old_ask_qty[2]   = { 0, 0 };
    int turn = 0;
};

static InMsg arb_quote_step(ArbQuoteFSM& q, const double mid[2], int spread, qty_t qty) {
    InMsg in{ false, 0, 0, Insert, Bid, 0, 0 };
    int k = q.turn; q.turn ^= 1;
    switch (q.phase[k]) {
    case ArbQuoteFSM::PH_INSERT_BID: {
        double bp = mid[k] - spread / 2.0;
        if (bp < 1) bp = 1; if (bp > PRICE_MAX - 1) bp = PRICE_MAX - 1;
        q.old_bid_price[k] = q.bid_price[k]; q.old_bid_qty[k] = q.bid_qty[k];
        q.bid_price[k] = (price_t)bp; q.bid_qty[k] = qty;
        in = InMsg{ true, MSG_PUBLIC, (uint8_t)k, Insert, Bid, q.bid_price[k], q.bid_qty[k] };
        q.phase[k] = ArbQuoteFSM::PH_REMOVE_BID;
        break;
    }
    case ArbQuoteFSM::PH_REMOVE_BID:
        if (q.old_bid_qty[k] > 0)
            in = InMsg{ true, MSG_PUBLIC, (uint8_t)k, Remove, Bid, q.old_bid_price[k], q.old_bid_qty[k] };
        q.old_bid_qty[k] = 0;
        q.phase[k] = ArbQuoteFSM::PH_INSERT_ASK;
        break;
    case ArbQuoteFSM::PH_INSERT_ASK: {
        double ap = mid[k] + spread / 2.0;
        if (ap < 1) ap = 1; if (ap > PRICE_MAX - 1) ap = PRICE_MAX - 1;
        q.old_ask_price[k] = q.ask_price[k]; q.old_ask_qty[k] = q.ask_qty[k];
        q.ask_price[k] = (price_t)ap; q.ask_qty[k] = qty;
        in = InMsg{ true, MSG_PUBLIC, (uint8_t)k, Insert, Ask, q.ask_price[k], q.ask_qty[k] };
        q.phase[k] = ArbQuoteFSM::PH_REMOVE_ASK;
        break;
    }
    case ArbQuoteFSM::PH_REMOVE_ASK:
        if (q.old_ask_qty[k] > 0)
            in = InMsg{ true, MSG_PUBLIC, (uint8_t)k, Remove, Ask, q.old_ask_price[k], q.old_ask_qty[k] };
        q.old_ask_qty[k] = 0;
        q.phase[k] = ArbQuoteFSM::PH_INSERT_BID;
        break;
    }
    return in;
}

// FIFO of legs still owed a fill (max 2 outstanding); cycle lets a CSV
// plot anchor the fill at the cycle the order was quoted, not where it lands
struct ArbOwedQueue {
    struct Fill { ob_side_t side; qty_t qty; price_t price; bool full_only; int cycle; };
    Fill q[2]; int head = 0, count = 0;
    void push(ob_side_t side, qty_t qty, price_t price, bool full_only, int placed_cycle) {
        if (count >= 2) return;
        q[(head + count) % 2] = Fill{ side, qty, price, full_only, placed_cycle };
        count++;
    }
    Fill pop() { Fill f = q[head]; head = (head + 1) % 2; count--; return f; }
};

// mirrors sw/arb_trader.c's FLATTEN price/market selection
static void arb_queue_owed_fill(ArbOwedQueue& owed, int state_before, int16_t residual_before,
                                 uint8_t pending_before, price_t ask_price_before, price_t bid_price_before,
                                 qty_t arb_qty_before, const book_t& copy0_before, const book_t& copy1_before, int cycle) {
    if (state_before == ARB_TRADE1) {
        owed.push(Ask, arb_qty_before, ask_price_before, false, cycle);
    } else if (state_before == ARB_TRADE2) {
        owed.push(Bid, arb_qty_before, bid_price_before, false, cycle);
    } else if (state_before == ARB_FLATTEN && pending_before == 0 && residual_before != 0) {
        bool sell = residual_before > 0;
        price_t price0 = sell ? copy0_before.bid_prices[0] : copy0_before.ask_prices[0];
        price_t price1 = sell ? copy1_before.bid_prices[0] : copy1_before.ask_prices[0];
        qty_t   q0     = sell ? copy0_before.bid_qtys[0]   : copy0_before.ask_qtys[0];
        qty_t   q1     = sell ? copy1_before.bid_qtys[0]   : copy1_before.ask_qtys[0];
        bool liquid0 = q0 != 0, liquid1 = q1 != 0;
        bool market;
        if (liquid0 && liquid1) market = sell ? (price1 > price0) : (price1 < price0);
        else if (liquid1)       market = true;
        else                    market = false;
        if (liquid0 || liquid1) {
            qty_t res_qty = (qty_t)(sell ? residual_before : -residual_before);
            owed.push(sell ? Ask : Bid, res_qty, market ? price1 : price0, true, cycle);
        }
    }
}

// directed scenario: ema mean-reversion, drives market 3 toward an OU mid-price
static void scenario_ema(Vhft_chip* dut, ChipModel* m, uint32_t seed, int ncyc, const char* csv_path) {
    printf("=== scenario: ema seed=%u ncyc=%d -> %s ===\n", seed, ncyc, csv_path);
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);
    reset(dut, m);

    FILE* f = fopen(csv_path, "w");
    fprintf(f, "cycle,mid_target,book_mid,fill_side,fill_qty,fill_price,position,mtm_pnl,error,pending,state,fill_cycle\n");

    const double mu    = (double)PRICE_MAX / 2.0;
    const double theta = 0.1;
    const double sigma = 6.0;
    const int    spread = 2;
    const int    qty    = 40;
    double mid_target = mu;
    double cash = 0.0;

    QuoteFSM quote;
    int fill_cooldown = 0;
    int order_placed_cycle = 0;   // cycle the currently-resting order was quoted at

    for (int i = 0; i < ncyc; i++) {
        if (i % 8 == 0) {
            mid_target += theta * (mu - mid_target) + sigma * noise(rng);
            if (mid_target < spread + 2) mid_target = spread + 2;
            if (mid_target > (double)PRICE_MAX - spread - 2) mid_target = PRICE_MAX - spread - 2;
        }

        InMsg in{ false, 0, 0, Insert, Bid, 0, 0 };
        if (!gen_drain_fill(3, m->ema.pending, m->ema.order_qty, m->ema.order_side, fill_cooldown, rng, in)) {
            double bp = mid_target - spread / 2.0, ap = mid_target + spread / 2.0;
            if (bp < 1) bp = 1; if (bp > PRICE_MAX - 1) bp = PRICE_MAX - 1;
            if (ap < 1) ap = 1; if (ap > PRICE_MAX - 1) ap = PRICE_MAX - 1;
            in = quote_step(quote, 3, (price_t)bp, (qty_t)qty, (price_t)ap, (qty_t)qty);
        }

        int16_t pos_before = m->ema.position;
        price_t order_price_before = m->ema.order_price;
        ema_state_e state_before = m->ema.state;

        char label[40]; snprintf(label, sizeof label, "ema_scn#%d", i);
        if (!cycle(dut, m, in, label)) {
            printf("  (mismatch at cycle %d)\n", i);
            fclose(f);
            return;
        }

        // new order just got quoted this cycle -> remember where to anchor its fill(s)
        if (state_before == EMA_IDLE && m->ema.state == EMA_TRADE) order_placed_cycle = i;

        int delta = (int)m->ema.position - (int)pos_before;
        bool filled = delta != 0;
        ob_side_t fside = Bid; qty_t fqty = 0; price_t fprice = 0;
        if (filled) {
            if (delta > 0) { fside = Bid; fqty = (qty_t)delta;  cash -= (double)order_price_before * fqty; }
            else           { fside = Ask; fqty = (qty_t)(-delta); cash += (double)order_price_before * fqty; }
            fprice = order_price_before;
        }

        double book_mid = ((double)m->ob[3].bid_prices[0] + (double)m->ob[3].ask_prices[0]) / 2.0;
        double mtm_pnl = cash + (double)m->ema.position * book_mid;   // mark the open position to the current mid
        if (i >= WARMUP_CYCLES)   // skip the cold-start blip before quotes are established
            fprintf(f, "%d,%.2f,%.2f,%d,%u,%u,%d,%.2f,%d,%d,%d,%d\n", i, mid_target, book_mid,
                    filled ? (int)fside : -1, filled ? fqty : 0, filled ? fprice : 0,
                    (int)m->ema.position, mtm_pnl, (int)m->ema.error, (int)m->ema.pending, (int)m->ema.state,
                    filled ? order_placed_cycle : -1);
    }
    fclose(f);
    printf("scenario final cash=%.2f position=%d\n", cash, (int)m->ema.position);
}

// directed scenario: cross-market arbitrage on markets 0/1 (2/3 idle). Reuses
// scenario_ema's CSV columns: mid_target=market0 mid, book_mid=market1 mid.
static void scenario_arb(Vhft_chip* dut, ChipModel* m, uint32_t seed, int ncyc, const char* csv_path) {
    printf("=== scenario: arb seed=%u ncyc=%d -> %s ===\n", seed, ncyc, csv_path);
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    reset(dut, m);

    FILE* f = fopen(csv_path, "w");
    fprintf(f, "cycle,mid_target,book_mid,fill_side,fill_qty,fill_price,position,mtm_pnl,error,pending,state,fill_cycle\n");

    const double mu     = (double)PRICE_MAX / 2.0;
    const int    spread = 2;
    const int    qty    = 40;
    // shared OU common (both markets' mid) plus a small OU basis (mid1-mid0),
    // tuned to occasionally drift past +/-ARB_THRESHOLD for smooth, small arb windows
    const double theta_c = 0.02, sigma_c = 1.2;
    const double theta_b = 0.05, sigma_b = 0.9;
    double common = mu;
    double basis  = 0.0;
    double cash = 0.0;

    ArbQuoteFSM quote;
    ArbOwedQueue owed;
    int fill_cooldown = 0;

    // a sent message lands one cycle later (registered input), so credit/log the fill
    // we sent on the NEXT iteration, when m->arb.residual actually moves
    bool in_flight_valid = false;
    ArbOwedQueue::Fill in_flight{ Bid, 0, 0, false, 0 };

    for (int i = 0; i < ncyc; i++) {
        common += theta_c * (mu - common) + sigma_c * noise(rng);
        if (common < spread + 4) common = spread + 4;
        if (common > (double)PRICE_MAX - spread - 4) common = PRICE_MAX - spread - 4;
        basis += -theta_b * basis + sigma_b * noise(rng);

        double mid[2] = { common - basis / 2.0, common + basis / 2.0 };
        for (int k = 0; k < 2; k++) {
            if (mid[k] < spread + 2) mid[k] = spread + 2;
            if (mid[k] > (double)PRICE_MAX - spread - 2) mid[k] = PRICE_MAX - spread - 2;
        }

        InMsg in{ false, 0, 0, Insert, Bid, 0, 0 };
        bool sending_fill = false; ob_side_t sent_side = Bid; qty_t sent_qty = 0; price_t sent_price = 0;
        int sent_cycle = 0;

        if (fill_cooldown > 0) fill_cooldown--;
        if (fill_cooldown == 0 && owed.count > 0 && rnd(0, 4) < 4) {
            ArbOwedQueue::Fill o = owed.pop();
            qty_t fqty = o.qty;
            if (!o.full_only && rnd(0, 4) == 0)   // ~20% of non-flatten legs: partial fill
                fqty = o.qty > 1 ? (qty_t)rnd(1, o.qty - 1) : o.qty;
            in = InMsg{ true, MSG_PRIVATE, 0, Insert, o.side, 0, fqty };
            sending_fill = true; sent_side = o.side; sent_qty = fqty; sent_price = o.price; sent_cycle = o.cycle;
            fill_cooldown = FILL_COOLDOWN_CYCLES;
        } else {
            in = arb_quote_step(quote, mid, spread, (qty_t)qty);
        }

        int     state_before    = (int)m->arb.state;
        int16_t residual_before = m->arb.residual;
        uint8_t pending_before  = m->arb.pending;
        price_t ask_price_before = m->arb.ask_price, bid_price_before = m->arb.bid_price;
        qty_t   arb_qty_before   = m->arb.arb_qty;
        book_t  copy0_before = m->copy[0], copy1_before = m->copy[1];

        char label[40]; snprintf(label, sizeof label, "arb_scn#%d", i);
        if (!cycle(dut, m, in, label)) {
            printf("  (mismatch at cycle %d)\n", i);
            fclose(f);
            return;
        }

        arb_queue_owed_fill(owed, state_before, residual_before, pending_before, ask_price_before,
                            bid_price_before, arb_qty_before, copy0_before, copy1_before, i);

        // settle the PREVIOUS iteration's in-flight fill (this iteration's send lands next)
        int delta = (int)m->arb.residual - (int)residual_before;
        bool landed = in_flight_valid && delta != 0;
        ob_side_t landed_side = Bid; qty_t landed_qty = 0; price_t landed_price = 0; int landed_cycle = i;
        if (landed) {
            landed_side = delta > 0 ? Bid : Ask;
            landed_qty  = (qty_t)(delta > 0 ? delta : -delta);
            landed_price = in_flight.price;
            landed_cycle = in_flight.cycle;   // plot the fill at the order's placement cycle, not the fill's landing cycle
            if (landed_side == Bid) cash -= (double)landed_price * landed_qty;   // bought
            else                    cash += (double)landed_price * landed_qty;   // sold
        }
        in_flight_valid = sending_fill;
        if (sending_fill) in_flight = ArbOwedQueue::Fill{ sent_side, sent_qty, sent_price, false, sent_cycle };

        double mid0 = ((double)m->ob[0].bid_prices[0] + (double)m->ob[0].ask_prices[0]) / 2.0;
        double mid1 = ((double)m->ob[1].bid_prices[0] + (double)m->ob[1].ask_prices[0]) / 2.0;
        // mark the still-open leg to the cross-market mid, so PnL doesn't spike on one-sided settlement
        double mtm_pnl = cash + (double)m->arb.residual * (mid0 + mid1) / 2.0;
        if (i >= WARMUP_CYCLES)
            fprintf(f, "%d,%.2f,%.2f,%d,%u,%u,%d,%.2f,%d,%d,%d,%d\n", i, mid0, mid1,
                    landed ? (int)landed_side : -1, landed ? landed_qty : 0,
                    landed ? landed_price : 0,
                    (int)m->arb.residual, mtm_pnl, (int)m->arb.error, (int)m->arb.pending, (int)m->arb.state,
                    landed ? landed_cycle : -1);
    }
    fclose(f);
    printf("scenario final cash=%.2f residual=%d\n", cash, (int)m->arb.residual);
}

// directed scenario: momentum_trader on market 2. flow (OU) drives the bid/ask
// qty skew; permanent accumulates it into mid so imbalance leads price, not just bumps it
static void scenario_momentum(Vhft_chip* dut, ChipModel* m, uint32_t seed, int ncyc, const char* csv_path) {
    printf("=== scenario: momentum seed=%u ncyc=%d -> %s ===\n", seed, ncyc, csv_path);
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);
    reset(dut, m);

    FILE* f = fopen(csv_path, "w");
    fprintf(f, "cycle,mid_target,book_mid,fill_side,fill_qty,fill_price,position,mtm_pnl,error,pending,state,fill_cycle,bid_qty,ask_qty\n");

    const double mu          = (double)PRICE_MAX / 2.0;
    const double theta_flow  = 0.08, sigma_flow = 1.0;     // latent order-flow OU process
    const double theta_perm  = 0.0045, beta = 0.6;         // permanent accumulator, fed by flow
    const double theta_mid   = 0.15, price_sigma = 0.6;
    const int    base_qty    = 24;
    const double skew_k      = 6.0;
    const int    spread      = 2;
    double mid  = mu;
    double flow = 0.0;
    double permanent = 0.0;
    double cash = 0.0;

    QuoteFSM quote;
    int fill_cooldown = 0;
    int order_placed_cycle = 0;   // cycle the currently-resting order was quoted at

    for (int i = 0; i < ncyc; i++) {
        flow += -theta_flow * flow + sigma_flow * noise(rng);
        permanent += -theta_perm * permanent + beta * flow;
        mid += theta_mid * (mu + permanent - mid) + price_sigma * noise(rng);
        if (mid < spread + 2) mid = spread + 2;
        if (mid > (double)PRICE_MAX - spread - 2) mid = PRICE_MAX - spread - 2;

        double raw_bid = base_qty + (flow > 0 ? flow : 0.0) * skew_k;
        double raw_ask = base_qty + (flow < 0 ? -flow : 0.0) * skew_k;
        qty_t skew_bid_qty = (qty_t)(raw_bid > 255.0 ? 255.0 : (raw_bid < 1.0 ? 1.0 : raw_bid));
        qty_t skew_ask_qty = (qty_t)(raw_ask > 255.0 ? 255.0 : (raw_ask < 1.0 ? 1.0 : raw_ask));

        InMsg in{ false, 0, 0, Insert, Bid, 0, 0 };
        if (!gen_drain_fill(2, m->mom.pending, m->mom.order_qty, m->mom.order_side, fill_cooldown, rng, in)) {
            double bp = mid - spread / 2.0, ap = mid + spread / 2.0;
            if (bp < 1) bp = 1; if (bp > PRICE_MAX - 1) bp = PRICE_MAX - 1;
            if (ap < 1) ap = 1; if (ap > PRICE_MAX - 1) ap = PRICE_MAX - 1;
            in = quote_step(quote, 2, (price_t)bp, skew_bid_qty, (price_t)ap, skew_ask_qty);
        }

        int16_t pos_before = m->mom.position;
        price_t order_price_before = m->mom.order_price;
        mom_state_e state_before = m->mom.state;

        char label[40]; snprintf(label, sizeof label, "mom_scn#%d", i);
        if (!cycle(dut, m, in, label)) {
            printf("  (mismatch at cycle %d)\n", i);
            fclose(f);
            return;
        }

        if (state_before == MOM_IDLE && m->mom.state == MOM_TRADE) order_placed_cycle = i;

        int delta = (int)m->mom.position - (int)pos_before;
        bool filled = delta != 0;
        ob_side_t fside = Bid; qty_t fqty = 0; price_t fprice = 0;
        if (filled) {
            if (delta > 0) { fside = Bid; fqty = (qty_t)delta;  cash -= (double)order_price_before * fqty; }
            else           { fside = Ask; fqty = (qty_t)(-delta); cash += (double)order_price_before * fqty; }
            fprice = order_price_before;
        }

        double book_mid = ((double)m->ob[2].bid_prices[0] + (double)m->ob[2].ask_prices[0]) / 2.0;
        double mtm_pnl = cash + (double)m->mom.position * book_mid;
        if (i >= WARMUP_CYCLES)
            fprintf(f, "%d,%.2f,%.2f,%d,%u,%u,%d,%.2f,%d,%d,%d,%d,%u,%u\n", i, mid, book_mid,
                    filled ? (int)fside : -1, filled ? fqty : 0, filled ? fprice : 0,
                    (int)m->mom.position, mtm_pnl, (int)m->mom.error, (int)m->mom.pending, (int)m->mom.state,
                    filled ? order_placed_cycle : -1,
                    m->ob[2].bid_qtys[0], m->ob[2].ask_qtys[0]);
    }
    fclose(f);
    printf("scenario final cash=%.2f position=%d\n", cash, (int)m->mom.position);
}

// directed scenario: all three traders' markets driven concurrently to exercise the arbiter
static void scenario_collision(Vhft_chip* dut, ChipModel* m, uint32_t seed, int ncyc, const char* csv_path) {
    printf("=== scenario: collision seed=%u ncyc=%d -> %s ===\n", seed, ncyc, csv_path);
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    reset(dut, m);

    FILE* f = fopen(csv_path, "w");
    fprintf(f, "cycle,arb_want,mom_want,ema_want,collision,winner_market,priority_ok\n");

    const double mu = (double)PRICE_MAX / 2.0;
    const int spread = 2;

    double ema_mid = mu;
    const double ema_theta = 0.1, ema_sigma = 6.0;
    const int ema_qty = 40;
    QuoteFSM ema_quote;
    int ema_cooldown = 0;

    double mom_flow = 0.0, mom_perm = 0.0, mom_mid = mu;
    const double mom_theta_flow = 0.08, mom_sigma_flow = 1.0;
    const double mom_theta_perm = 0.0045, mom_beta = 0.6;
    const double mom_theta_mid = 0.15, mom_price_sigma = 0.6;
    const int mom_base_qty = 24; const double mom_skew_k = 6.0;
    QuoteFSM mom_quote;
    int mom_cooldown = 0;

    double arb_common = mu, arb_basis = 0.0;
    const double arb_theta_c = 0.02, arb_sigma_c = 1.2;
    const double arb_theta_b = 0.05, arb_sigma_b = 0.9;
    const int arb_qty_const = 40;
    ArbQuoteFSM arb_quote;
    ArbOwedQueue arb_owed;
    int arb_cooldown = 0;

    long collisions = 0;

    for (int i = 0; i < ncyc; i++) {
        ema_mid += ema_theta * (mu - ema_mid) + ema_sigma * noise(rng);
        if (ema_mid < spread + 2) ema_mid = spread + 2;
        if (ema_mid > (double)PRICE_MAX - spread - 2) ema_mid = PRICE_MAX - spread - 2;

        mom_flow += -mom_theta_flow * mom_flow + mom_sigma_flow * noise(rng);
        mom_perm += -mom_theta_perm * mom_perm + mom_beta * mom_flow;
        mom_mid  += mom_theta_mid * (mu + mom_perm - mom_mid) + mom_price_sigma * noise(rng);
        if (mom_mid < spread + 2) mom_mid = spread + 2;
        if (mom_mid > (double)PRICE_MAX - spread - 2) mom_mid = PRICE_MAX - spread - 2;

        arb_common += arb_theta_c * (mu - arb_common) + arb_sigma_c * noise(rng);
        if (arb_common < spread + 4) arb_common = spread + 4;
        if (arb_common > (double)PRICE_MAX - spread - 4) arb_common = PRICE_MAX - spread - 4;
        arb_basis += -arb_theta_b * arb_basis + arb_sigma_b * noise(rng);
        double arb_mid[2] = { arb_common - arb_basis / 2.0, arb_common + arb_basis / 2.0 };
        for (int k = 0; k < 2; k++) {
            if (arb_mid[k] < spread + 2) arb_mid[k] = spread + 2;
            if (arb_mid[k] > (double)PRICE_MAX - spread - 2) arb_mid[k] = PRICE_MAX - spread - 2;
        }

        if (ema_cooldown > 0) ema_cooldown--;
        if (mom_cooldown > 0) mom_cooldown--;
        if (arb_cooldown > 0) arb_cooldown--;

        bool ema_want_fill = ema_cooldown == 0 && m->ema.pending > 0 && rnd(0, 4) < 4;
        bool mom_want_fill = mom_cooldown == 0 && m->mom.pending > 0 && rnd(0, 4) < 4;
        bool arb_want_fill = arb_cooldown == 0 && arb_owed.count > 0 && rnd(0, 4) < 4;

        int sel = rnd(0, 2);   // 0=arb 1=mom 2=ema
        InMsg in{ false, 0, 0, Insert, Bid, 0, 0 };

        if (sel == 0) {
            if (arb_want_fill) {
                ArbOwedQueue::Fill o = arb_owed.pop();
                qty_t fqty = o.qty;
                if (!o.full_only && rnd(0, 4) == 0) fqty = o.qty > 1 ? (qty_t)rnd(1, o.qty - 1) : o.qty;
                in = InMsg{ true, MSG_PRIVATE, 0, Insert, o.side, 0, fqty };
                arb_cooldown = FILL_COOLDOWN_CYCLES;
            } else {
                in = arb_quote_step(arb_quote, arb_mid, spread, (qty_t)arb_qty_const);
            }
        } else if (sel == 1) {
            if (mom_want_fill) {
                qty_t cap = m->mom.order_qty > 0 ? m->mom.order_qty : 1;
                in = InMsg{ true, MSG_PRIVATE, 2, Insert, m->mom.order_side, 0, (qty_t)rnd(1, cap) };
                mom_cooldown = FILL_COOLDOWN_CYCLES;
            } else {
                double raw_bid = mom_base_qty + (mom_flow > 0 ? mom_flow : 0.0) * mom_skew_k;
                double raw_ask = mom_base_qty + (mom_flow < 0 ? -mom_flow : 0.0) * mom_skew_k;
                qty_t sb = (qty_t)(raw_bid > 255.0 ? 255.0 : (raw_bid < 1.0 ? 1.0 : raw_bid));
                qty_t sa = (qty_t)(raw_ask > 255.0 ? 255.0 : (raw_ask < 1.0 ? 1.0 : raw_ask));
                double bp = mom_mid - spread / 2.0, ap = mom_mid + spread / 2.0;
                if (bp < 1) bp = 1; if (bp > PRICE_MAX - 1) bp = PRICE_MAX - 1;
                if (ap < 1) ap = 1; if (ap > PRICE_MAX - 1) ap = PRICE_MAX - 1;
                in = quote_step(mom_quote, 2, (price_t)bp, sb, (price_t)ap, sa);
            }
        } else {
            if (ema_want_fill) {
                qty_t cap = m->ema.order_qty > 0 ? m->ema.order_qty : 1;
                in = InMsg{ true, MSG_PRIVATE, 3, Insert, m->ema.order_side, 0, (qty_t)rnd(1, cap) };
                ema_cooldown = FILL_COOLDOWN_CYCLES;
            } else {
                double bp = ema_mid - spread / 2.0, ap = ema_mid + spread / 2.0;
                if (bp < 1) bp = 1; if (bp > PRICE_MAX - 1) bp = PRICE_MAX - 1;
                if (ap < 1) ap = 1; if (ap > PRICE_MAX - 1) ap = PRICE_MAX - 1;
                in = quote_step(ema_quote, 3, (price_t)bp, (qty_t)ema_qty, (price_t)ap, (qty_t)ema_qty);
            }
        }

        int state_before = (int)m->arb.state;
        int16_t residual_before = m->arb.residual;
        uint8_t pending_before = m->arb.pending;
        price_t ask_price_before = m->arb.ask_price, bid_price_before = m->arb.bid_price;
        qty_t arb_qty_before = m->arb.arb_qty;
        book_t copy0_before = m->copy[0], copy1_before = m->copy[1];

        char label[40]; snprintf(label, sizeof label, "collision_scn#%d", i);
        if (!cycle(dut, m, in, label)) {
            printf("  (mismatch at cycle %d)\n", i);
            fclose(f);
            return;
        }

        arb_queue_owed_fill(arb_owed, state_before, residual_before, pending_before, ask_price_before,
                            bid_price_before, arb_qty_before, copy0_before, copy1_before, i);

        int nwant = (int)m->last_arb_want + (int)m->last_mom_want + (int)m->last_ema_want;
        bool collision = nwant >= 2;
        if (collision) {
            collisions++;
            bool priority_ok = m->last_arb_want
                ? (dut->valid_o && (dut->market_o == 0 || dut->market_o == 1))
                : m->last_mom_want
                    ? (dut->valid_o && dut->market_o == 2)
                    : (dut->valid_o && dut->market_o == 3);
            if (!priority_ok) {
                printf("FAIL [collision@%d] arb_want=%d mom_want=%d ema_want=%d DUT market=%d valid=%d\n",
                       i, m->last_arb_want, m->last_mom_want, m->last_ema_want, dut->market_o, dut->valid_o);
                g_errors++;
            }
            fprintf(f, "%d,%d,%d,%d,1,%d,%d\n", i, (int)m->last_arb_want, (int)m->last_mom_want,
                    (int)m->last_ema_want, dut->market_o, (int)priority_ok);
        } else if (i >= WARMUP_CYCLES) {
            fprintf(f, "%d,%d,%d,%d,0,-1,-1\n", i, (int)m->last_arb_want, (int)m->last_mom_want, (int)m->last_ema_want);
        }
    }
    fclose(f);
    printf("scenario collisions exercised: %ld\n", collisions);
}

using ScenarioFn = void (*)(Vhft_chip*, ChipModel*, uint32_t, int, const char*);

static int run_scenario(Vhft_chip* dut, ChipModel* m, ScenarioFn fn, char** argv) {
    uint32_t seed = (uint32_t)strtoul(argv[3], nullptr, 0);
    int ncyc = atoi(argv[4]);
    fn(dut, m, seed, ncyc, argv[5]);
    printf("\n%d checks, %d failure(s)\n", g_checks, g_errors);
    g_tfp->close(); delete g_tfp; delete dut;
    return g_errors ? 1 : 0;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vhft_chip* dut = new Vhft_chip;
    g_tfp = new VerilatedVcdC;
    dut->trace(g_tfp, 99);
    g_tfp->open("obj_dir/hft_chip.vcd");

    ChipModel m;
    dut->clk_i = 0; dut->eval();

    uint32_t seed = 1; int ncyc = 100000;
    if (argc >= 4 && strcmp(argv[1], "--random") == 0) {
        seed = (uint32_t)strtoul(argv[2], nullptr, 0); ncyc = atoi(argv[3]);
    } else if (argc >= 6 && strcmp(argv[1], "--scenario") == 0 && strcmp(argv[2], "ema") == 0) {
        return run_scenario(dut, &m, scenario_ema, argv);
    } else if (argc >= 6 && strcmp(argv[1], "--scenario") == 0 && strcmp(argv[2], "arb") == 0) {
        return run_scenario(dut, &m, scenario_arb, argv);
    } else if (argc >= 6 && strcmp(argv[1], "--scenario") == 0 && strcmp(argv[2], "momentum") == 0) {
        return run_scenario(dut, &m, scenario_momentum, argv);
    } else if (argc >= 6 && strcmp(argv[1], "--scenario") == 0 && strcmp(argv[2], "collision") == 0) {
        return run_scenario(dut, &m, scenario_collision, argv);
    }

    int rc = run_random(dut, &m, seed, ncyc);

    printf("\ncoverage: out_valid=%ld arb_emit=%ld mom_emit=%ld ema_emit=%ld ob_error_cyc=%ld\n",
           g_cov_valid, g_cov_arb, g_cov_mom, g_cov_ema, g_cov_oberr);
    printf("%d checks, %d failure(s)\n", g_checks, g_errors);
    g_tfp->close(); delete g_tfp; delete dut;
    return (g_errors || rc) ? 1 : 0;
}
