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

// Whole-chip TB. A cycle-accurate composite golden model reproduces the chip
// datapath: 4 orderbooks (orderbook.c) -> 1-cycle registered copies -> 3 traders
// -> fixed-priority arbitration (arb>mom>ema) -> 1-cycle output register. The
// chip only exposes *trades* at its pads, so the model maintains all internal
// state and predicts the trade bus every cycle.
//
// Latencies modelled exactly:
//   * inputs are registered at the pad boundary (+1 cycle before the core acts)
//   * orderbook output is registered          (+1 cycle from a book-changing msg)
//   * hft_chip registers a copy of the book  (+1 cycle before a trader sees it)
//   * private fills reach traders in the same cycle the book sees the input
//   * the output bus is registered            (+1 cycle from a trader decision)

static const int ARB_THRESHOLD = 2;     // matches rtl/arb_trader.sv default
enum { MSG_PUBLIC = 0, MSG_PRIVATE = 1 };
static const int WARMUP_CYCLES = 20;    // scenario CSVs skip the cold-start blip before quotes settle

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
}

// advance the model one clock under input `in_new`. The chip registers its
// inputs at the pad boundary, so the core acts on the PREVIOUS cycle's input
// (m->in_q); `in_new` is registered for next cycle. Updates m->out (registered).
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
// Generate a legal public book op for `mk` so the orderbook doesn't latch a
// (sticky, chip-global) error; mirrors orderbook.c legality.
static InMsg gen_public(ChipModel* m, uint8_t mk, std::mt19937& rng) {
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    InMsg in{ true, MSG_PUBLIC, mk, Insert, rnd(0,1) ? Ask : Bid, 0, 0 };
    // The chip registers inputs at the pad boundary, so this op lands one cycle
    // later, after the in-flight input (m->in_q) has already updated the book.
    // Validate against that post-update state so we never emit a stale-illegal op.
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
        // drain outstanding orders aggressively so trader `pending` never wraps
        // into a spurious (sticky) error; only ever fill when pending>0.
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

// directed scenario: ema mean-reversion, drives market 3 toward an OU mid-price
static void scenario_ema(Vhft_chip* dut, ChipModel* m, uint32_t seed, int ncyc, const char* csv_path) {
    printf("=== scenario: ema seed=%u ncyc=%d -> %s ===\n", seed, ncyc, csv_path);
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
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

    // insert-before-remove FSM: always quote a fresh level before removing the old one,
    // so book[3] never goes empty; track our own quotes since index can shift once both exist
    enum QuotePhase { PH_INSERT_BID, PH_REMOVE_BID, PH_INSERT_ASK, PH_REMOVE_ASK };
    int phase = PH_INSERT_BID;
    price_t bid_price = 0, ask_price = 0, old_bid_price = 0, old_ask_price = 0;
    qty_t   bid_qty = 0,   ask_qty = 0,   old_bid_qty = 0,   old_ask_qty = 0;
    int fill_cooldown = 0;
    int order_placed_cycle = 0;   // cycle the currently-resting order was quoted at

    for (int i = 0; i < ncyc; i++) {
        if (i % 8 == 0) {
            mid_target += theta * (mu - mid_target) + sigma * noise(rng);
            if (mid_target < spread + 2) mid_target = spread + 2;
            if (mid_target > (double)PRICE_MAX - spread - 2) mid_target = PRICE_MAX - spread - 2;
        }

        InMsg in{ false, 0, 0, Insert, Bid, 0, 0 };
        // cool down after each fill: pending lags the registered input by ~2 cycles,
        // so firing on every "pending>0" read can wrap into a spurious sticky error
        if (fill_cooldown > 0) fill_cooldown--;
        bool want_fill = fill_cooldown == 0 && m->ema.pending > 0 && rnd(0, 4) < 4;
        if (want_fill) {
            qty_t cap = m->ema.order_qty > 0 ? m->ema.order_qty : 1;   // bound to the actual resting order
            qty_t fqty = (qty_t)rnd(1, cap);
            ob_side_t fside = m->ema.order_side;   // fill must match the resting order's side
            in = InMsg{ true, MSG_PRIVATE, 3, Insert, fside, 0, fqty };
            fill_cooldown = 3;
        } else {
            switch (phase) {
            case PH_INSERT_BID: {
                double bp = mid_target - spread / 2.0;
                if (bp < 1) bp = 1; if (bp > PRICE_MAX - 1) bp = PRICE_MAX - 1;
                old_bid_price = bid_price; old_bid_qty = bid_qty;   // prior quote, removed next step
                bid_price = (price_t)bp; bid_qty = (qty_t)qty;
                in = InMsg{ true, MSG_PUBLIC, 3, Insert, Bid, bid_price, bid_qty };
                phase = PH_REMOVE_BID;
                break;
            }
            case PH_REMOVE_BID:
                if (old_bid_qty > 0)
                    in = InMsg{ true, MSG_PUBLIC, 3, Remove, Bid, old_bid_price, old_bid_qty };
                old_bid_qty = 0;
                phase = PH_INSERT_ASK;
                break;
            case PH_INSERT_ASK: {
                double ap = mid_target + spread / 2.0;
                if (ap < 1) ap = 1; if (ap > PRICE_MAX - 1) ap = PRICE_MAX - 1;
                old_ask_price = ask_price; old_ask_qty = ask_qty;
                ask_price = (price_t)ap; ask_qty = (qty_t)qty;
                in = InMsg{ true, MSG_PUBLIC, 3, Insert, Ask, ask_price, ask_qty };
                phase = PH_REMOVE_ASK;
                break;
            }
            case PH_REMOVE_ASK:
                if (old_ask_qty > 0)
                    in = InMsg{ true, MSG_PUBLIC, 3, Remove, Ask, old_ask_price, old_ask_qty };
                old_ask_qty = 0;
                phase = PH_INSERT_BID;
                break;
            }
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
    // shared OU `common` (both markets' mid) + small OU `basis` (mid1-mid0); basis
    // is tuned to occasionally drift past +/-ARB_THRESHOLD for smooth, small arb windows
    const double theta_c = 0.02, sigma_c = 1.2;
    const double theta_b = 0.05, sigma_b = 0.9;
    double common = mu;
    double basis  = 0.0;
    double cash = 0.0;

    // two insert-before-remove FSMs, one per market, alternating turns (one pad msg/cycle)
    enum QuotePhase { PH_INSERT_BID, PH_REMOVE_BID, PH_INSERT_ASK, PH_REMOVE_ASK };
    int     phase[2]         = { PH_INSERT_BID, PH_INSERT_BID };
    price_t bid_price[2]     = { 0, 0 }, ask_price[2]     = { 0, 0 };
    price_t old_bid_price[2] = { 0, 0 }, old_ask_price[2] = { 0, 0 };
    qty_t   bid_qty[2]       = { 0, 0 }, ask_qty[2]       = { 0, 0 };
    qty_t   old_bid_qty[2]   = { 0, 0 }, old_ask_qty[2]   = { 0, 0 };
    int turn = 0;   // which market's quote FSM gets to act this cycle

    // owed-fill FIFO (max 2 outstanding legs); `cycle` is the order's placement
    // cycle, kept so its fill marker can be plotted where it was quoted, not where it lands
    struct OwedFill { ob_side_t side; qty_t qty; price_t price; bool full_only; int cycle; };
    OwedFill owed[2]; int owed_head = 0, owed_count = 0;
    auto push_owed = [&](ob_side_t side, qty_t q, price_t price, bool full_only, int placed_cycle) {
        if (owed_count >= 2) return;   // shouldn't happen; see plan notes
        owed[(owed_head + owed_count) % 2] = OwedFill{ side, q, price, full_only, placed_cycle };
        owed_count++;
    };
    int fill_cooldown = 0;

    // a sent message lands one cycle later (registered input), so credit/log the fill
    // we sent on the NEXT iteration, when m->arb.residual actually moves
    bool in_flight_valid = false;
    OwedFill in_flight{ Bid, 0, 0, false, 0 };

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
        if (fill_cooldown == 0 && owed_count > 0 && rnd(0, 4) < 4) {
            OwedFill o = owed[owed_head];
            qty_t fqty = o.qty;
            if (!o.full_only && rnd(0, 4) == 0)   // ~20% of non-flatten legs: partial fill
                fqty = o.qty > 1 ? (qty_t)rnd(1, o.qty - 1) : o.qty;
            in = InMsg{ true, MSG_PRIVATE, 0, Insert, o.side, 0, fqty };
            sending_fill = true; sent_side = o.side; sent_qty = fqty; sent_price = o.price; sent_cycle = o.cycle;
            owed_head = (owed_head + 1) % 2; owed_count--;
            fill_cooldown = 3;
        } else {
            int k = turn; turn ^= 1;
            switch (phase[k]) {
            case PH_INSERT_BID: {
                double bp = mid[k] - spread / 2.0;
                if (bp < 1) bp = 1; if (bp > PRICE_MAX - 1) bp = PRICE_MAX - 1;
                old_bid_price[k] = bid_price[k]; old_bid_qty[k] = bid_qty[k];
                bid_price[k] = (price_t)bp; bid_qty[k] = (qty_t)qty;
                in = InMsg{ true, MSG_PUBLIC, (uint8_t)k, Insert, Bid, bid_price[k], bid_qty[k] };
                phase[k] = PH_REMOVE_BID;
                break;
            }
            case PH_REMOVE_BID:
                if (old_bid_qty[k] > 0)
                    in = InMsg{ true, MSG_PUBLIC, (uint8_t)k, Remove, Bid, old_bid_price[k], old_bid_qty[k] };
                old_bid_qty[k] = 0;
                phase[k] = PH_INSERT_ASK;
                break;
            case PH_INSERT_ASK: {
                double ap = mid[k] + spread / 2.0;
                if (ap < 1) ap = 1; if (ap > PRICE_MAX - 1) ap = PRICE_MAX - 1;
                old_ask_price[k] = ask_price[k]; old_ask_qty[k] = ask_qty[k];
                ask_price[k] = (price_t)ap; ask_qty[k] = (qty_t)qty;
                in = InMsg{ true, MSG_PUBLIC, (uint8_t)k, Insert, Ask, ask_price[k], ask_qty[k] };
                phase[k] = PH_REMOVE_ASK;
                break;
            }
            case PH_REMOVE_ASK:
                if (old_ask_qty[k] > 0)
                    in = InMsg{ true, MSG_PUBLIC, (uint8_t)k, Remove, Ask, old_ask_price[k], old_ask_qty[k] };
                old_ask_qty[k] = 0;
                phase[k] = PH_INSERT_BID;
                break;
            }
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

        // a leg/flatten order just got emitted this cycle -> queue the fill it needs
        if (state_before == ARB_TRADE1)
            push_owed(Ask, arb_qty_before, ask_price_before, false, i);
        else if (state_before == ARB_TRADE2)
            push_owed(Bid, arb_qty_before, bid_price_before, false, i);
        else if (state_before == ARB_FLATTEN && pending_before == 0 && residual_before != 0) {
            // mirrors sw/arb_trader.c's FLATTEN price/market selection
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
                push_owed(sell ? Ask : Bid, res_qty, market ? price1 : price0, true, i);
            }
        }

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
        if (sending_fill) in_flight = OwedFill{ sent_side, sent_qty, sent_price, false, sent_cycle };

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

// directed scenario: momentum_trader on market 2. `flow` (OU) sets the instantaneous
// bid/ask qty skew (the imbalance signal); a slower `permanent` accumulator fed by `flow`
// shifts mid's target, giving imbalance a real, lasting lead over price (transient +
// permanent impact, Kyle's-lambda-style) instead of a bump that mean-reversion erases.
static void scenario_momentum(Vhft_chip* dut, ChipModel* m, uint32_t seed, int ncyc, const char* csv_path) {
    printf("=== scenario: momentum seed=%u ncyc=%d -> %s ===\n", seed, ncyc, csv_path);
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
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

    enum QuotePhase { PH_INSERT_BID, PH_REMOVE_BID, PH_INSERT_ASK, PH_REMOVE_ASK };
    int phase = PH_INSERT_BID;
    price_t bid_price = 0, ask_price = 0, old_bid_price = 0, old_ask_price = 0;
    qty_t   bid_qty = 0,   ask_qty = 0,   old_bid_qty = 0,   old_ask_qty = 0;
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
        if (fill_cooldown > 0) fill_cooldown--;
        bool want_fill = fill_cooldown == 0 && m->mom.pending > 0 && rnd(0, 4) < 4;
        if (want_fill) {
            qty_t cap = m->mom.order_qty > 0 ? m->mom.order_qty : 1;
            qty_t fqty = (qty_t)rnd(1, cap);
            ob_side_t fside = m->mom.order_side;
            in = InMsg{ true, MSG_PRIVATE, 2, Insert, fside, 0, fqty };
            fill_cooldown = 3;
        } else {
            switch (phase) {
            case PH_INSERT_BID: {
                double bp = mid - spread / 2.0;
                if (bp < 1) bp = 1; if (bp > PRICE_MAX - 1) bp = PRICE_MAX - 1;
                old_bid_price = bid_price; old_bid_qty = bid_qty;
                bid_price = (price_t)bp; bid_qty = skew_bid_qty;
                in = InMsg{ true, MSG_PUBLIC, 2, Insert, Bid, bid_price, bid_qty };
                phase = PH_REMOVE_BID;
                break;
            }
            case PH_REMOVE_BID:
                if (old_bid_qty > 0)
                    in = InMsg{ true, MSG_PUBLIC, 2, Remove, Bid, old_bid_price, old_bid_qty };
                old_bid_qty = 0;
                phase = PH_INSERT_ASK;
                break;
            case PH_INSERT_ASK: {
                double ap = mid + spread / 2.0;
                if (ap < 1) ap = 1; if (ap > PRICE_MAX - 1) ap = PRICE_MAX - 1;
                old_ask_price = ask_price; old_ask_qty = ask_qty;
                ask_price = (price_t)ap; ask_qty = skew_ask_qty;
                in = InMsg{ true, MSG_PUBLIC, 2, Insert, Ask, ask_price, ask_qty };
                phase = PH_REMOVE_ASK;
                break;
            }
            case PH_REMOVE_ASK:
                if (old_ask_qty > 0)
                    in = InMsg{ true, MSG_PUBLIC, 2, Remove, Ask, old_ask_price, old_ask_qty };
                old_ask_qty = 0;
                phase = PH_INSERT_BID;
                break;
            }
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
        seed = (uint32_t)strtoul(argv[3], nullptr, 0); ncyc = atoi(argv[4]);
        scenario_ema(dut, &m, seed, ncyc, argv[5]);
        printf("\n%d checks, %d failure(s)\n", g_checks, g_errors);
        g_tfp->close(); delete g_tfp; delete dut;
        return g_errors ? 1 : 0;
    } else if (argc >= 6 && strcmp(argv[1], "--scenario") == 0 && strcmp(argv[2], "arb") == 0) {
        seed = (uint32_t)strtoul(argv[3], nullptr, 0); ncyc = atoi(argv[4]);
        scenario_arb(dut, &m, seed, ncyc, argv[5]);
        printf("\n%d checks, %d failure(s)\n", g_checks, g_errors);
        g_tfp->close(); delete g_tfp; delete dut;
        return g_errors ? 1 : 0;
    } else if (argc >= 6 && strcmp(argv[1], "--scenario") == 0 && strcmp(argv[2], "momentum") == 0) {
        seed = (uint32_t)strtoul(argv[3], nullptr, 0); ncyc = atoi(argv[4]);
        scenario_momentum(dut, &m, seed, ncyc, argv[5]);
        printf("\n%d checks, %d failure(s)\n", g_checks, g_errors);
        g_tfp->close(); delete g_tfp; delete dut;
        return g_errors ? 1 : 0;
    }

    int rc = run_random(dut, &m, seed, ncyc);

    printf("\ncoverage: out_valid=%ld arb_emit=%ld mom_emit=%ld ema_emit=%ld ob_error_cyc=%ld\n",
           g_cov_valid, g_cov_arb, g_cov_mom, g_cov_ema, g_cov_oberr);
    printf("%d checks, %d failure(s)\n", g_checks, g_errors);
    g_tfp->close(); delete g_tfp; delete dut;
    return (g_errors || rc) ? 1 : 0;
}
