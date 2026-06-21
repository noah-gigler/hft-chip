#include "orderbook.h"

void orderbook_init(orderbook_t *ob) {
    for (int i = 0; i < ORDERBOOK_N; i++) {
        ob->bid_prices[i] = DEFAULT_BID;
        ob->bid_qtys[i]   = 0;
        ob->ask_prices[i] = DEFAULT_ASK;
        ob->ask_qtys[i]   = 0;
    }
    ob->error = false;
}

// Returns lowest index where array[i] is true. Mirrors SV first() function.
static int first_set(bool *array, int n) {
    int result = 0;
    for (int i = n - 1; i >= 0; i--)
        if (array[i]) result = i;
    return result;
}

void orderbook_update(orderbook_t *ob, ob_op_t op, ob_side_t side, price_t price, qty_t qty) {
    if (ob->error) return;

    price_t *prices = (side == Bid) ? ob->bid_prices : ob->ask_prices;
    qty_t   *qtys   = (side == Bid) ? ob->bid_qtys   : ob->ask_qtys;

    bool equals[ORDERBOOK_N]   = {0};
    bool compares[ORDERBOOK_N] = {0};
    bool any_eq = false;
    int  eq_count = 0;
    int  eq_idx = 0;

    for (int i = 0; i < ORDERBOOK_N; i++) {
        equals[i]   = (prices[i] == price);
        compares[i] = (side == Bid) ? (price > prices[i]) : (price < prices[i]);
        if (equals[i]) { any_eq = true; eq_idx = i; eq_count++; }
    }

    // qty=0 or duplicate price levels are illegal
    if (qty == 0 || (any_eq && eq_count > 1)) {
        ob->error = true;
        return;
    }

    if (op == Insert) {
        if (any_eq) {
            uint32_t sum = (uint32_t)qtys[eq_idx] + qty;
            if (sum > QTY_MAX) { ob->error = true; return; }
            qtys[eq_idx] = (qty_t)sum;
        } else {
            bool any_cmp = false;
            for (int i = 0; i < ORDERBOOK_N; i++) if (compares[i]) { any_cmp = true; break; }
            if (!any_cmp) return; // price outside top-N — silently drop

            int cmp_idx = first_set(compares, ORDERBOOK_N);

            for (int i = ORDERBOOK_N - 1; i > cmp_idx; i--) {
                prices[i] = prices[i-1];
                qtys[i]   = qtys[i-1];
            }
            prices[cmp_idx] = price;
            qtys[cmp_idx]   = qty;
        }
    } else { // Remove
        if (any_eq && qtys[eq_idx] >= qty) {
            if (qtys[eq_idx] == qty) {
                for (int i = eq_idx; i < ORDERBOOK_N - 1; i++) {
                    prices[i] = prices[i+1];
                    qtys[i]   = qtys[i+1];
                }
                prices[ORDERBOOK_N-1] = (side == Bid) ? DEFAULT_BID : DEFAULT_ASK;
                qtys[ORDERBOOK_N-1]   = 0;
            } else {
                qtys[eq_idx] -= qty;
            }
        } else {
            ob->error = true;
        }
    }
}
