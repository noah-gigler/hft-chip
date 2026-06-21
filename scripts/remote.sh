#!/usr/bin/env bash
# Drive the VLSI2 remote (vlsi2-remote / tardis) from the local Mac.
#
# Workflow contract:
#   - git is the source of truth; local and remote stay on the SAME commit for
#     completed units of work (commit locally, push, `remote.sh pull`).
#   - during active iteration, `remote.sh sync` rsyncs ONLY changed files in the
#     folders of interest. It never deletes and never touches generated flow
#     output (openroad/out, calibre/, innovus/, dfii/, *.log, ...).
#
# Tools (yosys/verilator/openroad/klayout) live on the remote at /usr/sepp/bin.
set -euo pipefail

REMOTE=vlsi2-remote
RDIR=/scratch/vlsi2_19fs26/hft-chip

# Source dirs pushed during iteration. Add here if scope grows.
SRC=(rtl tb sw openroad/scripts openroad/src)

# rsync: archive, compress, only changed files (size+mtime), show changes.
# NO --delete (would clobber remote generated artifacts).
RSYNC_OPTS=(-az --checksum --itemize-changes --relative
  --exclude='*.log' --exclude='.DS_Store' --exclude='obj_dir/')

here() { cd "$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"; }

cmd_sync() {
  here
  # --relative keeps the rtl/ tb/ ... prefixes so layout is preserved.
  rsync "${RSYNC_OPTS[@]}" "${SRC[@]}" "$REMOTE:$RDIR/"
}

# Run a command on the remote, inside the repo, INSIDE the oseda container
# (that's where bender/yosys/openroad/verilator actually live).
# git itself runs on the host, outside the container.
# oseda ignores CLI args to the progname; it runs `bash` reading from stdin.
# So we pipe the script in. env.sh is sourced inside the container.
cmd_run() {
  local script="cd $RDIR && source env.sh >/dev/null 2>&1 || true; $*"
  ssh "$REMOTE" "export LC_ALL=en_US.UTF-8; cd $RDIR && oseda bash" <<<"$script"
}
# Host-side (no container) — for git, rsync targets, file ops, SEPP-native bins.
cmd_host() { ssh "$REMOTE" "cd $RDIR && $*"; }

cmd_sim()   { cmd_sync; cmd_run "make -C tb"; }
cmd_synth() { cmd_sync; cmd_run "${*:-bash openroad/run_backend.sh}"; }

# Keep remote working tree at the same commit as local (completed work only).
cmd_pull()  { cmd_host "git fetch origin && git checkout main && git pull --ff-only"; }
cmd_status(){ cmd_host "git rev-parse --short HEAD; git status --porcelain | head"; }

case "${1:-}" in
  sync)   shift; cmd_sync ;;
  sim)    shift; cmd_sim ;;
  synth)  shift; cmd_synth "$@" ;;
  run)    shift; cmd_sync; cmd_run "$@" ;;   # EDA tools: inside oseda container
  hrun)   shift; cmd_host "$@" ;;            # host shell: no container, no sync
  pull)   shift; cmd_pull ;;
  status) shift; cmd_status ;;
  *) echo "usage: remote.sh {sync|sim|synth|run <cmd>|hrun <cmd>|pull|status}" >&2
     echo "  run  = sync sources + run inside oseda container (verilator/yosys/openroad/bender/klayout)" >&2
     echo "  hrun = run on the host shell, no container, no sync (git, file ops, SEPP-native bins)" >&2
     exit 2 ;;
esac
