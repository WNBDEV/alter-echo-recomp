#!/usr/bin/env bash
# Build Alter Echo for this PC from your own copy of the game (Linux, Wine).
#
#   ./setup.sh /path/to/AlterEcho.iso      an Xbox disc image (XISO)
#   ./setup.sh /path/to/extracted/game     the disc's files (default.xbe and
#                                          the "ODB Data" folder)
#
# The work is done by tools/setup_game.py, which the Windows installer runs
# too; its options are listed there (--check-only, --jobs N, ...), and the
# old environment variables (CHECK_ONLY=1, JOBS=2, NATIVE=1, FORCE=1,
# XBOXRECOMP=) still work. Needs python3, git, cmake, ninja, MinGW-w64
# (x86_64-w64-mingw32-gcc), wine and DXVK. Expect 20-40 minutes. Afterwards:
# ./run.sh
set -euo pipefail
cd "$(dirname "$0")"
command -v python3 >/dev/null || { echo "setup: missing: python3 (install it with your package manager)" >&2; exit 1; }
exec python3 tools/setup_game.py "$@"
