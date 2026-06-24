#!/usr/bin/env bash
# Build+run a directed trader scenario on the remote, pull the CSV back, plot it.
# Usage: scripts/scenario.sh <ema|momentum|arb>
set -euo pipefail

REMOTE=vlsi2-remote
RDIR=/scratch/vlsi2_19fs26/hft-chip

here() { cd "$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"; }

trader="${1:-}"
case "$trader" in
  ema)      target=ema_scenario;      csv=ema_scenario.csv;      title=EMA ;;
  momentum) target=momentum_scenario; csv=momentum_scenario.csv; title=Momentum ;;
  arb)      target=arb_scenario;      csv=arb_scenario.csv;      title=Arb ;;
  *) echo "usage: scenario.sh {ema|momentum|arb}" >&2; exit 2 ;;
esac

here
bash scripts/remote.sh run "make -C tb $target"

mkdir -p report/results report/figures
scp "$REMOTE:$RDIR/tb/$csv" "report/results/$csv"

.venv/bin/python scripts/plot_scenario.py "report/results/$csv" --title "$title" \
  --out "report/figures/${trader}_scenario.png"
