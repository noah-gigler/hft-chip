// Momentum trader end-to-end trade latency on Croc's RISC-V core (sw vs hw
// comparison). Warms up a minimal orderbook, then ticks all three traders
// every cycle (matching the chip's fixed pipeline) until momentum completes
// one full trade lifecycle and returns to IDLE. Fill/grant sequencing
// mirrors tb/tb_momentum_trader.cpp's calling convention.
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
static book_t b, empty, empty2;
static arb_model_t arb;
static mom_model_t mom;
static ema_model_t ema;

// get_mcycle() only fills the lower 32 bits (RV32's mcycle CSR), so we
// truncate to uint32_t throughout instead of trusting the uint64_t return.
static uint64_t tick(uint32_t label, bool mf, qty_t mfq, ob_side_t mfs, bool mg) {
    uint32_t t0, t1;
    uint64_t total = 0;

    t0 = (uint32_t)get_mcycle();
    trade_out_t oa = arb_step(&arb, &empty, &empty2, false, 0, Bid, ARB_THRESHOLD);
    t1 = (uint32_t)get_mcycle();
    printf("%x arb: %x cyc valid=%x err=%x\n", label, (uint32_t)(t1 - t0), (uint32_t)oa.valid, (uint32_t)oa.error);
    total += t1 - t0;

    t0 = (uint32_t)get_mcycle();
    trade_out_t om = mom_step(&mom, &b, mf, mfq, mfs, mg);
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
    printf("=== momentum end-to-end ===\n");

    orderbook_init(&ob);
    orderbook_update(&ob, Insert, Bid, 99, 10);   // heavy bid, thin ask -> imbalance buy signal
    orderbook_update(&ob, Insert, Ask, 101, 1);
    book_from_ob(&b, &ob);
    book_zero(&empty); book_zero(&empty2);

    arb_init(&arb); mom_init(&mom); ema_init_model(&ema);

    uint64_t total = 0;
    total += tick(1, false, 0, Bid, false); // IDLE detects buy, latches TRADE
    total += tick(2, false, 0, Bid, true);  // TRADE emits order, granted -> WAIT
    total += tick(3, true,  1, Bid, false); // fill -> WAIT settles -> IDLE

    printf("mom test total: %x cycles, final state=%x error=%x\n", (uint32_t)total, (uint32_t)mom.state, (uint32_t)mom.error);

    uart_write_flush();
    return 0;
}
