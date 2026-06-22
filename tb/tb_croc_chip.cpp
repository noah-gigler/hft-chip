#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include "Vcroc_chip.h"
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
//   * croc_chip registers a copy of the book  (+1 cycle before a trader sees it)
//   * private fills reach traders in the same cycle the book sees the input
//   * the output bus is registered            (+1 cycle from a trader decision)

static const int ARB_THRESHOLD = 2;     // matches rtl/arb_trader.sv default
enum { MSG_PUBLIC = 0, MSG_PRIVATE = 1 };

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
static void drive(Vcroc_chip* dut, const InMsg& in) {
    dut->valid_i        = in.valid;
    dut->message_type_i = in.msg_type;
    dut->market_i       = in.market;
    dut->op_i           = (uint8_t)in.op;
    dut->side_i         = (uint8_t)in.side;
    dut->price_i        = in.price;
    dut->qty_i          = in.qty;
}

static bool compare(Vcroc_chip* dut, const OutBus& e, const char* label) {
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

static void clk_edge(Vcroc_chip* dut) {
    dut->clk_i = 1; dut->eval(); dump();
    dut->clk_i = 0; dut->eval(); dump();
}

static void reset(Vcroc_chip* dut, ChipModel* m) {
    InMsg z{ false, 0, 0, Insert, Bid, 0, 0 };
    drive(dut, z);
    dut->rst_ni = 0; dut->clk_i = 0; dut->eval();
    clk_edge(dut); clk_edge(dut);
    dut->rst_ni = 1; dut->eval();
    model_init(m);
}

static bool cycle(Vcroc_chip* dut, ChipModel* m, const InMsg& in, const char* label) {
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
    const orderbook_t* o = &m->ob[mk];
    const price_t* p = in.side == Bid ? o->bid_prices : o->ask_prices;
    const qty_t*   q = in.side == Bid ? o->bid_qtys   : o->ask_qtys;
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

static int run_random(Vcroc_chip* dut, ChipModel* m, uint32_t seed, int ncyc) {
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

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vcroc_chip* dut = new Vcroc_chip;
    g_tfp = new VerilatedVcdC;
    dut->trace(g_tfp, 99);
    g_tfp->open("obj_dir/croc_chip.vcd");

    ChipModel m;
    dut->clk_i = 0; dut->eval();

    uint32_t seed = 1; int ncyc = 100000;
    if (argc >= 4 && strcmp(argv[1], "--random") == 0) {
        seed = (uint32_t)strtoul(argv[2], nullptr, 0); ncyc = atoi(argv[3]);
    }

    int rc = run_random(dut, &m, seed, ncyc);

    printf("\ncoverage: out_valid=%ld arb_emit=%ld mom_emit=%ld ema_emit=%ld ob_error_cyc=%ld\n",
           g_cov_valid, g_cov_arb, g_cov_mom, g_cov_ema, g_cov_oberr);
    printf("%d checks, %d failure(s)\n", g_checks, g_errors);
    g_tfp->close(); delete g_tfp; delete dut;
    return (g_errors || rc) ? 1 : 0;
}
