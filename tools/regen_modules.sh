#!/usr/bin/env bash
# Regenerate the C code for default.xbe plus every runtime-loaded module,
# from the game setup has already linked at game/ (run as your own user).
# The pipeline itself is translate() in tools/setup_game.py; this is the
# shortcut for running only that step.
set -euo pipefail
cd "$(dirname "$0")/.."
exec python3 tools/setup_game.py --translate "$@"
