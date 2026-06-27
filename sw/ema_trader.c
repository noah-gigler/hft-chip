#include "ema_trader.h"

static int16_t sext14(int v) {
    unsigned u = (unsigned)v & 0x3FFFu;
    return (int16_t)((u ^ 0x2000u) - 0x2000u);
}
static int16_t sext_pos(int v) {   // signed 10-bit, same as momentum
    unsigned u = (unsigned)v & 0x3FFu;
    return (int16_t)((u ^ 0x200u) - 0x200u);
}

void ema_init_model(ema_model_t *m) {
    m->state = EMA_IDLE;
    m->pending = 0; m->position = 0; m->error = false;
    m->ema = 0; m->ema_init = false;
    m->order_side = Bid; m->order_price = 0; m->order_qty = 0;
}

trade_out_t ema_step(ema_model_t *m, const book_t *b,
                     bool order_filled, qty_t filled_qty, ob_side_t filled_side,
                     bool grant) {
    bool both_liquid = b->bid_qtys[0] != 0 && b->ask_qtys[0] != 0;
    int mid = (b->bid_prices[0] + b->ask_prices[0]) >> 1;   // 10-bit unsigned
    int mid_scaled = mid << EMA_SHIFT;                       // signed 14-bit

    bool error_d = m->error || (order_filled && m->pending == 0)
                             || (order_filled && filled_qty > m->order_qty);
    trade_out_t o = { false, false, Bid, 0, 0, error_d };

    ema_model_t n = *m;
    n.error = error_d;

    // EMA filter updates every cycle when both sides are liquid (FSM-independent).
    // gcc signed >> is arithmetic (floor), matching Verilog >>>; same compiler as DUT.
    int ema_d = m->ema;
    if (both_liquid) {
        if (!m->ema_init) { ema_d = mid_scaled; n.ema_init = true; }
        else              { ema_d = m->ema + ((mid_scaled - m->ema) >> EMA_SHIFT); }
    }
    ema_d = sext14(ema_d);
    n.ema = (int16_t)ema_d;

    uint8_t pend_next = m->pending;
    int     pos_next  = m->position;
    if (!error_d && order_filled) {
        pend_next = (uint8_t)((m->pending - 1) & 0x3);
        pos_next  = sext_pos(filled_side == Bid ? m->position + filled_qty
                                                : m->position - filled_qty);
    }
    n.pending  = pend_next;
    n.position = (int16_t)pos_next;

    if (!error_d) {
        switch (m->state) {
        case EMA_IDLE: {
            int dev = (mid_scaled - ema_d) >> EMA_SHIFT;      // uses the NEW ema_d
            bool buy  = m->ema_init && both_liquid && dev < -EMA_DEV_THRESH && pos_next <  +EMA_MAX_POS;
            bool sell = m->ema_init && both_liquid && dev >  EMA_DEV_THRESH && pos_next >  -EMA_MAX_POS;
            if (buy) {
                n.order_side  = Bid;
                n.order_price = b->ask_prices[0];
                n.order_qty   = min3(EMA_ORDER_QTY, b->ask_qtys[0], EMA_MAX_POS - pos_next);
                n.state = EMA_TRADE;
            } else if (sell) {
                n.order_side  = Ask;
                n.order_price = b->bid_prices[0];
                n.order_qty   = min3(EMA_ORDER_QTY, b->bid_qtys[0], EMA_MAX_POS + pos_next);
                n.state = EMA_TRADE;
            }
            break;
        }
        case EMA_TRADE:
            o.valid = true; o.side = m->order_side;
            o.price = m->order_price; o.qty = m->order_qty;
            if (grant) { n.pending = (uint8_t)((pend_next + 1) & 0x3); n.state = EMA_WAIT; }
            break;

        case EMA_WAIT:
            if (pend_next == 0) n.state = EMA_IDLE;
            break;
        }
    }

    *m = n;
    return o;
}
