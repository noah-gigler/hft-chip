#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include "util.h"    
#include "uart.h"
#include "print.h"    

void never() {
  uart_write('E');
  while (1) {}
}

typedef enum {
  Bid,
  Ask
} OrderType;
const uint32_t DEFAULT_PRICES[2] = {0, UINT32_MAX};

#define N 8

uint32_t bid_prices[N];
uint32_t bid_qtys[N];
uint32_t ask_prices[N];
uint32_t ask_qtys[N];

void init() {
  for (int i = 0; i < N; ++i) {
    bid_prices[i] = DEFAULT_PRICES[0];
    ask_prices[i] = DEFAULT_PRICES[1];
    bid_qtys[i] = 0;
    ask_qtys[i] = 0;
  }
}

int insert_pos(uint32_t prices[N], uint32_t price, OrderType type) {
  switch (type) {
  case Bid:
    for (int i = 0; i < N; ++i) {
      if (prices[i] <= price) return i;
    }
    return -1;
  case Ask:
    for (int i = 0; i < N; ++i) {
      if (prices[i] >= price) return i;
    }
    return -1;
  }
  return -1;
}

void add(uint32_t price, uint32_t qty, OrderType type) {
  uint32_t* prices = (type == Ask) ? ask_prices : bid_prices;
  uint32_t* qtys = (type == Ask) ? ask_qtys : bid_qtys;

  int pos = insert_pos(prices, price, type);
  if (pos == -1) return;

  // increase qty
  if (prices[pos] == price) {
    qtys[pos] += qty;
    return;
  }

  // shift
  for (int i = N - 1; i > pos; --i) {
    prices[i] = prices[i-1];
    qtys[i] = qtys[i-1];
  }
  prices[pos] = price;
  qtys[pos] = qty;
}

void remove(uint32_t price, uint32_t qty, OrderType type) {
  uint32_t *prices = (type == Ask) ? ask_prices : bid_prices;
  uint32_t *qtys = (type == Ask) ? ask_qtys : bid_qtys;

  int pos = -1;
  for (int i = 0; i < N; ++i) {
    if (prices[i] == price) {
      pos = i;
      break;
    }
  }
  if (pos == -1) never(); // not found

  if (qtys[pos] < qty) never(); // not enough qty left
  qtys[pos] -= qty;
  
  if (qtys[pos] == 0) {
    for (int i = pos; i < N - 1; ++i) {
      prices[i] = prices[i+1];
      qtys[i] = qtys[i+1];
    }
    prices[N-1] = DEFAULT_PRICES[type];
    qtys[N-1] = 0;
  }
}

int main() {
    init();
    add(15000, 100, Bid);
    add(14800, 200, Bid);
    add(15200,  50, Ask);

    // print best bid
    printf("%x\n", bid_prices[0]);
    printf("%x\n", bid_qtys[0]);
    uart_write_flush();
    return 0;
}
