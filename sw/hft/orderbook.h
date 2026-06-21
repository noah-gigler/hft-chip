#pragma once
#include "common.h"

#define ORDERBOOK_N N

typedef struct {
    price_t bid_prices[ORDERBOOK_N];
    qty_t   bid_qtys[ORDERBOOK_N];
    price_t ask_prices[ORDERBOOK_N];
    qty_t   ask_qtys[ORDERBOOK_N];
    bool    error;
} orderbook_t;

void orderbook_init(orderbook_t *ob);
void orderbook_update(orderbook_t *ob, ob_op_t op, ob_side_t side, price_t price, qty_t qty);
