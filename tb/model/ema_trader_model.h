#pragma once
// Cycle-accurate golden model of rtl/ema_trader.sv.
#include "common.h"
#include "arb_trader_model.h"   // reuse trade_out_t

#define EMA_SHIFT      4
#define EMA_DEV_THRESH 4
#define EMA_ORDER_QTY  32
#define EMA_MAX_POS    (8 * EMA_ORDER_QTY)   // 256

typedef enum { EMA_IDLE, EMA_TRADE, EMA_WAIT } ema_state_e;

typedef struct {
    ema_state_e state;
    uint8_t  pending;
    int16_t  position;     // signed 10-bit, emulated wrap
    bool     error;
    int16_t  ema;          // signed 14-bit (PRICE_WIDTH+EMA_SHIFT+1)
    bool     ema_init;
    ob_side_t order_side;
    price_t  order_price;
    qty_t    order_qty;
} ema_model_t;

void ema_init_model(ema_model_t *m);

trade_out_t ema_step(ema_model_t *m, const book_t *b,
                     bool order_filled, qty_t filled_qty, ob_side_t filled_side,
                     bool grant);
