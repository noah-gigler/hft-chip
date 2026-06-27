#pragma once
// Single source of truth for the C golden models (orderbook + traders).
// Mirrors rtl/orderbook_pkg.sv.
#include <stdint.h>
#include <stdbool.h>

#define PRICE_WIDTH 9
#define QTY_WIDTH   8
#define N           64      // price levels per side; matches hft_chip localparam N

#define PRICE_MAX   ((1u << PRICE_WIDTH) - 1)
#define QTY_MAX     ((1u << QTY_WIDTH)   - 1)
#define DEFAULT_BID 0u
#define DEFAULT_ASK PRICE_MAX

typedef uint16_t price_t;   // holds PRICE_WIDTH bits
typedef uint8_t  qty_t;     // holds QTY_WIDTH bits

typedef enum { Insert, Remove } ob_op_t;
typedef enum { Bid, Ask }       ob_side_t;

// A full book view as seen by a trader (sorted: index 0 = top of book).
typedef struct {
    price_t bid_prices[N];
    qty_t   bid_qtys[N];
    price_t ask_prices[N];
    qty_t   ask_qtys[N];
} book_t;

// One trade-output bus sample (combinational outputs of a trader). `market` is
// only meaningful for arb_trader.
typedef struct {
    bool      valid;
    bool      market;
    ob_side_t side;
    price_t   price;
    qty_t     qty;
    bool      error;
} trade_out_t;

// min of three ints, truncated to qty_t — mirrors orderbook_pkg::min3.
static inline qty_t min3(int a, int b, int c) {
    int m = a < b ? a : b;
    m = m < c ? m : c;
    return (qty_t)m;
}
