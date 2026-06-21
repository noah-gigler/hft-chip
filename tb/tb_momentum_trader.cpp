#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include "Vmomentum_trader.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "model/momentum_trader_model.h"

// momentum_trader unit TB. Full-depth book drives the registered qty sums;
// compare every cycle against the cycle-accurate golden model.

static const int PRICE_WORDS = (N * PRICE_WIDTH + 31) / 32;
static const int QTY_WORDS   = (N * QTY_WIDTH   + 31) / 32;

static int g_errors = 0, g_checks = 0;
static VerilatedVcdC* g_tfp = nullptr;
static vluint64_t g_time = 0;
static void dump() { if (g_tfp) g_tfp->dump(g_time); g_time++; }

static void set_field(uint32_t* arr, int i, int width, uint32_t val) {
    int bit = i * width, word = bit / 32, off = bit % 32;
    uint32_t mask = width == 32 ? ~0u : ((1u << width) - 1);
    val &= mask;
    arr[word] = (arr[word] & ~(mask << off)) | (val << off);
    if (off + width > 32)
        arr[word + 1] = (arr[word + 1] & ~(mask >> (32 - off))) | (val >> (32 - off));
}
static void pack_prices(uint32_t* a, const price_t* p) {
    memset(a, 0, PRICE_WORDS * 4); for (int i = 0; i < N; i++) set_field(a, i, PRICE_WIDTH, p[i]);
}
static void pack_qtys(uint32_t* a, const qty_t* q) {
    memset(a, 0, QTY_WORDS * 4); for (int i = 0; i < N; i++) set_field(a, i, QTY_WIDTH, q[i]);
}
static void drive_book(Vmomentum_trader* dut, const book_t* b) {
    pack_prices(&dut->bid_prices_i[0], b->bid_prices);
    pack_qtys  (&dut->bid_qtys_i[0],   b->bid_qtys);
    pack_prices(&dut->ask_prices_i[0], b->ask_prices);
    pack_qtys  (&dut->ask_qtys_i[0],   b->ask_qtys);
}

static bool compare(Vmomentum_trader* dut, const trade_out_t& e, const char* label) {
    g_checks++;
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

static void clk_edge(Vmomentum_trader* dut) {
    dut->clk_i = 1; dut->eval(); dump();
    dut->clk_i = 0; dut->eval(); dump();
}
static void reset(Vmomentum_trader* dut, mom_model_t* m) {
    book_t z; memset(&z, 0, sizeof z);
    drive_book(dut, &z);
    dut->order_filled_i = 0; dut->filled_qty_i = 0; dut->filled_side_i = 0; dut->grant_i = 0;
    dut->rst_ni = 0; dut->clk_i = 0; dut->eval();
    clk_edge(dut); clk_edge(dut);
    dut->rst_ni = 1; dut->eval();
    mom_init(m);
}
static bool cycle(Vmomentum_trader* dut, mom_model_t* m, const book_t* b,
                  bool filled, qty_t fqty, ob_side_t fside, bool grant, const char* label) {
    drive_book(dut, b);
    dut->order_filled_i = filled; dut->filled_qty_i = fqty; dut->filled_side_i = (uint8_t)fside;
    dut->grant_i = grant;
    dut->eval();
    trade_out_t e = mom_step(m, b, filled, fqty, fside, grant);
    bool ok = compare(dut, e, label);
    clk_edge(dut);
    return ok;
}

// pure-model anchor: validate the golden model independently of the DUT
static void anchor_checks() {
    mom_model_t m; mom_init(&m);
    book_t b; memset(&b, 0, sizeof b);
    // heavy asks, light bids -> imbalance negative -> sell signal (after sums register)
    for (int i = 0; i < 4; i++) { b.ask_qtys[i] = 50; b.ask_prices[i] = 200 - i; }
    b.bid_qtys[0] = 5; b.bid_prices[0] = 150;
    mom_step(&m, &b, 0, 0, Bid, 0);   // cycle0: sums latch (200 ask, 5 bid)
    g_checks++;
    if (!(m.asks_sum == 200 && m.bids_sum == 5)) { printf("FAIL [anchor] mom sums\n"); g_errors++; }
    trade_out_t e = mom_step(&m, &b, 0, 0, Bid, 0); // cycle1: IDLE sees imb=-195 -> sell -> TRADE next
    g_checks++;
    if (!(m.state == MOM_TRADE && m.order_side == Ask && m.order_price == 150)) {
        printf("FAIL [anchor] mom sell setup (state=%d side=%d price=%u)\n",
               m.state, m.order_side, m.order_price); g_errors++;
    }
    (void)e;
}

static int run_random(Vmomentum_trader* dut, mom_model_t* m, uint32_t seed, int ncyc) {
    printf("=== random: seed=%u ncyc=%d ===\n", seed, ncyc);
    std::mt19937 rng(seed);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    reset(dut, m);

    book_t b; memset(&b, 0, sizeof b);
    for (int i = 0; i < ncyc; i++) {
        // occasionally reshape the whole book; otherwise keep it (stable signals)
        if (rnd(0, 3) == 0) {
            int depth = rnd(1, N);
            memset(&b, 0, sizeof b);
            for (int k = 0; k < depth; k++) {
                b.bid_qtys[k] = rnd(0, QTY_MAX); b.bid_prices[k] = rnd(0, PRICE_MAX);
                b.ask_qtys[k] = rnd(0, QTY_MAX); b.ask_prices[k] = rnd(0, PRICE_MAX);
            }
        }
        bool grant = rnd(0, 9) < 7;   // 70%
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
    Vmomentum_trader* dut = new Vmomentum_trader;
    g_tfp = new VerilatedVcdC;
    dut->trace(g_tfp, 99);
    g_tfp->open("obj_dir/momentum_trader.vcd");

    mom_model_t m;
    dut->clk_i = 0; dut->eval();

    uint32_t seed = 1; int ncyc = 100000;
    if (argc >= 4 && strcmp(argv[1], "--random") == 0) {
        seed = (uint32_t)strtoul(argv[2], nullptr, 0); ncyc = atoi(argv[3]);
    }

    printf("=== anchor ===\n");
    anchor_checks();
    int rc = run_random(dut, &m, seed, ncyc);

    printf("\n%d checks, %d failure(s)\n", g_checks, g_errors);
    g_tfp->close(); delete g_tfp; delete dut;
    return (g_errors || rc) ? 1 : 0;
}
