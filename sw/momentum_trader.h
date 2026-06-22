#pragma once
// Cycle-accurate golden model of rtl/momentum_trader.sv.
#include "common.h"

#define MOM_IMB_THRESHOLD 2
#define MOM_ORDER_QTY     32
#define MOM_MAX_POS       (8 * MOM_ORDER_QTY)   // 256

typedef enum { MOM_IDLE, MOM_TRADE, MOM_WAIT } mom_state_e;

typedef struct {
    mom_state_e state;
    uint8_t  pending;       // 2-bit
    int16_t  position;      // signed 10-bit ($clog2(MAX_POS)+1 .. 0), emulated wrap
    bool     error;
    ob_side_t order_side;
    price_t  order_price;
    qty_t    order_qty;
} mom_model_t;

void mom_init(mom_model_t *m);

// Combinational outputs for current state/inputs, then advance one clock.
// `b` is the full book (sums use all N levels; signals use top-of-book).
trade_out_t mom_step(mom_model_t *m, const book_t *b,
                     bool order_filled, qty_t filled_qty, ob_side_t filled_side,
                     bool grant);
