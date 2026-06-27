#include "arb_trader.h"

void arb_init(arb_model_t *m) {
    m->state = ARB_IDLE;
    m->ask_price = 0; m->bid_price = 0; m->arb_qty = 0; m->sent_qty = 0;
    m->ask_market = false; m->bid_market = false;
    m->pending = 0; m->residual = 0; m->error = false;
}

trade_out_t arb_step(arb_model_t *m,
                     const book_t *b0, const book_t *b1,
                     bool order_filled, qty_t filled_qty, ob_side_t filled_side,
                     int arb_threshold) {
    // error_d = error_q | (order_filled & pending_q==0)
    bool error_d = m->error || (order_filled && m->pending == 0)
                             || (order_filled && filled_qty > m->sent_qty);

    trade_out_t o = { false, false, Bid, 0, 0, error_d };

    // next-state defaults = current
    arb_model_t n = *m;
    n.error = error_d;

    // pending/residual update (gated by !error_d), computed up front
    uint8_t pend_next = m->pending;
    int16_t res_next  = m->residual;
    if (!error_d && order_filled) {
        pend_next = (uint8_t)((m->pending - 1) & 0x3);
        res_next  = (int16_t)(filled_side == Bid ? m->residual + filled_qty
                                                  : m->residual - filled_qty);
    }
    n.pending  = pend_next;
    n.residual = res_next;

    if (!error_d) {
        switch (m->state) {
        case ARB_IDLE: {
            bool arb0 = b0->bid_qtys[0] > 0 && b1->ask_qtys[0] > 0 &&
                        (b0->bid_prices[0] > b1->ask_prices[0] + arb_threshold);
            bool arb1 = b1->bid_qtys[0] > 0 && b0->ask_qtys[0] > 0 &&
                        (b1->bid_prices[0] > b0->ask_prices[0] + arb_threshold);
            if (arb0) {
                n.ask_market = false; n.bid_market = true;
                n.ask_price = b0->bid_prices[0];
                n.bid_price = b1->ask_prices[0];
                n.arb_qty = b0->bid_qtys[0] < b1->ask_qtys[0] ? b0->bid_qtys[0] : b1->ask_qtys[0];
                n.state = ARB_TRADE1;
            } else if (arb1) {
                n.ask_market = true; n.bid_market = false;
                n.ask_price = b1->bid_prices[0];
                n.bid_price = b0->ask_prices[0];
                n.arb_qty = b1->bid_qtys[0] < b0->ask_qtys[0] ? b1->bid_qtys[0] : b0->ask_qtys[0];
                n.state = ARB_TRADE1;
            }
            break;
        }
        case ARB_TRADE1:
            o.valid = true; o.market = m->ask_market; o.side = Ask;
            o.price = m->ask_price; o.qty = m->arb_qty;
            n.sent_qty = m->arb_qty;
            n.pending = (uint8_t)((pend_next + 1) & 0x3);
            n.state = ARB_TRADE2;
            break;

        case ARB_TRADE2:
            o.valid = true; o.market = m->bid_market; o.side = Bid;
            o.price = m->bid_price; o.qty = m->arb_qty;
            n.sent_qty = m->arb_qty;
            n.pending = (uint8_t)((pend_next + 1) & 0x3);
            n.state = ARB_FLATTEN;
            break;

        case ARB_FLATTEN:
            if (m->pending == 0) {
                if (m->residual == 0) {
                    n.state = ARB_IDLE;
                } else {
                    bool sell = m->residual > 0;
                    qty_t res_qty = (qty_t)(sell ? m->residual : -m->residual);
                    price_t price0 = sell ? b0->bid_prices[0] : b0->ask_prices[0];
                    price_t price1 = sell ? b1->bid_prices[0] : b1->ask_prices[0];
                    qty_t   q0     = sell ? b0->bid_qtys[0]   : b0->ask_qtys[0];
                    qty_t   q1     = sell ? b1->bid_qtys[0]   : b1->ask_qtys[0];
                    bool liquid0 = q0 != 0, liquid1 = q1 != 0;
                    bool market;
                    if (liquid0 && liquid1) market = sell ? (price1 > price0) : (price1 < price0);
                    else if (liquid1)       market = true;
                    else                    market = false;
                    if (liquid0 || liquid1) {
                        o.valid = true; o.market = market; o.side = sell ? Ask : Bid;
                        o.price = market ? price1 : price0; o.qty = res_qty;
                        n.sent_qty = res_qty;
                        n.pending = (uint8_t)((pend_next + 1) & 0x3);
                    }
                }
            }
            break;
        }
    }

    *m = n;
    return o;
}
