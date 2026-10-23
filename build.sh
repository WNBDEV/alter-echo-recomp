#!/usr/bin/env bash
# Cross-compile the recompiled game as a Windows .exe with MinGW-w64.
#
#   ./build.sh            # configure (first time) + build
#   JOBS=2 ./build.sh     # fewer parallel compiles if memory runs short
#
# GCC's memory use scales with the size of each generated chunk, so this runs
# fewer compiles than there are cores. Alter Echo's chunks are small (largest
# 2.5 MB at --split 250), so JOBS=6 or so is probably fine too. If the build
# dies with a signal rather than an error, check `dmesg` for the OOM killer.
set -euo pipefail

# Claude Code runs as root on this machine; re-run as the project's owner so
# build output stays theirs.
if [ "$(id -u)" -eq 0 ]; then
    exec sudo -u "$(stat -c %U "$0")" -H --preserve-env=JOBS "$0" "$@"
fi

cd "$(dirname "$0")"

# On Windows (MSYS2's MINGW64 shell) MinGW is the native compiler, so CMake
# finds it as it is; on Linux it is a cross-compiler, named by the toolchain.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) toolchain_arg=() ;;
    *) toolchain_arg=(-DCMAKE_TOOLCHAIN_FILE="$(realpath ../xboxrecomp/cmake/mingw-w64-x86_64.cmake)") ;;
esac

cmake -S . -B build-win -G Ninja \
    "${toolchain_arg[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS_RELEASE="-O2"

cmake --build build-win -j"${JOBS:-3}"

echo
echo "Built: build-win/alter_echo_recomp.exe  -- run it with ./run.sh (Windows: run.bat)"
