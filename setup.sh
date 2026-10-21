#!/usr/bin/env bash
# Build Alter Echo for this PC from your own copy of the game.
#
#   ./setup.sh /path/to/AlterEcho.iso      an Xbox disc image (XISO)
#   ./setup.sh /path/to/extracted/game     the disc's files (default.xbe and
#                                          the "ODB Data" folder)
#
# Nothing from the game is in this repository. This script takes your copy,
# checks it is the release the project was made for, translates its code to
# C on this machine, compiles that, and sets up the Wine prefix run.sh uses.
# Afterwards: ./run.sh
#
# Needs: python3, cmake, ninja, x86_64-w64-mingw32-gcc (MinGW-w64), git, wine
# and DXVK (setup_dxvk, or the dxvk package). The xboxrecomp toolkit is
# fetched next to this folder (../xboxrecomp) if it is not there already, or
# XBOXRECOMP= names another checkout. Expect 20-40 minutes and several GB of
# RAM (JOBS=2 ./setup.sh is faster if you have the memory).
set -euo pipefail

cd "$(dirname "$0")"
P="$(pwd)"
T="${XBOXRECOMP:-$P/../xboxrecomp}"

# The toolkit this project is built against: upstream xboxrecomp plus the
# work done for this game (the Direct3D 11 backend, the fixes in the README's
# table), on the fork's gpu-backend branch. Upstream alone does not build it.
# An existing checkout is never changed, only checked: it must contain this
# commit (a later one on the same branch is fine).
TOOLKIT_URL="${TOOLKIT_URL:-https://github.com/FILL-IN/xboxrecomp.git}"
TOOLKIT_COMMIT="d1ae65fbd78df15e94dc7d8b5e5d099a479b48d6"

say()  { printf '\n== %s\n' "$*"; }
fail() { printf '\nsetup: %s\n' "$*" >&2; exit 1; }

# The release everything here was made against: Alter Echo, NTSC, version 1.
# The function list, the fixes and the addresses in the notes are specific to
# this exact executable, so another release would build and then misbehave.
KNOWN_XBE_SHA1="9b6a1b054bfe35f43c6478d46228b58afb759273"

[ "$(id -u)" -ne 0 ] || fail "run this as your own user, not root"
[ $# -ge 1 ] || fail "usage: ./setup.sh <game.iso | extracted game folder>"
SRC="$1"

# ---- tools ----------------------------------------------------------------
say "Checking tools"
missing=()
for t in python3 cmake ninja x86_64-w64-mingw32-gcc git wine; do
    command -v "$t" >/dev/null || missing+=("$t")
done
[ ${#missing[@]} -eq 0 ] || fail "missing: ${missing[*]} (install them with your package manager)"

if [ ! -e "$T" ]; then
    say "Fetching the xboxrecomp toolkit into $T"
    case "$TOOLKIT_URL" in *FILL-IN*)
        fail "this copy of setup.sh has no toolkit URL yet: clone the toolkit to $T yourself (or set TOOLKIT_URL=)" ;;
    esac
    git clone --quiet "$TOOLKIT_URL" "$T" || fail "could not clone $TOOLKIT_URL"
    git -C "$T" checkout --quiet "$TOOLKIT_COMMIT" \
        || fail "$TOOLKIT_URL has no commit $TOOLKIT_COMMIT"
fi
[ -d "$T/tools/recomp" ] || fail "$T is not the xboxrecomp toolkit (set XBOXRECOMP= to the right folder)"
if [ -d "$T/.git" ]; then
    git -C "$T" merge-base --is-ancestor "$TOOLKIT_COMMIT" HEAD 2>/dev/null \
        || fail "the toolkit at $T does not contain $TOOLKIT_COMMIT, the version this project needs. Update it: git -C \"$T\" fetch && git -C \"$T\" checkout $TOOLKIT_COMMIT"
else
    # A copy without its history (an unpacked archive): the next best check.
    grep -q xbox_PhysicalOrigin "$T/src/kernel/xbox_memory_layout.c" 2>/dev/null \
        || fail "the toolkit at $T is too old for this project"
fi

# Python with capstone, which the disassembler needs.
PY="${PY:-}"
for cand in "$PY" "$HOME/.venvs/xboxrecomp/bin/python" "$P/.venv/bin/python"; do
    [ -n "$cand" ] && [ -x "$cand" ] && "$cand" -c 'import capstone' 2>/dev/null && { PY="$cand"; break; }
done
if ! [ -n "$PY" ] || ! "$PY" -c 'import capstone' 2>/dev/null; then
    say "Creating a Python environment with capstone (.venv)"
    python3 -m venv "$P/.venv" || fail "python3 -m venv failed (install python's venv module)"
    "$P/.venv/bin/python" -m pip install --quiet capstone || fail "could not install capstone"
    PY="$P/.venv/bin/python"
fi
export PY

# ---- the game files -------------------------------------------------------
# default.xbe first, from an image without unpacking the rest, so a copy of
# the wrong release is turned away in seconds rather than after 3 GB.
check_xbe() {
    local sum
    sum="$(sha1sum "$1" | cut -d' ' -f1)"
    if [ "$sum" != "$KNOWN_XBE_SHA1" ]; then
        if [ "${FORCE:-0}" = 1 ]; then
            echo "default.xbe is $sum, not the supported release; continuing (FORCE=1)"
        else
            fail "default.xbe is $sum; this project supports the NTSC release ($KNOWN_XBE_SHA1). FORCE=1 tries anyway, unsupported."
        fi
    else
        echo "default.xbe matches the supported release (NTSC, title 5451000F)"
    fi
}

say "Checking it is the supported release"
if [ -f "$SRC" ]; then
    tmp="$(mktemp -d)"
    (cd "$T" && "$PY" -m tools.xiso get "$SRC" default.xbe -o "$tmp" >/dev/null) \
        || { rm -rf "$tmp"; fail "could not read $SRC as an Xbox disc image"; }
    check_xbe "$tmp/default.xbe"
    rm -rf "$tmp"
elif [ -d "$SRC" ]; then
    [ -f "$SRC/default.xbe" ] || fail "no default.xbe in $SRC"
    check_xbe "$SRC/default.xbe"
else
    fail "$SRC is neither a file nor a folder"
fi
if [ "${CHECK_ONLY:-0}" = 1 ]; then
    say "Your copy and tools are fine (CHECK_ONLY=1: stopping here)"
    exit 0
fi

if [ -f "$SRC" ]; then
    say "Unpacking the disc image into game-files/ (about 3 GB)"
    mkdir -p "$P/game-files"
    (cd "$T" && "$PY" -m tools.xiso unpack "$SRC" -o "$P/game-files") \
        || fail "could not unpack $SRC"
    GAME="$P/game-files"
else
    GAME="$(cd "$SRC" && pwd)"
fi
ln -sfn "$GAME" "$P/game"

# ---- translate and build --------------------------------------------------
say "Extracting the game's runtime-loaded modules"
"$PY" "$P/tools/extract_modules.py" "$P/game" "$P/modules"

say "Translating the game's code to C (5-10 minutes)"
"$P/tools/regen_modules.sh"

say "Compiling (10-20 minutes)"
JOBS="${JOBS:-1}" "$P/build.sh"

# ---- Wine prefix ----------------------------------------------------------
PREFIX="$HOME/.local/share/wineprefixes/alter-echo-recomp"
say "Setting up the Wine prefix with DXVK ($PREFIX)"
export WINEPREFIX="$PREFIX" WINEDEBUG=-all
mkdir -p "$PREFIX"
wineboot -u >/dev/null 2>&1 || true
if command -v setup_dxvk >/dev/null; then
    setup_dxvk install >/dev/null
elif [ -x /usr/share/dxvk/setup_dxvk.sh ]; then
    /usr/share/dxvk/setup_dxvk.sh install >/dev/null
else
    echo "DXVK not found: install it (setup_dxvk, or winetricks dxvk) into $PREFIX,"
    echo "or the game falls back to Wine's slower Direct3D."
fi

say "Done"
echo "Play with ./run.sh  (settings: settings.ini, made on first run)"
