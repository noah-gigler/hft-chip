// EMA trader end-to-end trade latency on Croc's RISC-V core (sw vs hw
// comparison). Warms up a minimal orderbook, then ticks all three traders
// every cycle (matching the chip's fixed pipeline) until ema completes one
// full trade lifecycle and returns to IDLE. Fill/grant sequencing mirrors
// tb/tb_ema_trader.cpp's calling convention.
#include "util.h"
#include "uart.h"
#include "print.h"
#include "orderbook.h"
#include "arb_trader.h"
#include "momentum_trader.h"
#include "ema_trader.h"

#define ARB_THRESHOLD 2

static void book_from_ob(book_t *b, const orderbook_t *ob) {
    for (int i = 0; i < N; i++) {
        b->bid_prices[i] = ob->bid_prices[i];
        b->bid_qtys[i]   = ob->bid_qtys[i];
        b->ask_prices[i] = ob->ask_prices[i];
        b->ask_qtys[i]   = ob->ask_qtys[i];
    }
}

static void book_zero(book_t *b) {
    for (int i = 0; i < N; i++) {
        b->bid_prices[i] = DEFAULT_BID; b->bid_qtys[i] = 0;
        b->ask_prices[i] = DEFAULT_ASK; b->ask_qtys[i] = 0;
    }
}

static orderbook_t ob;
static book_t b1, b2, empty, empty2;
static arb_model_t arb;
static mom_model_t mom;
static ema_model_t ema;

// get_mcycle() only fills the lower 32 bits (RV32's mcycle CSR), so we
// truncate to uint32_t throughout instead of trusting the uint64_t return.
static uint64_t tick(uint32_t label, const book_t *eb, bool ef, qty_t efq, ob_side_t efs, bool eg) {
    uint32_t t0, t1;
    uint64_t total = 0;

    t0 = (uint32_t)get_mcycle();
    trade_out_t oa = arb_step(&arb, &empty, &empty2, false, 0, Bid, ARB_THRESHOLD);
    t1 = (uint32_t)get_mcycle();
    printf("%x arb: %x cyc valid=%x err=%x\n", label, (uint32_t)(t1 - t0), (uint32_t)oa.valid, (uint32_t)oa.error);
    total += t1 - t0;

    t0 = (uint32_t)get_mcycle();
    trade_out_t om = mom_step(&mom, &empty, false, 0, Bid, false);
    t1 = (uint32_t)get_mcycle();
    printf("%x mom: %x cyc valid=%x err=%x\n", label, (uint32_t)(t1 - t0), (uint32_t)om.valid, (uint32_t)om.error);
    total += t1 - t0;

    t0 = (uint32_t)get_mcycle();
    trade_out_t oe = ema_step(&ema, eb, ef, efq, efs, eg);
    t1 = (uint32_t)get_mcycle();
    printf("%x ema: %x cyc valid=%x err=%x\n", label, (uint32_t)(t1 - t0), (uint32_t)oe.valid, (uint32_t)oe.error);
    total += t1 - t0;

    printf("%x tick total: %x cyc\n", label, (uint32_t)total);
    return total;
}

int main() {
    uart_init();
    printf("=== ema end-to-end (steady-state, filter pre-warmed) ===\n");

    orderbook_init(&ob);
    orderbook_update(&ob, Insert, Bid, 99, 5);    // mid ~100, warms up the EMA filter
    orderbook_update(&ob, Insert, Ask, 101, 5);
    book_from_ob(&b1, &ob);

    // shift the (single) bid/ask level to create a deviation from the warmed-up EMA
    orderbook_update(&ob, Remove, Bid, 99, 5);
    orderbook_update(&ob, Remove, Ask, 101, 5);
    orderbook_update(&ob, Insert, Bid, 299, 5);
    orderbook_update(&ob, Insert, Ask, 301, 5);
    book_from_ob(&b2, &ob);

    book_zero(&empty); book_zero(&empty2);

    arb_init(&arb); mom_init(&mom); ema_init_model(&ema);

    // Untimed warm-up: initializes ema_init/ema, which on real hardware
    // would already be warm from continuous prior operation -- this models
    // a trader that has been running, not a cold reset, matching the same
    // steady-state assumption as the momentum/arb tests.
    ema_step(&ema, &b1, false, 0, Bid, false);

    uint64_t total = 0;
    total += tick(1, &b2, false, 0, Bid, false); // price at mid~300 vs warm EMA~100, deviation triggers sell, latches TRADE
    total += tick(2, &b2, false, 0, Bid, true);  // TRADE emits order, granted -> WAIT
    total += tick(3, &b2, true,  5, Ask, false); // fill -> WAIT settles -> IDLE

    printf("ema test total: %x cycles, final state=%x error=%x\n", (uint32_t)total, (uint32_t)ema.state, (uint32_t)ema.error);

    uart_write_flush();
    return 0;
}
