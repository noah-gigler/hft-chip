# VLSI2 Chip Project: HFT-CHIP

VLSI2 course project at ETH Zurich (227-0147-00 L). Team: Luis Wirth, Noah Gigler.

**ETH remote machine**: `ssh vlsi2-remote`, repo at `/scratch/vlsi2_19fs26/hft-chip`

---

## Course context

The standard VLSI2 track has students extend the **Croc SoC** (RISC-V core, UART, JTAG, user
domain peripheral) on IHP SG13G2 130nm. Grading includes running the course's standard Croc
testbench and checking for required ports (`uart_rx_i`, `uart_tx_o`, `clk_i`, `rst_ni`, JTAG, ...).

**Our project is a full-custom ASIC — no RISC-V, no UART, no JTAG.** Frank Gürkaynak approved
this deviation explicitly. Consequences:
- We will fail the course's standard Croc testbench. This is expected and accepted.
- We must provide our own custom testbench and document it thoroughly in the report.
- We still reuse the Croc pad cells, QFN-64 footprint, and power-pad layout so that the
  floorplan and power-grid scripts apply without modification. Clock and reset were moved to
  new pad positions.

### Submission deadline

**June 30, 2026, 10:00am CEST**

### Required submission files

Single `.tar.gz` archive (named per group username) containing:
- Report PDF (≤16 pages)
- `.cockpitrc`
- `openroad/out/` — `croc_lvs.v`, `croc.def`, `croc.odb`, `croc.sdc`, `croc.v`
- `klayout/out/croc.gds`
- `rtl/`
- `sw/` (any compiled SW; unclear for ASIC)
- `tb` (our custom testbench)

### Grading targets

- +0.50 DRC/LVS pass
- +0.25 simulation passes (our custom testbench)
- +1.00 technical report content
- +0.50 report quality
- +0.25 chosen for tapeout (bonus)

### Physical constraints

- Chip size: **2500 µm × 2000 µm** (fixed, must match Croc bond diagram)
- Pad positions: must match bond diagram; power pads must not move
- No sealring / metal fill needed in submission (TAs handle post-submission)
- Known DRC false positive to ignore: `metal1_pin_Offgrid` inside pad cells and SRAM macros (IHP PDK issue)


## Flow

RTL (SystemVerilog) → synthesis (Yosys) → floorplan/place/route (OpenROAD) → DRC/LVS (KLayout/Calibre)

Process: IHP SG13G2 (130nm). Dependencies via Bender.

---

## Current design

The chip (`croc_chip`, top-level) is a market participant in a continuous limit order book.
It ingests market data (Insert/Remove on bids/asks) over a parallel pad interface, maintains
4 independent order books (one per market), and emits trades from 3 concurrent strategies:

- **arb_trader**: cross-market arbitrage between markets 0 & 1
- **momentum_trader**: order book imbalance signal on market 2
- **ema_trader**: EMA mean-reversion on market 3

Output arbitration: arb > momentum > ema (fixed priority).

Key parameters: `PRICE_WIDTH=9`, `QTY_WIDTH=8`, `N=32` price levels per side per book.

Pipeline registers sit between orderbooks and traders (break timing path), and on all outputs
(pad cells are slow).


