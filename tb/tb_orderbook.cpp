#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Vorderbook.h"
#include "verilated.h"
extern "C" {
#include "orderbook.h"
}

static int g_errors = 0;
static int g_checks = 0;

// Extract a PRICE_WIDTH-bit price from a packed Verilator WData array
static uint16_t get_price(const uint32_t* arr, int i) {
    int bit  = i * PRICE_WIDTH;
    int word = bit / 32;
    int off  = bit % 32;
    uint32_t v = arr[word] >> off;
    if (off + PRICE_WIDTH > 32)
        v |= arr[word + 1] << (32 - off);
    return (uint16_t)(v & PRICE_MAX);
}

// Extract a QTY_WIDTH-bit qty from a packed Verilator WData array
static uint8_t get_qty(const uint32_t* arr, int i) {
    int bit  = i * QTY_WIDTH;
    int word = bit / 32;
    int off  = bit % 32;
    return (uint8_t)((arr[word] >> off) & QTY_MAX);
}

static void tick(Vorderbook* dut) {
    dut->clk_i = 0; dut->eval();
    dut->clk_i = 1; dut->eval();
}

static void reset(Vorderbook* dut, orderbook_t* golden) {
    dut->rst_ni  = 0;
    dut->valid_i = 0;
    dut->eval();
    tick(dut); tick(dut);
    dut->rst_ni = 1;
    tick(dut);
    orderbook_init(golden);
}

static void check(Vorderbook* dut, orderbook_t* golden, const char* label) {
    g_checks++;
    bool ok = true;

    if ((bool)dut->error_o != golden->error) {
        printf("FAIL [%s] error: DUT=%d golden=%d\n", label, dut->error_o, golden->error);
        ok = false;
    }

    if (!golden->error) {
        for (int i = 0; i < ORDERBOOK_N; i++) {
            uint16_t dp = get_price(&dut->bid_prices_o[0], i);
            uint8_t  dq = get_qty (&dut->bid_qtys_o[0],   i);
            if (dp != golden->bid_prices[i] || dq != golden->bid_qtys[i]) {
                printf("FAIL [%s] bid[%2d]: DUT=(%3u,%3u) golden=(%3u,%3u)\n",
                       label, i, dp, dq, golden->bid_prices[i], golden->bid_qtys[i]);
                ok = false;
            }

            uint16_t ap = get_price(&dut->ask_prices_o[0], i);
            uint8_t  aq = get_qty (&dut->ask_qtys_o[0],    i);
            if (ap != golden->ask_prices[i] || aq != golden->ask_qtys[i]) {
                printf("FAIL [%s] ask[%2d]: DUT=(%3u,%3u) golden=(%3u,%3u)\n",
                       label, i, ap, aq, golden->ask_prices[i], golden->ask_qtys[i]);
                ok = false;
            }
        }
    }

    if (ok) printf("PASS [%s]\n", label);
    else g_errors++;
}

// Apply one operation to both DUT and golden model, tick, then check
static void apply(Vorderbook* dut, orderbook_t* golden,
                  int valid, ob_op_t op, ob_side_t side, uint16_t price, uint8_t qty,
                  const char* label) {
    dut->valid_i = valid;
    dut->op_i    = (uint8_t)op;
    dut->side_i  = (uint8_t)side;
    dut->price_i = price;
    dut->qty_i   = qty;

    if (valid)
        orderbook_update(golden, op, side, price, qty);

    tick(dut);
    check(dut, golden, label);

    dut->valid_i = 0;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vorderbook* dut = new Vorderbook;
    orderbook_t golden;

    dut->clk_i = 0;
    dut->eval();

    // -------------------------------------------------------
    // Test 1: reset state
    // -------------------------------------------------------
    reset(dut, &golden);
    check(dut, &golden, "reset");

    // -------------------------------------------------------
    // Test 2: basic bid inserts (sorted descending)
    // -------------------------------------------------------
    apply(dut, &golden, 1, Insert, Bid, 100, 10, "insert bid 100/10");
    apply(dut, &golden, 1, Insert, Bid, 120,  5, "insert bid 120/5 (best)");
    apply(dut, &golden, 1, Insert, Bid, 110,  8, "insert bid 110/8 (middle)");

    // -------------------------------------------------------
    // Test 3: qty accumulation
    // -------------------------------------------------------
    apply(dut, &golden, 1, Insert, Bid, 100,  3, "accumulate bid 100 +3");

    // -------------------------------------------------------
    // Test 4: partial and full remove
    // -------------------------------------------------------
    apply(dut, &golden, 1, Remove, Bid, 110,  3, "partial remove bid 110");
    apply(dut, &golden, 1, Remove, Bid, 110,  5, "full remove bid 110");

    // -------------------------------------------------------
    // Test 5: ask side
    // -------------------------------------------------------
    apply(dut, &golden, 1, Insert, Ask, 200, 20, "insert ask 200/20");
    apply(dut, &golden, 1, Insert, Ask, 180, 15, "insert ask 180/15 (best)");
    apply(dut, &golden, 1, Insert, Ask, 190, 10, "insert ask 190/10 (middle)");
    apply(dut, &golden, 1, Remove, Ask, 200, 20, "full remove ask 200");

    // -------------------------------------------------------
    // Test 6: out-of-range insert — fill book, then try worse price (should drop)
    // -------------------------------------------------------
    reset(dut, &golden);
    for (int i = 0; i < ORDERBOOK_N; i++) {
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "fill bid[%d]=%d", i, ORDERBOOK_N - i);
        apply(dut, &golden, 1, Insert, Bid, (uint16_t)(ORDERBOOK_N - i), 1, lbl);
    }
    // book full with bids [32..1]; inserting price 0 should drop silently
    apply(dut, &golden, 1, Insert, Bid, 0, 1, "out-of-range bid drop (no error)");

    // -------------------------------------------------------
    // Test 7: error on remove of nonexistent price
    // -------------------------------------------------------
    reset(dut, &golden);
    apply(dut, &golden, 1, Insert, Ask, 300, 5, "setup ask 300/5");
    apply(dut, &golden, 1, Remove, Ask, 400, 1, "remove nonexistent → error");

    // -------------------------------------------------------
    // Test 8: no-op when valid_i=0
    // -------------------------------------------------------
    reset(dut, &golden);
    apply(dut, &golden, 1, Insert, Bid, 50, 10, "insert bid 50/10");
    apply(dut, &golden, 0, Insert, Bid, 60, 10, "no-op (valid=0)");

    printf("\n%d checks, %d failure(s)\n", g_checks, g_errors);
    delete dut;
    return g_errors ? 1 : 0;
}
