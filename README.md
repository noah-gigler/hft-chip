# hft-chip

Custom ASIC for high-frequency trading (HFT).

VLSI2 course project at ETH Zurich (227-0147-00 L)
Team: Noah Gigler, Luis Wirth

## Design

`hft_chip` (top) wraps a pad ring around `hft_core`, which maintains 4 order
books and runs 3 concurrent trading strategies (arbitrage, momentum, EMA
mean-reversion).

Process: IHP SG13G2 130nm.

## Flow

```
RTL (SystemVerilog) → Yosys synthesis → OpenROAD place/route → KLayout DRC/LVS
```
