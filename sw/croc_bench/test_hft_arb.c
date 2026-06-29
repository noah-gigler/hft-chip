// Arb trader end-to-end trade latency on Croc's RISC-V core (sw vs hw
// comparison). Warms up a minimal orderbook, then ticks all three traders
// every cycle (matching the chip's fixed pipeline) until arb completes one
// full trade lifecycle and returns to IDLE. Fill/pending sequencing mirrors
// tb/tb_arb_trader.cpp's directed_arb test.
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

static orderbook_t ob0, ob1;
static book_t b0, b1, empty;
static arb_model_t arb;
static mom_model_t mom;
static ema_model_t ema;

// get_mcycle() only fills the lower 32 bits (RV32's mcycle CSR), so we
// truncate to uint32_t throughout instead of trusting the uint64_t return.
static uint64_t tick(uint32_t label, const book_t *ab0, const book_t *ab1,
                      bool af, qty_t afq, ob_side_t afs) {
    uint32_t t0, t1;
    uint64_t total = 0;

    t0 = (uint32_t)get_mcycle();
    trade_out_t oa = arb_step(&arb, ab0, ab1, af, afq, afs, ARB_THRESHOLD);
    t1 = (uint32_t)get_mcycle();
    printf("%x arb: %x cyc valid=%x err=%x\n", label, (uint32_t)(t1 - t0), (uint32_t)oa.valid, (uint32_t)oa.error);
    total += t1 - t0;

    t0 = (uint32_t)get_mcycle();
    trade_out_t om = mom_step(&mom, &empty, false, 0, Bid, false);
    t1 = (uint32_t)get_mcycle();
    printf("%x mom: %x cyc valid=%x err=%x\n", label, (uint32_t)(t1 - t0), (uint32_t)om.valid, (uint32_t)om.error);
    total += t1 - t0;

    t0 = (uint32_t)get_mcycle();
    trade_out_t oe = ema_step(&ema, &empty, false, 0, Bid, false);
    t1 = (uint32_t)get_mcycle();
    printf("%x ema: %x cyc valid=%x err=%x\n", label, (uint32_t)(t1 - t0), (uint32_t)oe.valid, (uint32_t)oe.error);
    total += t1 - t0;

    printf("%x tick total: %x cyc\n", label, (uint32_t)total);
    return total;
}

int main() {
    uart_init();
    printf("=== arb end-to-end ===\n");

    orderbook_init(&ob0); orderbook_init(&ob1);
    orderbook_update(&ob0, Insert, Bid, 300, 10);  // crosses ob1's ask -> triggers arb
    orderbook_update(&ob1, Insert, Ask, 100, 4);
    book_from_ob(&b0, &ob0); book_from_ob(&b1, &ob1);
    book_zero(&empty);

    arb_init(&arb); mom_init(&mom); ema_init_model(&ema);

    uint64_t total = 0;
    total += tick(1, &b0,    &b1,    false, 0, Bid); // IDLE detects, latches TRADE1
    total += tick(2, &empty, &empty, false, 0, Bid); // TRADE1 emits ask leg
    total += tick(3, &empty, &empty, false, 0, Bid); // TRADE2 emits bid leg
    total += tick(4, &empty, &empty, true,  4, Ask); // fill leg1 (ask)
    total += tick(5, &empty, &empty, true,  4, Bid); // fill leg2 (bid)
    total += tick(6, &empty, &empty, false, 0, Bid); // flatten -> idle
    total += tick(7, &empty, &empty, false, 0, Bid); // idle settle

    printf("arb test total: %x cycles, final state=%x error=%x\n", (uint32_t)total, (uint32_t)arb.state, (uint32_t)arb.error);

    uart_write_flush();
    return 0;
}
