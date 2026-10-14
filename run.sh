#!/usr/bin/env bash
# Run the recompiled build under Wine, in a dedicated prefix with DXVK.
#
#   ./run.sh
#   WINEDEBUG=+seh ./run.sh      # more Wine detail on exceptions
#
# Runs from the project root so the relative "game\..." paths in main.c
# resolve through the game/ symlink. stderr (crash reports, ICALL traces)
# is shown and also saved to logs/run-<timestamp>.txt.
set -euo pipefail

# Claude Code runs as root on this machine; re-run as the project's owner so
# the Wine prefix and logs stay theirs, keeping the display so a window can open.
if [ "$(id -u)" -eq 0 ]; then
    exec sudo -u "$(stat -c %U "$0")" -H \
        --preserve-env=DISPLAY,XAUTHORITY,WAYLAND_DISPLAY,WINEDEBUG "$0" "$@"
fi

cd "$(dirname "$0")"

export WINEPREFIX="$HOME/.local/share/wineprefixes/alter-echo-recomp"
export WINEDEBUG="${WINEDEBUG:--all}"

mkdir -p logs
log="logs/run-$(date +%Y%m%d-%H%M%S).txt"
echo "stderr -> $log"

wine build-win/alter_echo_recomp.exe "$@" 2> >(tee "$log" >&2)
