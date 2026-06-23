#!/bin/bash
# Full sign-off run for the current RTL: synth -> backend -> GDS -> DRC -> LVS,
# then snapshot outputs + timing + DRC/LVS verdict into artifacts/n<LABEL>-signoff/.
# Usage (run inside tmux on the remote):
#   bash scripts/signoff.sh <LABEL>     e.g. LABEL=32 for an N=32 run
# LABEL only names the snapshot dir; the actual N comes from rtl/hft_chip.sv.
set -euo pipefail

LABEL="${1:?usage: signoff.sh <LABEL>}"
ROOT="/scratch/vlsi2_19fs26/hft-chip"
cd "$ROOT"

# --- oseda part: synth, backend, GDS, DRC ---
oseda bash <<'OSEDA'
set -e
cd yosys    && ./run_synthesis.sh --synth      > /dev/null 2>&1
cd ../openroad && ./run_backend.sh --all       > /dev/null 2>&1
cd ../klayout  && ./def2gds-hft                 > /dev/null 2>&1
                  ./run_drc-hft                 > drc_run.log 2>&1
OSEDA

DRC=$(cat "$ROOT"/klayout/drc/out/*.lyrdb | grep -c '<item>' || true)

# --- native part: Calibre LVS (needs PDK env via csh) ---
cd "$ROOT"/calibre/lvs
./verilog2spice ../../openroad/out/hft_lvs.v hft_chip.spice > v2s.log 2>&1
cd "$ROOT"
csh -c 'source .setPDK.csh; cd calibre/lvs; calibre-2021.3 calibre -lvs -hier -turbo 8 _lvs_hft.tvf_' \
    > calibre/lvs/lvs_hft_run.log 2>&1
LVS=$(grep -E '^[[:space:]]+(CORRECT|INCORRECT)' calibre/lvs/hft_chip.lvs.report | head -1 | awk '{print $1}')

# --- snapshot ---
D="artifacts/n${LABEL}-signoff"
mkdir -p "$D"/openroad "$D"/klayout "$D"/calibre "$D"/logs "$D"/reports
cp openroad/out/hft.def openroad/out/hft.odb openroad/out/hft.sdc openroad/out/hft.v openroad/out/hft_lvs.v "$D"/openroad/
cp klayout/out/hft.gds "$D"/klayout/
cp calibre/lvs/hft_chip.spice calibre/lvs/hft_chip.lvs.report "$D"/calibre/
cp openroad/logs/*.log "$D"/logs/ 2>/dev/null || true
cp openroad/reports/*.rpt "$D"/reports/ 2>/dev/null || true
cp klayout/drc_run.log calibre/lvs/lvs_hft_run.log calibre/lvs/v2s.log "$D"/logs/ 2>/dev/null || true

UTIL=$(grep -E 'Utilization' openroad/logs/02_placement.log | tail -1 | sed 's/.*Utilization:/Utilization:/')
{
  echo "label=n${LABEL}, commit $(git rev-parse --short HEAD)"
  echo "DRC: ${DRC} violations"
  echo "LVS: ${LVS}"
  echo "${UTIL}"
  echo "timing:"
  grep -E 'wns max|tns max|worst slack max' openroad/reports/05_hft.final.rpt | sed 's/^/  /'
} > "$D"/README.txt

echo "DONE label=n${LABEL} DRC=${DRC} LVS=${LVS}"
cat "$D"/README.txt
