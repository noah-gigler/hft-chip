# hft-chip

A full-custom ASIC implementing a hardware market participant for high-frequency trading, built for the VLSI2 course at ETH Zurich (227-0147-00 L).

The chip ingests live market data (order inserts/removals) for 4 independent markets over a parallel pad interface and reconstructs a N-level orderbook for each one entirely in hardware. 

Three trading strategies each watch their own market's order book and decide every cycle whether to trade: arbitrage watches markets 0 and 1 for a cross-market spread, momentum watches market 2's order-flow imbalance, and EMA mean-reversion watches market 3's price deviation from its running average. 

A fixed-priority arbiter resolves conflicts into a single trade decision per cycle, all on-chip with no off-chip compute in the loop. A trader that loses arbitration simply holds its trade and retries on the next cycle until granted, so no signal is ever dropped. Collisions should be rare in practice: each strategy watches a different market, so their trade signals are independent.

## Note for randomized Croc testing

This project is **not** an extension of the Croc RISC-V SoC. It is a full-custom ASIC: no RISC-V core, no UART, no JTAG. This deviation was proposed and approved by the course staff.

Consequences:
- It will **fail** the standard Croc testbench; this is expected and intentional, not a bug.
- A custom testbench is used instead (see [Building and testing](#building-and-testing)), documented in the report and below.
- The design still reuses Croc's pad cells, QFN-64 footprint, and power-pad layout for mechanical compatibility; clock and reset were moved to new pad positions.

## Design

`hft_chip` is the top-level pad ring + boundary I/O registers, wrapping `hft_core`, which contains all the logic:

- 4 independent order books, each tracking bids and asks across `N=64` price levels
- 3 concurrent trading strategies, each consuming only the top of book:
  - **arb_trader**: cross-market arbitrage between markets 0 and 1
  - **momentum_trader**: order book imbalance signal on market 2
  - **ema_trader**: EMA mean-reversion on market 3
- Fixed-priority output arbitration: arb > momentum > ema

Key parameters: `PRICE_WIDTH=9`, `QTY_WIDTH=8`, `N=64`.

See [`spec.md`](spec.md) for output arbitration rules and price encoding.

## Repo layout

| Path | Contents |
|---|---|
| `rtl/` | SystemVerilog sources (`hft_chip`, `hft_core`, traders, orderbook implementations) |
| `tb/` | Verilator testbenches, co-simulated against C golden models |
| `sw/` | C golden models (`orderbook.c`, `arb_trader.c`, `momentum_trader.c`, `ema_trader.c`) |
| `yosys/` | Synthesis flow (Yosys) |
| `openroad/` | Floorplan / place / route / CTS flow (OpenROAD) |
| `klayout/` | GDS generation and DRC |
| `calibre/` | LVS (Calibre, native tool, not via KLayout) |
| `scripts/` | Flow drivers: `remote.sh` (remote sync/run), `signoff.sh` (full sign-off pipeline), `scenario.sh` / `plot_scenario.py` (trading scenario plots) |
| `report/` | Scenario results (`results/*.csv`) and plots (`figures/*.png`) used in the report |
| `spec.md` | Output arbitration and price-encoding spec |

## Flow

```
RTL (SystemVerilog) → Yosys synthesis → OpenROAD place & route → KLayout DRC/GDS → Calibre LVS
```

Process: IHP SG13G2 (130 nm). Chip size: 2500 µm × 2000 µm (fixed, matches the Croc bond diagram). Pad positions and power pads also match the bond diagram.

## Building and testing

Simulation only needs Verilator and a C compiler. `make all` runs the orderbook, arb, momentum, ema, and chip targets covered in the per-trader sections below:

```bash
make -C tb all
```

The backend flow needs IHP SG13G2 PDK tooling (Yosys, OpenROAD, KLayout, Calibre) set up. Synthesis, then place & route, then GDS + DRC:

```bash
cd yosys && ./run_synthesis.sh --flist && ./run_synthesis.sh --synth
cd openroad && ./run_backend.sh --all
cd klayout && ./def2gds-hft && ./run_drc-hft
```

LVS is run with Calibre directly (not via KLayout); see `calibre/lvs/`.

## Verification

Verification is Verilator + C golden-model co-simulation: each SystemVerilog unit is driven cycle-by-cycle against a same-named C model in `sw/`, comparing every output every cycle. All targets accept `SEED=<n>` and `NOPS=<n>` to control the random stream length (default `NOPS=100000`).

### Orderbook

`rtl/orderbook_sorted.sv` / `rtl/orderbook_unsorted.sv`, golden model `sw/orderbook.c`.

Maintains one side (bid or ask) of a price-level book over `N=64` levels with insert/remove operations. The sorted implementation keeps levels ordered by price in a shift register (best is always level 0); the unsorted implementation (the one that tapes out) holds an unordered pool and computes the best price combinationally. Both are checked directly against the same golden model: `orderbook_sorted` gets a full per-level state check, `orderbook_unsorted` gets a top-of-book + qty-sum check (its internal slot order has no sorted counterpart to compare against). Prices 0/511 are excluded from the unsorted impl's random stimulus — they collide with the sorted impl's empty-slot sentinel and would falsely flag as a mismatch.

Runs directed vectors plus a constrained-random regression:

```bash
make -C tb orderbook
```

### Arbitrage trader (`arb_trader`)

`rtl/arb_trader.sv`, golden model `sw/arb_trader.c`.

Cross-market arbitrage between market 0 and market 1: watches the best bid of one market against the best ask of the other, and trades whenever the spread exceeds `ARB_THRESHOLD` (default 2) in either direction. On a signal it emits two back-to-back trades (`TRADE1` + `TRADE2`), during which the other traders are blocked from the output port.

Constrained-random regression:

```bash
make -C tb arb
```

Scenario plot (directed cross-market arbitrage scenario, plotted from `hft_chip`):

```bash
scripts/scenario.sh arb
```

![arb scenario](report/figures/arb_scenario.png)

### Momentum trader (`momentum_trader`)

`rtl/momentum_trader.sv`, golden model `sw/momentum_trader.c`.

Order-book-imbalance strategy on market 2: tracks a running sum of bid vs. ask quantity (maintained incrementally by the orderbook, not recomputed per cycle) and buys when the imbalance exceeds `IMB_THRESHOLD` (default 2) to the bid side, sells when it exceeds the threshold to the ask side, subject to a `MAX_POS` position cap (default `8 * ORDER_QTY`).

Constrained-random regression:

```bash
make -C tb momentum
```

Scenario plot (directed order-flow-imbalance-leads-price scenario, plotted from `hft_chip`):

```bash
scripts/scenario.sh momentum
```

![momentum scenario](report/figures/momentum_scenario.png)

### EMA mean-reversion trader (`ema_trader`)

`rtl/ema_trader.sv`, golden model `sw/ema_trader.c`.

Mean-reversion strategy on market 3: maintains an exponential moving average of the mid-price (`EMA_SHIFT` controls the decay, default 4) and trades against deviations from it. It buys when price is `DEV_THRESHOLD` (default 4) below the EMA, sells when it's that far above, once the EMA has initialized and both sides of the book are liquid, again subject to `MAX_POS`.

Constrained-random regression:

```bash
make -C tb ema
```

Scenario plot (directed mean-reversion scenario, plotted from `hft_chip`):

```bash
scripts/scenario.sh ema
```

![ema scenario](report/figures/ema_scenario.png)

### Full chip

`make chip` exercises the full `hft_chip` via `pad_stubs.sv` (input regs, 4 orderbooks, top-of-book pipeline, 3 traders, output arbitration, output regs) against a composite C model. Defaults to the taped-out unsorted orderbook; pass `OB_UNSORTED=0` to run it against the sorted impl instead.

```bash
make -C tb chip
```

### Status and results

- **DRC:** pass (0 violations)
- **LVS:** pass (`hft_chip` verified correct)
- Trading scenario plots are shown in each trader's section above; raw results are in [`report/results/`](report/results/)

