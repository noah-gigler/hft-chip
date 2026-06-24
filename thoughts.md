## Critical paths

1. first() priority encoder (orderbook.sv:53) — written as a sequential for
  loop, which infers an N-deep mux chain (~64 levels) rather than a balanced tree.
  Called twice (on equals and compares). If anything becomes the new critical
  path, this is the prime suspect. Fix: rewrite as a log-depth priority encoder,
  or — since equals is already proven one-hot — compute eq_idx as an OR-reduction
  of indices (balanced tree).
2. N:1 index muxes — cur_qtys[eq_idx], cur_prices[eq_idx] feeding the
  overflow/arithmetic. Log-depth, scales mildly.
3. **onehot = (equals & (equals-1)) ** — an N-bit borrow chain; cheap but grows.

## Whole-chip latency

End-to-end pipeline, input pad → output pad, is 4 register stages:

┌─────┬────────────────────┬─────────────────────────────────────────┐
│  #  │       Stage        │                Register                 │
├─────┼────────────────────┼─────────────────────────────────────────┤
│ 1   │ book update        │ orderbook internal state (*_qtys_q)     │
├─────┼────────────────────┼─────────────────────────────────────────┤
│ 2   │ book→trader        │ hft top-of-book regs (bid_price0_q, …)  │
│     │ pipeline           │  + sums                                 │
├─────┼────────────────────┼─────────────────────────────────────────┤
│ 3   │ trader decision    │ trader FSM state_q (IDLE→TRADE)         │
├─────┼────────────────────┼─────────────────────────────────────────┤
│ 4   │ output             │ hft out_* regs (pads are slow)          │
└─────┴────────────────────┴─────────────────────────────────────────┘

o a Public market-data update at cycle 0 can produce a trade at the output pad at cycle 4.

Per strategy (latency from the triggering input to the emitted trade):
- momentum: 4 cycles
- ema: 4 cycles
- arb: 4 cycles (leg 1, the ask), then 5 (leg 2, the bid) on the next cycle;
  a later FLATTEN trade follows once fills return

One asymmetry worth knowing: the private-fill feed (order_filled_i, filled_*)
goes to the traders combinationally from the input pads — it does not pass
through the orderbook/pipeline stages. So position/pending accounting is on a
different (shorter) timeline than the book-driven trade decisions. That's by
design.


## Backend flow improvements

- stop at global_route to judge reoutability. no need for detailed routing.
- low N of course
- lower global_placement -density 0.60 -> 0.40
- NO global_route -allow_congestion for interation! only for final sign-off. Loud fail at GR.

eventually:
- pipeline qty delta sum from orderbook to traders

