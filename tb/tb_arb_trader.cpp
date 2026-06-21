#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>
#include "Varb_trader.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "model/arb_trader_model.h"

// arb_trader unit TB: drive both order-book buses + the private fill feed,
// compare every cycle against the cycle-accurate golden model (arb_trader_model).

static const int ARB_THRESHOLD = 2;   // matches rtl/arb_trader.sv default
static const int PRICE_WORDS = (N * PRICE_WIDTH + 31) / 32;  // 9
static const int QTY_WORDS   = (N * QTY_WIDTH   + 31) / 32;  // 8

static int g_errors = 0, g_checks = 0;
static VerilatedVcdC* g_tfp = nullptr;
static vluint64_t g_time = 0;

static void dump() { if (g_tfp) g_tfp->dump(g_time); g_time++; }

// pack value `val` of `width` bits into packed array at element index i
static void set_field(uint32_t* arr, int i, int width, uint32_t val) {
    int bit = i * width, word = bit / 32, off = bit % 32;
    uint32_t mask = width == 32 ? ~0u : ((1u << width) - 1);
    val &= mask;
    arr[word] = (arr[word] & ~(mask << off)) | (val << off);
    if (off + width > 32)
        arr[word + 1] = (arr[word + 1] & ~(mask >> (32 - off))) | (val >> (32 - off));
}

static void pack_prices(uint32_t* arr, const price_t* p) {
    memset(arr, 0, PRICE_WORDS * 4);
    for (int i = 0; i < N; i++) set_field(arr, i, PRICE_WIDTH, p[i]);
}
static void pack_qtys(uint32_t* arr, const qty_t* q) {
    memset(arr, 0, QTY_WORDS * 4);
    for (int i = 0; i < N; i++) set_field(arr, i, QTY_WIDTH, q[i]);
}

static void drive_books(Varb_trader* dut, const book_t* b0, const book_t* b1) {
    pack_prices(&dut->bid_prices0_i[0], b0->bid_prices);
    pack_qtys  (&dut->bid_qtys0_i[0],   b0->bid_qtys);
    pack_prices(&dut->ask_prices0_i[0], b0->ask_prices);
    pack_qtys  (&dut->ask_qtys0_i[0],   b0->ask_qtys);
    pack_prices(&dut->bid_prices1_i[0], b1->bid_prices);
    pack_qtys  (&dut->bid_qtys1_i[0],   b1->bid_qtys);
    pack_prices(&dut->ask_prices1_i[0], b1->ask_prices);
    pack_qtys  (&dut->ask_qtys1_i[0],   b1->ask_qtys);
}

static bool compare(Varb_trader* dut, const trade_out_t& e, const char* label) {
    g_checks++;
    bool ok = ((bool)dut->valid_o == e.valid) && ((bool)dut->error_o == e.error);
    // output payload only meaningful when a trade is emitted
    if (e.valid) {
        ok = ok && ((bool)dut->market_o == e.market)
                && ((int)dut->side_o == (int)e.side)
                && ((uint16_t)dut->price_o == e.price)
                && ((uint8_t)dut->qty_o == e.qty);
    }
    if (!ok) {
        printf("FAIL [%s]\n  DUT: v=%d m=%d s=%d p=%u q=%u err=%d\n"
               "  EXP: v=%d m=%d s=%d p=%u q=%u err=%d\n", label,
               dut->valid_o, dut->market_o, dut->side_o, dut->price_o, dut->qty_o, dut->error_o,
               e.valid, e.market, e.side, e.price, e.qty, e.error);
        g_errors++;
    }
    return ok;
}

static void clk_edge(Varb_trader* dut) {
    dut->clk_i = 1; dut->eval(); dump();
    dut->clk_i = 0; dut->eval(); dump();
}

static void reset(Varb_trader* dut, arb_model_t* m) {
    book_t z; memset(&z, 0, sizeof z);
    drive_books(dut, &z, &z);
    dut->order_filled_i = 0; dut->filled_qty_i = 0; dut->filled_side_i = 0;
    dut->rst_ni = 0; dut->clk_i = 0; dut->eval();
    clk_edge(dut); clk_edge(dut);
    dut->rst_ni = 1; dut->eval();
    arb_init(m);
}

// Advance one cycle: drive inputs, settle, compare, clock.
static bool cycle(Varb_trader* dut, arb_model_t* m,
                  const book_t* b0, const book_t* b1,
                  bool filled, qty_t fqty, ob_side_t fside, const char* label) {
    drive_books(dut, b0, b1);
    dut->order_filled_i = filled;
    dut->filled_qty_i   = fqty;
    dut->filled_side_i  = (uint8_t)fside;
    dut->eval();
    trade_out_t e = arb_step(m, b0, b1, filled, fqty, fside, ARB_THRESHOLD);
    bool ok = compare(dut, e, label);
    clk_edge(dut);
    return ok;
}

// ---- directed: one clean arbitrage on market0-bid vs market1-ask, full fills ----
static void directed_arb(Varb_trader* dut, arb_model_t* m) {
    reset(dut, m);
    book_t b0, b1; memset(&b0, 0, sizeof b0); memset(&b1, 0, sizeof b1);
    // crossed: bid0=300 (qty10) >> ask1=100 (qty4)+threshold -> arb0 fires, qty=min(10,4)=4
    b0.bid_prices[0] = 300; b0.bid_qtys[0] = 10;
    b1.ask_prices[0] = 100; b1.ask_qtys[0] = 4;
    cycle(dut, m, &b0, &b1, 0, 0, Bid, "arb IDLE->detect");   // IDLE detects, latches TRADE1
    // remove the cross so it doesn't re-trigger while we walk the legs
    memset(&b0, 0, sizeof b0); memset(&b1, 0, sizeof b1);
    cycle(dut, m, &b0, &b1, 0, 0, Bid, "arb TRADE1 (sell mkt0)"); // emits Ask@300 q4
    cycle(dut, m, &b0, &b1, 0, 0, Bid, "arb TRADE2 (buy mkt1)");  // emits Bid@100 q4
    // fill both legs fully: residual nets to 0 -> FLATTEN returns to IDLE
    cycle(dut, m, &b0, &b1, 1, 4, Ask, "fill leg1 (ask)");
    cycle(dut, m, &b0, &b1, 1, 4, Bid, "fill leg2 (bid)");
    cycle(dut, m, &b0, &b1, 0, 0, Bid, "flatten -> idle");
    cycle(dut, m, &b0, &b1, 0, 0, Bid, "idle settle");
}

// ---- directed: illegal fill with no pending order must latch error and hold ----
static void directed_error(Varb_trader* dut, arb_model_t* m) {
    reset(dut, m);
    book_t z; memset(&z, 0, sizeof z);
    cycle(dut, m, &z, &z, 1, 5, Bid, "spurious fill -> error");
    cycle(dut, m, &z, &z, 0, 0, Bid, "error holds");
}

// ---- constrained-random: keep pending bounded (fill outstanding orders) ----
static int run_random(Varb_trader* dut, arb_model_t* m, uint32_t seed, int ncyc) {
    printf("=== random: seed=%u ncyc=%d ===\n", seed, ncyc);
    std::mt19937 rng(seed);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };
    reset(dut, m);

    for (int i = 0; i < ncyc; i++) {
        book_t b0, b1; memset(&b0, 0, sizeof b0); memset(&b1, 0, sizeof b1);
        // 40% of cycles set up a real arbitrage, else random top-of-book
        if (rnd(0, 4) < 2) {
            bool dir = rnd(0, 1);
            book_t* hi = dir ? &b0 : &b1;   // high bid
            book_t* lo = dir ? &b1 : &b0;   // low ask
            hi->bid_prices[0] = rnd(ARB_THRESHOLD + 5, PRICE_MAX); hi->bid_qtys[0] = rnd(1, QTY_MAX);
            lo->ask_prices[0] = rnd(0, hi->bid_prices[0] - ARB_THRESHOLD - 1); lo->ask_qtys[0] = rnd(1, QTY_MAX);
        } else {
            b0.bid_prices[0] = rnd(0, PRICE_MAX); b0.bid_qtys[0] = rnd(0, QTY_MAX);
            b0.ask_prices[0] = rnd(0, PRICE_MAX); b0.ask_qtys[0] = rnd(0, QTY_MAX);
            b1.bid_prices[0] = rnd(0, PRICE_MAX); b1.bid_qtys[0] = rnd(0, QTY_MAX);
            b1.ask_prices[0] = rnd(0, PRICE_MAX); b1.ask_qtys[0] = rnd(0, QTY_MAX);
        }
        // drain outstanding orders so pending never wraps into a spurious error
        bool filled = false; qty_t fqty = 0; ob_side_t fside = Bid;
        if (m->pending > 0 && (m->pending >= 2 || rnd(0, 3) != 0)) {
            filled = true; fqty = (qty_t)rnd(1, QTY_MAX); fside = rnd(0, 1) ? Ask : Bid;
        }
        char label[32]; snprintf(label, sizeof label, "rand#%d", i);
        if (!cycle(dut, m, &b0, &b1, filled, fqty, fside, label)) return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Varb_trader* dut = new Varb_trader;
    g_tfp = new VerilatedVcdC;
    dut->trace(g_tfp, 99);
    g_tfp->open("obj_dir/arb_trader.vcd");

    arb_model_t m;
    dut->clk_i = 0; dut->eval();

    int rc = 0;
    uint32_t seed = 1; int ncyc = 100000;
    if (argc >= 4 && strcmp(argv[1], "--random") == 0) {
        seed = (uint32_t)strtoul(argv[2], nullptr, 0); ncyc = atoi(argv[3]);
    }

    printf("=== directed ===\n");
    directed_arb(dut, &m);
    directed_error(dut, &m);
    rc += run_random(dut, &m, seed, ncyc);

    printf("\n%d checks, %d failure(s)\n", g_checks, g_errors);
    g_tfp->close(); delete g_tfp; delete dut;
    return (g_errors || rc) ? 1 : 0;
}
