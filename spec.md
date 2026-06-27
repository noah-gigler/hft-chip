# Spec

## 1. Parameters

| Name | Value | Defined in |
|---|---|---|
| `PRICE_WIDTH` | 9 | `orderbook_pkg` |
| `QTY_WIDTH` | 8 | `orderbook_pkg` |
| `DEFAULT_BID` | 0 | `orderbook_pkg` |
| `DEFAULT_ASK` | 511 | `orderbook_pkg` |
| `N` (levels/side/book) | 64 | `hft_chip` |
| `OB_UNSORTED` | 1 (default) | `hft_chip` / `hft_core` |
| `ARB_THRESHOLD` | 2 | `arb_trader` |
| `IMB_THRESHOLD` | 2 | `momentum_trader` |
| `ORDER_QTY` | 32 | `momentum_trader`, `ema_trader` |
| `MAX_POS` | 256 (`8*ORDER_QTY`) | `momentum_trader`, `ema_trader` |
| `EMA_SHIFT` | 4 | `ema_trader` |
| `DEV_THRESHOLD` | 4 | `ema_trader` |

taped-out RTL uses `N=64`. Testbenches set `N` per target.

## 2. Top-level I/O

### Inputs

| Signal | Width | Values |
|---|---|---|
| `valid_i` | 1 | strobe |
| `message_type_i` | 1 | 0=Public, 1=Private |
| `market_i` | 2 | 0..3 |
| `op_i` | 1 | 0=Insert, 1=Remove |
| `side_i` | 1 | 0=Bid, 1=Ask |
| `price_i` | 9 | 0..511 |
| `qty_i` | 8 | 0..255 |

### Outputs

| Signal | Width | Values |
|---|---|---|
| `valid_o` | 1 | trade present |
| `market_o` | 2 | 0..3 |
| `side_o` | 1 | 0=Bid, 1=Ask |
| `price_o` | 9 | 0..511 |
| `qty_o` | 8 | 0..255 |
| `error_o` | 1 | sticky, global |
| `spare0_o` | 1 | tied 0 |

No ready/backpressure either direction. 1 message consumed per `valid_i` cycle, 0 or 1 trade emitted per cycle.

### Message routing

| `message_type_i` | `market_i` | Target |
|---|---|---|
| Public | 0..3 | order book 0..3 |
| Private | 0 or 1 | `arb_trader` |
| Private | 2 | `momentum_trader` |
| Private | 3 | `ema_trader` |

Public: `op_i`/`side_i`/`price_i`/`qty_i` -> book Insert/Remove.
Private: `side_i`/`qty_i` -> fill notification (`filled_side_i`, `filled_qty_i`); `price_i` unused. Updates trader bookkeeping only, does not touch any order book.

### Pipeline latency

| Stage | Location | Cycles |
|---|---|---|
| Input boundary register | `hft_chip` | 1 |
| Top-of-book register | `hft_core` | 1 |
| Output boundary register | `hft_chip` | 1 |

Book update at cycle `T` -> visible at trader top-of-book from `T+2` -> resulting trade at output pads from `T+3`.

## 3. Orderbooks

4 instances, one per market.

| Impl | Selected by | Structure | Best extraction | Sentinels |
|---|---|---|---|---|
| `orderbook_unsorted` (taped-out) | `UNSORTED=1`  | unsorted pool, empty slot = `qty==0` | comb. reduction tree over all N | none, full `[0,511]` valid |
| `orderbook_sorted` | `UNSORTED=0` | sorted shift register | index 0 | empty tail = `DEFAULT_BID`/`DEFAULT_ASK` |

Both checked directly against the golden model `sw/orderbook.c` (`tb/tb_orderbook.cpp` / `tb/tb_orderbook_unsorted.cpp`)

### 3.1 Insert

| Case | Unsorted | Sorted |
|---|---|---|
| exact price match | add qty; error if overflow (`qty_i > 255 - cur_qty`) | same |
| no match, room | place in empty slot | shift into sorted position if more extreme than ≥1 level, drop tail level (`N-1`) |
| no match, full | evict worst level if new price beats it, else drop silently | if it beast worst level, evict worst and shift into sorted position else drop silently |

### 3.2 Remove

| Case | Behavior |
|---|---|
| exact match, qty sufficient | subtract qty; qty->0 frees slot (unsorted) / removes level + sentinel-fills tail (sorted) |
| no match or insufficient qty | error |

### 3.3 Errors (per book)

- `qty_i == 0` on any op
- Insert overflow at matched level
- Remove with no match or insufficient qty
- invalid `side_i`/`op_i` (defensive only, not reachable)

Sticky: once set, book ignores all further ops until reset.

### 3.4 Qty sums

Each book maintains signed running qty sum per side, incremental update (`SUM_WIDTH = $clog2(N)+QTY_WIDTH` = 14b), aligned with top-of-book pipeline depth. Only book 2's sums are wired out of `hft_core` (-> `momentum_trader`); books 0,1,3 leave them unconnected.

### 3.5 Empty-side output

| Impl | Empty bid | Empty ask |
|---|---|---|
| unsorted | `(DEFAULT_BID, qty=0)` | `(DEFAULT_ASK, qty=0)` |
| sorted | sentinel at index 0, `qty=0` | sentinel at index 0, `qty=0` |

## 4. Price encoding

- `PRICE_WIDTH=9`, range `[0,511]`.
- `0`=`DEFAULT_BID`, `511`=`DEFAULT_ASK`: sentinels for empty levels.
- Tradable range: `[1,510]`.
- No RTL check rejects inserts at sentinel prices. Traders never quote there in practice (gated by `qty != 0` liquidity checks).

## 5. Traders

All traders read only top-of-book (best bid/ask price+qty) of their market(s). 2-bit `pending` counter (wraps mod 4) tracks outstanding fills. Signed running position.

### 5.1 arb_trader (markets 0, 1)

States: `IDLE -> TRADE1 -> TRADE2 -> FLATTEN -> IDLE`

| State | Condition | Action |
|---|---|---|
| IDLE | `bid_price0 > ask_price1 + ARB_THRESHOLD` (arb0) | latch legs, qty=`min(bid_qty0,ask_qty1)`, -> TRADE1 |
| IDLE | `bid_price1 > ask_price0 + ARB_THRESHOLD` (arb1) | latch legs (mirrored), -> TRADE1 |
| TRADE1 | — | emit Ask leg, `pending++`, -> TRADE2 |
| TRADE2 | — | emit Bid leg, `pending++`, -> FLATTEN |
| FLATTEN | `pending==0`, `residual==0` | -> IDLE |
| FLATTEN | `pending==0`, `residual!=0` | emit flatten trade: side=Ask if long else Bid, qty=`\|residual\|`, price=better of the two markets, -> stays until liquid |

TRADE1/TRADE2 emit unconditionally, no `grant_i` — blocks momentum/ema for 2 cycles by priority alone.

### 5.2 momentum_trader (market 2)

States: `IDLE -> TRADE -> WAIT -> IDLE`

| State | Condition | Action |
|---|---|---|
| IDLE | `imb = bid_qty_sum - ask_qty_sum`; `imb > IMB_THRESHOLD`, `pos < MAX_POS`, ask liquid | quote Bid @ ask_price, qty=`min(ORDER_QTY, ask_qty, MAX_POS-pos)`, -> TRADE |
| IDLE | `imb < -IMB_THRESHOLD`, `pos > -MAX_POS`, bid liquid | quote Ask @ bid_price, qty=`min(ORDER_QTY, bid_qty, MAX_POS+pos)`, -> TRADE |
| TRADE | `grant_i` | hold quote until granted; `pending++`, -> WAIT |
| WAIT | `pending==0` | -> IDLE |

### 5.3 ema_trader (market 3)

EMA filter (combinational, runs every cycle regardless of FSM state):
- `mid = (bid_price + ask_price) >> 1`, `mid_scaled = mid << EMA_SHIFT`
- both sides liquid, first tick: `ema = mid_scaled` (init)
- both sides liquid, subsequent: `ema += (mid_scaled - ema) >> EMA_SHIFT`
- either side illiquid: `ema` holds

States: `IDLE -> TRADE -> WAIT -> IDLE` (same shape as momentum)

| State | Condition | Action |
|---|---|---|
| IDLE | `dev = (mid_scaled - ema) >> EMA_SHIFT`; `dev < -DEV_THRESHOLD`, `pos < MAX_POS` | quote Bid @ ask_price, qty=`min(ORDER_QTY, ask_qty, MAX_POS-pos)`, -> TRADE |
| IDLE | `dev > DEV_THRESHOLD`, `pos > -MAX_POS` | quote Ask @ bid_price, qty=`min(ORDER_QTY, bid_qty, MAX_POS+pos)`, -> TRADE |
| TRADE | `grant_i` | hold quote; `pending++`, -> WAIT |
| WAIT | `pending==0` | -> IDLE |

### 5.4 Fill handling (all traders)

On Private message addressed to trader, while not in error:
- `pending--` (2-bit there are never more than 2 pending trades by design)
- `position += filled_qty` if fill side Bid, else `-= filled_qty`

## 6. Output arbitration

Fixed priority: **arb > momentum > ema**, combinational, in `hft_core`.

```
grant_mom = valid_mom & ~valid_arb
grant_ema = valid_ema & ~valid_arb & ~valid_mom
valid_o   = valid_arb | valid_mom | valid_ema
market_o  = valid_arb ? {1'b0,market_arb} : valid_mom ? 2 : 3
side_o/price_o/qty_o muxed the same way
```

- arb has no `grant_i` input; its 2-cycle TRADE1/TRADE2 burst blocks momentum/ema purely by priority.
- A trader that loses arbitration holds its quote and retries next cycle.
- `market_o` for momentum is hardwired to 2, ema to 3; only arb's output carries its own market bit.

## 7. Error handling

| Level | Signal | Set by |
|---|---|---|
| per book | `error_ob[3:0]` | see 3.3 |
| per trader | `error_trader[2:0]` | Private fill while `pending==0` |
| global | `error_o = \|error_ob \| \|error_trader` | either above |

- Sticky and global by design: no in-band clear, only `rst_ni` clears it.