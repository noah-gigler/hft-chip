#include "momentum_trader.h"

// sign-extend to the RTL's signed[$clog2(MAX_POS)+1:0] width.
// MAX_POS=256 -> $clog2=8 -> bits [9:0] -> 10-bit signed.
static int16_t sext_pos(int v) {
    unsigned u = (unsigned)v & 0x3FFu;
    return (int16_t)((u ^ 0x200u) - 0x200u);
}

void mom_init(mom_model_t *m) {
    m->state = MOM_IDLE;
    m->pending = 0; m->position = 0; m->error = false;
    m->order_side = Bid; m->order_price = 0; m->order_qty = 0;
    m->bids_sum = 0; m->asks_sum = 0;
}

trade_out_t mom_step(mom_model_t *m, const book_t *b,
                     bool order_filled, qty_t filled_qty, ob_side_t filled_side,
                     bool grant) {
    bool error_d = m->error || (order_filled && m->pending == 0);
    trade_out_t o = { false, false, Bid, 0, 0, error_d };

    mom_model_t n = *m;
    n.error = error_d;

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
        case MOM_IDLE: {
            int imb = (int)m->bids_sum - (int)m->asks_sum;   // uses registered (prev) sums
            bool buy  = imb >  MOM_IMB_THRESHOLD && pos_next <  +MOM_MAX_POS && b->ask_qtys[0] != 0;
            bool sell = imb < -MOM_IMB_THRESHOLD && pos_next >  -MOM_MAX_POS && b->bid_qtys[0] != 0;
            if (buy) {
                n.order_side  = Bid;
                n.order_price = b->ask_prices[0];
                n.order_qty   = min3(MOM_ORDER_QTY, b->ask_qtys[0], MOM_MAX_POS - pos_next);
                n.state = MOM_TRADE;
            } else if (sell) {
                n.order_side  = Ask;
                n.order_price = b->bid_prices[0];
                n.order_qty   = min3(MOM_ORDER_QTY, b->bid_qtys[0], MOM_MAX_POS + pos_next);
                n.state = MOM_TRADE;
            }
            break;
        }
        case MOM_TRADE:
            o.valid = true; o.side = m->order_side;
            o.price = m->order_price; o.qty = m->order_qty;
            if (grant) { n.pending = (uint8_t)((pend_next + 1) & 0x3); n.state = MOM_WAIT; }
            break;

        case MOM_WAIT:
            if (pend_next == 0) n.state = MOM_IDLE;
            break;
        }
    }

    // registered full-depth sums recomputed from the CURRENT book (available next cycle)
    unsigned bs = 0, as = 0;
    for (int i = 0; i < N; i++) { bs += b->bid_qtys[i]; as += b->ask_qtys[i]; }
    n.bids_sum = (uint16_t)bs;
    n.asks_sum = (uint16_t)as;

    *m = n;
    return o;
}
