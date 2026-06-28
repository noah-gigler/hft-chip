# VLSI2 HFT-CHIP

VLSI2 course project at ETH Zurich (227-0147-00 L). Team: Luis Wirth, Noah Gigler.

## Remote machine

- Claude Code runs locally (Mac). All EDA tooling is on remote.
- Access: `ssh vlsi2-remote` — host `tardis`, user `vlsi2_19fs26`, repo `/scratch/vlsi2_19fs26/hft-chip`
- Same GitHub repo (`github:noah-gigler/hft-chip`) on both machines; keep both on the same commit for completed work.
- Tooling is behind `oseda` (SEPP/Apptainer container): Verilator 5.046, Bender 0.31.0, Yosys 0.64, OpenROAD, KLayout
- git runs **outside** oseda. Gotcha: `oseda bash -c '...'` does NOT work — pipe a script instead:
  `ssh vlsi2-remote "cd <repo> && oseda bash" <<<'verilator ...'`
- **Always run long flows in a named tmux session** on the remote so they survive ssh/Mac/Claude disconnection. Pattern: `ssh vlsi2-remote "tmux new -d -s <name> 'cd <repo> && oseda bash <<<\"...\"'"`; reattach with `tmux attach -t <name>`, check liveness with `tmux ls`.

**Driver: `scripts/remote.sh`**
- `sync` — rsync `rtl/ tb/ sw/ openroad/scripts openroad/src` with `--checksum`, no `--delete`, excludes `*.log`/`obj_dir`
- `sim` / `synth` / `run <cmd>` — sync then run in container
- `pull` / `status` — host git ops
- `--checksum` is required (git checkout leaves mismatched mtimes). Never `--delete` the remote root — it holds untracked artifacts (`openroad/out/`, `calibre/`, `innovus/`, `dfii/`).

## Course context

Standard VLSI2 track: extend Croc SoC (RISC-V + UART + JTAG) on IHP SG13G2 130nm.

**Our project is a full-custom ASIC — no RISC-V, no UART, no JTAG.** Frank Gürkaynak approved this deviation.
- Will fail the course's standard Croc testbench — expected and accepted.
- Must provide a custom testbench, documented thoroughly in the report.
- Still reuses Croc pad cells, QFN-64 footprint, and power-pad layout; clock/reset moved to new pad positions.

### Submission — June 30, 2026, 10:00am CEST

Submitted via Nextcloud as `vlsi2_19fs26.tar.gz` containing:
- `vlsi2_19fs26.pdf` (report, ≤16 pages), `.cockpitrc`
- `openroad/out/`: `hft_lvs.v`, `hft.def`, `hft.odb`, `hft.sdc`, `hft.v`
- `klayout/out/hft.gds`
- `rtl/`, `sw/`, `tb/`

Spec lists these as `croc_lvs.v`, `croc.def`, `croc.odb`, `croc.sdc`, `croc.v`, `croc.gds`. We emit `hft.*` (PROJ_NAME=hft, top=hft_chip). Pending TA confirmation that `hft.*` names are accepted for the custom ASIC.

### Grading

| Points | Criterion |
|--------|-----------|
| +0.50 | DRC/LVS pass |
| +0.25 | Custom simulation passes |
| +1.00 | Technical report content |
| +0.50 | Report quality |
| +0.25 | Chosen for tapeout (bonus) |

### Physical constraints

- Chip size: **2500 µm × 2000 µm** (fixed, must match Croc bond diagram)
- Pad positions must match bond diagram; power pads must not move
- No sealring / metal fill needed in submission (TAs handle post-submission)
- Known DRC false positive to ignore: `metal1_pin_Offgrid` inside pad cells and SRAM macros (IHP PDK issue)

## Design

`hft_chip` (top-level) is a thin wrapper: pad ring + boundary I/O registers only. All logic lives in `hft_core`, a hardware market participant in a continuous limit order book.

- Ingests market data (Insert/Remove on bids/asks) via parallel pad interface
- Maintains 4 independent order books (one per market)
- 3 concurrent trading strategies:
  - **arb_trader** — cross-market arbitrage between markets 0 & 1
  - **momentum_trader** — order book imbalance signal on market 2
  - **ema_trader** — EMA mean-reversion on market 3
- Output arbitration: arb > momentum > ema (fixed priority)
- Key parameters: `PRICE_WIDTH=9`, `QTY_WIDTH=8`, `N=64` price levels per side per book
- Traders consume only top-of-book (level 0); momentum also uses a running qty sum maintained incrementally in the orderbook
- Register stages: inputs at the pad boundary, top-of-book between orderbooks and traders, outputs before the pads (pad cells are slow)

#### Orderbook implementations

Two interchangeable impls behind the `orderbook` wrapper (`rtl/orderbook.sv`), selected at elaboration by `UNSORTED` (propagated from `hft_core`'s `OB_UNSORTED`, from `hft_chip`'s `OB_UNSORTED`). **Default everywhere is unsorted (`1'b1`), which is what tapes out.**
- `orderbook_unsorted.sv` — unsorted pool, best computed combinationally. Treats the full `[0,511]` price range as valid (no sentinels). **This is the taped-out impl.**
- `orderbook_sorted.sv` — sorted shift register, best is always level 0. Uses reserved sentinel prices (0/511) for empty levels; see `spec.md`. Select with `OB_UNSORTED=0`.

Both are proven behaviorally identical at top-of-book by `tb/tb_orderbook_equiv.sv` (`make -C tb equiv`), so `hft_core` is impl-agnostic.

### Known RTL concerns

- **Fill contract unenforced**: `filled_qty` (ext, up to 255) not bounded to ordered qty → `position` (signed 10-bit) can wrap; residual can exceed 8 bits (arb qty truncation).
- **`pending` is 2-bit** in all traders: >3 outstanding orders wraps → spurious error.
- **`error` is sticky + global**: one bad message bricks all chip output — deliberate per Noah.

## Flow

RTL (SystemVerilog) → synthesis (Yosys) → floorplan/place/route (OpenROAD) → GDS + DRC (KLayout) → LVS (Calibre)

Process: IHP SG13G2 (130nm). Dependencies via Bender. All steps run inside `oseda` except Calibre LVS (native SEPP tool). PROJ_NAME=hft, top=hft_chip.

Run order (on remote, from repo root):
- Synth: `cd yosys && ./run_synthesis.sh --flist` (regenerate `src/hft.flist` via Bender) then `./run_synthesis.sh --synth` → `out/hft_yosys.v`
- Backend: `cd openroad && ./run_backend.sh --all` (or `--floorplan/--placement/--cts/--routing/--finishing`) → `out/hft.{def,odb,sdc,v}`, `out/hft_lvs.v`
- GDS: `cd klayout && ./def2gds-hft` → `out/hft.gds`
- DRC: `cd klayout && ./run_drc-hft` (flags `--no_recommended --no_density --antenna`); results in `drc/out/*.lyrdb`
- LVS (Calibre, not KLayout — `klayout/run_lvs-hft` is unused):
  1. `cd calibre/lvs && ./verilog2spice ../../openroad/out/hft_lvs.v hft_chip.spice`
  2. adapt runset: `sed -e 's|"croc_chip.calibre.db"|"<abs>/klayout/out/hft.gds"|' -e 's/croc_chip/hft_chip/g' _lvs.tvf_ > _lvs_hft.tvf_`
  3. `csh -c 'source .setPDK.csh; cd calibre/lvs; calibre-2021.3 calibre -lvs -hier -turbo 8 _lvs_hft.tvf_'` → `hft_chip.lvs.report`

LVS gotchas: must use **calibre-2021.3** (default `calibre` symlink is 2018.2, too old for the PDK tvf rules); must `source .setPDK.csh` first (sets `IHP_TECH`); the SEPP wrapper takes `calibre calibre ...` (wrapper name + engine), and tvf is auto-detected via `#!tvf` (no `-tvf` flag).

## Testbench

Verilator + C golden-model co-simulation. Golden models live in `sw/` (`common.h` + `orderbook.c`, `arb_trader.c`, `momentum_trader.c`, `ema_trader.c`). Harnesses in `tb/`.

Run: `bash scripts/remote.sh run "make -C tb <target>"`

| Target | What it tests |
|--------|---------------|
| `orderbook` | Directed `.vec` + constrained-random; targets `orderbook_sorted` directly |
| `arb` / `momentum` / `ema` | Unit, cycle-accurate trader models |
| `chip` | Full `hft_chip` via `pad_stubs.sv`; composite model = input register + 4 OBs + top-of-book pipeline + 3 traders + arbitration + output register. Defaults to unsorted (taped-out); `OB_UNSORTED=0` for sorted |
| `all` | All of the above (~100k random cycles each) |
| `equiv` | `orderbook_sorted` vs `orderbook_unsorted` equivalence (no C model, self-checking SV); add `EQ_N=`/`EQ_NOPS=` to tune |

**Key principle**: golden models are transcribed from the RTL, so passing proves DUT == model (self-consistency), not spec-correctness. Only `anchor_checks()` are independent.

## Guidelines

### RTL changes

- **All RTL refactors must be verified** by running the testbench against the golden C model before committing. Don't change `sw/` during a pure RTL refactor.
- **Behavioral changes are spec changes.** Flag explicitly when an RTL change alters observable behavior. Only touch both RTL and `sw/` together once the change is acknowledged.
- When practical: RTL change first → verify TB passes → then separately change spec + golden model.

### Writing style

Avoid AI writing giveaways: no em-dashes, no filler phrases, no overly polished prose. Write like a human engineer.

### Comments

Avoid writing comments — code should be self-explanatory. Only add one if strictly necessary, and keep it to a single concise line.

### Memory vs CLAUDE.md

Never write project memories. All persistent context — design decisions, RTL concerns, workflow notes, conventions — goes into CLAUDE.md so every Claude instance and team member shares the same source of truth.

### Git commits

- Single concise subject line. No body, no bullet list.
- Never add `Co-Authored-By: Claude`.
- Never use `git add -A` or `git add .` — always stage only the specific files for the change.
