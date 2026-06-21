#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <random>
#include "Vorderbook.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "orderbook.h"

// -----------------------------------------------------------------------------
// Verification strategy
//   * Stimulus is external (.vec text files) or randomly generated.
//   * The golden C model (sw/hft/orderbook.c) is the oracle: after every op we
//     compare the full DUT state against it.
//   * A couple of in-code anchor checks validate the golden model itself, so a
//     bug shared by RTL and model can't hide.
//   * On any mismatch the offending random sequence is dumped to a replayable
//     .vec file, and a VCD of the whole run is always written.
// -----------------------------------------------------------------------------

static int g_errors = 0;
static int g_checks = 0;

static VerilatedVcdC* g_tfp = nullptr;
static vluint64_t     g_time = 0;

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

static void dump() {
    if (g_tfp) g_tfp->dump(g_time);
    g_time++;
}

static void tick(Vorderbook* dut) {
    dut->clk_i = 0; dut->eval(); dump();
    dut->clk_i = 1; dut->eval(); dump();
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

static bool check(Vorderbook* dut, orderbook_t* golden, const char* label) {
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

    if (!ok) g_errors++;
    return ok;
}

// One step: drive inputs, advance golden, tick, compare. valid==0 is a hold.
static bool step(Vorderbook* dut, orderbook_t* golden,
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
    bool ok = check(dut, golden, label);

    dut->valid_i = 0;
    return ok;
}

// -----------------------------------------------------------------------------
// .vec stimulus format (one op per line, '#' comments, blank lines ignored):
//   reset
//   nop
//   insert bid <price> <qty>
//   remove ask <price> <qty>
// -----------------------------------------------------------------------------

struct Op { int valid; ob_op_t op; ob_side_t side; uint16_t price; uint8_t qty; };

static const char* op_name(const Op& o) {
    if (!o.valid) return "nop";
    return o.op == Insert ? "insert" : "remove";
}

static void write_vec(const char* path, const std::vector<Op>& seq) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# auto-dumped failing sequence — replay with: Vorderbook --vectors %s\n", path);
    fprintf(f, "reset\n");
    for (const Op& o : seq) {
        if (!o.valid) { fprintf(f, "nop\n"); continue; }
        fprintf(f, "%s %s %u %u\n", op_name(o),
                o.side == Bid ? "bid" : "ask", o.price, o.qty);
    }
    fclose(f);
}

static int run_vec_file(Vorderbook* dut, orderbook_t* golden, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { printf("ERROR: cannot open vector file %s\n", path); return 1; }
    printf("=== vectors: %s ===\n", path);

    char line[256];
    int lineno = 0, fails = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        // strip comment + trailing whitespace
        char* hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char tok[4][32];
        int n = sscanf(line, "%31s %31s %31s %31s", tok[0], tok[1], tok[2], tok[3]);
        if (n <= 0) continue;

        char label[320];
        snprintf(label, sizeof(label), "%s:%d %s", path, lineno, line);
        // trim newline inside label
        for (char* p = label; *p; p++) if (*p == '\n') { *p = '\0'; break; }

        if (strcmp(tok[0], "reset") == 0) {
            reset(dut, golden);
            if (!check(dut, golden, label)) fails++;
        } else if (strcmp(tok[0], "nop") == 0) {
            if (!step(dut, golden, 0, Insert, Bid, 0, 0, label)) fails++;
        } else if (strcmp(tok[0], "insert") == 0 || strcmp(tok[0], "remove") == 0) {
            if (n < 4) { printf("ERROR %s:%d: expected '<op> <side> <price> <qty>'\n", path, lineno); fails++; continue; }
            ob_op_t   op   = strcmp(tok[0], "insert") == 0 ? Insert : Remove;
            ob_side_t side = strcmp(tok[1], "bid") == 0 ? Bid : Ask;
            uint16_t price = (uint16_t)atoi(tok[2]);
            uint8_t  qty   = (uint8_t)atoi(tok[3]);
            if (!step(dut, golden, 1, op, side, price, qty, label)) fails++;
        } else {
            printf("ERROR %s:%d: unknown op '%s'\n", path, lineno, tok[0]);
            fails++;
        }
    }
    fclose(f);
    return fails;
}

// -----------------------------------------------------------------------------
// Anchor checks: validate the golden model independently of the RTL, so that a
// bug present in BOTH can't pass silently. Hand-computed expected states.
// -----------------------------------------------------------------------------
static void anchor_checks() {
    orderbook_t ob; orderbook_init(&ob);
    // Insert three bids out of order; best (highest) must end up at index 0.
    orderbook_update(&ob, Insert, Bid, 100, 10);
    orderbook_update(&ob, Insert, Bid, 120, 5);
    orderbook_update(&ob, Insert, Bid, 110, 8);
    g_checks++;
    if (!(ob.bid_prices[0] == 120 && ob.bid_qtys[0] == 5 &&
          ob.bid_prices[1] == 110 && ob.bid_qtys[1] == 8 &&
          ob.bid_prices[2] == 100 && ob.bid_qtys[2] == 10 && !ob.error)) {
        printf("FAIL [anchor] golden bid sort wrong\n"); g_errors++;
    }
    // qty overflow on accumulate must latch error.
    orderbook_init(&ob);
    orderbook_update(&ob, Insert, Ask, 200, 200);
    orderbook_update(&ob, Insert, Ask, 200, 200); // 400 > QTY_MAX(255)
    g_checks++;
    if (!ob.error) { printf("FAIL [anchor] golden overflow should error\n"); g_errors++; }
}

// -----------------------------------------------------------------------------
// Constrained-random: keep the book churning (qty>=1, removes biased to existing
// levels). On golden error, reset and continue. On any mismatch, dump the
// sequence for replay and stop.
// -----------------------------------------------------------------------------
static int run_random(Vorderbook* dut, orderbook_t* golden, uint32_t seed, int nops) {
    printf("=== random: seed=%u nops=%d ===\n", seed, nops);
    std::mt19937 rng(seed);
    auto rnd = [&](int lo, int hi) { return (int)(rng() % (hi - lo + 1)) + lo; };

    reset(dut, golden);
    std::vector<Op> seq;
    seq.reserve(nops);

    for (int i = 0; i < nops; i++) {
        ob_side_t side = rnd(0, 1) ? Ask : Bid;
        price_t* gp = (side == Bid) ? golden->bid_prices : golden->ask_prices;
        qty_t*   gq = (side == Bid) ? golden->bid_qtys   : golden->ask_qtys;

        // collect occupied levels for this side
        int occ[ORDERBOOK_N], nocc = 0;
        for (int k = 0; k < ORDERBOOK_N; k++) if (gq[k] != 0) occ[nocc++] = k;

        ob_op_t op = (nocc > 0 && rnd(0, 1)) ? Remove : Insert;
        uint16_t price; uint8_t qty;

        if (op == Remove && nocc > 0 && rnd(0, 3) != 0) {
            // 75% of removes target an existing level (valid remove)
            int k = occ[rnd(0, nocc - 1)];
            price = gp[k];
            qty   = (uint8_t)rnd(1, gq[k]); // partial or full
        } else {
            price = (uint16_t)rnd(0, PRICE_MAX);
            qty   = (uint8_t)rnd(1, QTY_MAX); // never 0 → avoid trivial error spam
        }

        Op o{1, op, side, price, qty};
        seq.push_back(o);

        char label[64];
        snprintf(label, sizeof(label), "rand#%d", i);
        if (!step(dut, golden, 1, op, side, price, qty, label)) {
            char path[128];
            snprintf(path, sizeof(path), "vectors/fail_%u.vec", seed);
            write_vec(path, seq);
            printf("  -> dumped failing sequence to %s (%zu ops)\n", path, seq.size());
            return 1;
        }

        // keep exploring: a latched error freezes the book, so recover
        if (golden->error) { reset(dut, golden); seq.clear(); }
    }
    return 0;
}

static void usage(const char* prog) {
    printf("usage:\n");
    printf("  %s --vectors <file.vec> [more.vec ...]\n", prog);
    printf("  %s --random <seed> <nops>\n", prog);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    Vorderbook* dut = new Vorderbook;
    g_tfp = new VerilatedVcdC;
    dut->trace(g_tfp, 99);
    g_tfp->open("obj_dir/orderbook.vcd");

    orderbook_t golden;
    dut->clk_i = 0;
    dut->eval();

    anchor_checks();

    int rc = 0;
    if (argc >= 2 && strcmp(argv[1], "--vectors") == 0) {
        for (int i = 2; i < argc; i++)
            rc += run_vec_file(dut, &golden, argv[i]);
    } else if (argc >= 4 && strcmp(argv[1], "--random") == 0) {
        rc += run_random(dut, &golden, (uint32_t)strtoul(argv[2], nullptr, 0), atoi(argv[3]));
    } else {
        usage(argv[0]);
        rc = 2;
    }

    printf("\n%d checks, %d failure(s)\n", g_checks, g_errors);

    g_tfp->close();
    delete g_tfp;
    delete dut;
    return (g_errors || rc) ? 1 : 0;
}
