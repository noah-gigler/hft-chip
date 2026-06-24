#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include "Vema_trader.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "ema_trader.h"

// ema_trader unit TB. Top-of-book mid feeds the recursive EMA filter; compare
// every cycle against the cycle-accurate golden model.

static int g_errors = 0, g_checks = 0, g_emits = 0;
static VerilatedVcdC* g_tfp = nullptr;
static vluint64_t g_time = 0;
static void dump() { if (g_tfp) g_tfp->dump(g_time); g_time++; }

static void drive_book(Vema_trader* dut, const book_t* b) {
    // top-of-book (level 0)
    dut->bid_price_i = b->bid_prices[0];
    dut->bid_qty_i   = b->bid_qtys[0];
    dut->ask_price_i = b->ask_prices[0];
    dut->ask_qty_i   = b->ask_qtys[0];
}

static bool compare(Vema_trader* dut, const trade_out_t& e, const char* label) {
    g_checks++;
    if (e.valid) g_emits++;
    bool ok = ((bool)dut->valid_o == e.valid) && ((bool)dut->error_o == e.error);
    if (e.valid)
        ok = ok && ((int)dut->side_o == (int)e.side)
                && ((uint16_t)dut->price_o == e.price)
                && ((uint8_t)dut->qty_o == e.qty);
    if (!ok) {
        printf("FAIL [%s]\n  DUT: v=%d s=%d p=%u q=%u err=%d\n"
               "  EXP: v=%d s=%d p=%u q=%u err=%d\n", label,
               dut->valid_o, dut->side_o, dut->price_o, dut->qty_o, dut->error_o,
               e.valid, e.side, e.price, e.qty, e.error);
        g_errors++;
    }
    return ok;
}

static void clk_edge(Vema_trader* dut) {
    dut->clk_i = 1; dut->eval(); dump();
    dut->clk_i = 0; dut->eval(); dump();
}
static void reset(Vema_trader* dut, ema_model_t* m) {
    book_t z; memset(&z, 0, sizeof z);
    drive_book(dut, &z);
    dut->order_filled_i = 0; dut->filled_qty_i = 0; dut->filled_side_i = 0; dut->grant_i = 0;
    dut->rst_ni = 0; dut->clk_i = 0; dut->eval();
    clk_edge(dut); clk_edge(dut);
    dut->rst_ni = 1; dut->eval();
    ema_init_model(m);
}
static bool cycle(Vema_trader* dut, ema_model_t* m, const book_t* b,
                  bool filled, qty_t fqty, ob_side_t fside, bool grant, const char* label) {
    drive_book(dut, b);
    dut->order_filled_i = filled; dut->filled_qty_i = fqty; dut->filled_side_i = (uint8_t)fside;
    dut->grant_i = grant;
    dut->eval();
    trade_out_t e = ema_step(m, b, filled, fqty, fside, grant);
    bool ok = compare(dut, e, label);
    clk_edge(dut);
    return ok;
}

// pure-model anchor: first liquid cycle loads EMA = mid<<SHIFT; stable mid -> no trade.
static void anchor_checks() {
    ema_model_t m; ema_init_model(&m);
    book_t b; memset(&b, 0, sizeof b);
    b.bid_prices[0] = 100; b.bid_qtys[0] = 10;
    b.ask_prices[0] = 110; b.ask_qtys[0] = 10;     // mid=105
    ema_step(&m, &b, 0, 0, Bid, 0);
    g_checks++;
    if (!(m.ema_init && m.ema == (105 << EMA_SHIFT))) {
        printf("FAIL [anchor] ema init load (init=%d ema=%d)\n", m.ema_init, m.ema); g_errors++;
    }
    // hold the same mid: ema stays, dev ~0 -> never trades
    bool traded = false;
    for (int i = 0; i < 20; i++) { trade_out_t e = ema_step(&m, &b, 0, 0, Bid, 0); traded |= e.valid; }
    g_checks++;
    if (traded) { printf("FAIL [anchor] ema traded on flat mid\n"); g_errors++; }
}

static int run_random(Vema_trader* dut, ema_model_t* m, uint32_t seed, int ncyc) {
    printf("=== random: seed=%u ncyc=%d ===\n", seed, ncyc);
    std::mt19937 rng(seed);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    reset(dut, m);

    int base = 256;   // mid random-walks to drive EMA deviations
    for (int i = 0; i < ncyc; i++) {
        book_t b; memset(&b, 0, sizeof b);
        if (rnd(0, 9) < 2) base = rnd(20, PRICE_MAX - 20);          // 20% big jump
        else base += rnd(-8, 8);                                    // else random walk
        if (base < 10) base = 10; if (base > PRICE_MAX - 10) base = PRICE_MAX - 10;
        int spread = rnd(1, 6);
        // mostly both-liquid (exercise the filter); occasionally one side empty
        b.bid_prices[0] = base - spread; b.bid_qtys[0] = rnd(0, 9) ? rnd(1, QTY_MAX) : 0;
        b.ask_prices[0] = base + spread; b.ask_qtys[0] = rnd(0, 9) ? rnd(1, QTY_MAX) : 0;

        bool grant = rnd(0, 9) < 7;
        bool filled = false; qty_t fqty = 0; ob_side_t fside = Bid;
        if (m->pending > 0 && (m->pending >= 2 || rnd(0, 3) != 0)) {
            filled = true; fqty = (qty_t)rnd(1, QTY_MAX); fside = rnd(0, 1) ? Ask : Bid;
        }
        char label[32]; snprintf(label, sizeof label, "rand#%d", i);
        if (!cycle(dut, m, &b, filled, fqty, fside, grant, label)) return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vema_trader* dut = new Vema_trader;
    g_tfp = new VerilatedVcdC;
    dut->trace(g_tfp, 99);
    g_tfp->open("obj_dir/ema_trader.vcd");

    ema_model_t m;
    dut->clk_i = 0; dut->eval();

    uint32_t seed = 1; int ncyc = 100000;
    if (argc >= 4 && strcmp(argv[1], "--random") == 0) {
        seed = (uint32_t)strtoul(argv[2], nullptr, 0); ncyc = atoi(argv[3]);
    }

    printf("=== anchor ===\n");
    anchor_checks();
    int rc = run_random(dut, &m, seed, ncyc);

    printf("\n%d checks, %d failure(s), %d emits\n", g_checks, g_errors, g_emits);
    g_tfp->close(); delete g_tfp; delete dut;
    return (g_errors || rc) ? 1 : 0;
}
