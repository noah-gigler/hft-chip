#pragma once
// Cycle-accurate golden model of rtl/arb_trader.sv.
#include "common.h"

typedef enum { ARB_IDLE, ARB_TRADE1, ARB_TRADE2, ARB_FLATTEN } arb_state_e;

typedef struct {
    arb_state_e state;
    price_t ask_price, bid_price;
    qty_t   arb_qty;
    bool    ask_market, bid_market;
    uint8_t pending;    // 2-bit, wraps mod 4
    int16_t residual;   // signed 2*QTY_WIDTH
    bool    error;
} arb_model_t;

// One trade-output bus sample (combinational outputs of the trader).
typedef struct {
    bool      valid;
    bool      market;
    ob_side_t side;
    price_t   price;
    qty_t     qty;
    bool      error;
} trade_out_t;

void arb_init(arb_model_t *m);

// Compute combinational outputs for the CURRENT state/inputs, then advance the
// registered state by one clock. Reads only top-of-book ([0]) of both books.
trade_out_t arb_step(arb_model_t *m,
                     const book_t *b0, const book_t *b1,
                     bool order_filled, qty_t filled_qty, ob_side_t filled_side,
                     int arb_threshold);
