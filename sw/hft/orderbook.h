#pragma once
#include <stdint.h>
#include <stdbool.h>

#define PRICE_WIDTH 9
#define QTY_WIDTH   8
#define ORDERBOOK_N 32

#define PRICE_MAX   ((1u << PRICE_WIDTH) - 1)
#define QTY_MAX     ((1u << QTY_WIDTH)   - 1)
#define DEFAULT_BID 0u
#define DEFAULT_ASK PRICE_MAX

typedef uint16_t price_t;
typedef uint8_t  qty_t;

typedef enum { Insert, Remove } ob_op_t;
typedef enum { Bid, Ask }       ob_side_t;

typedef struct {
    price_t bid_prices[ORDERBOOK_N];
    qty_t   bid_qtys[ORDERBOOK_N];
    price_t ask_prices[ORDERBOOK_N];
    qty_t   ask_qtys[ORDERBOOK_N];
    bool    error;
} orderbook_t;

void orderbook_init(orderbook_t *ob);
void orderbook_update(orderbook_t *ob, ob_op_t op, ob_side_t side, price_t price, qty_t qty);
