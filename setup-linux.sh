#!/usr/bin/env bash
# Build the native Linux version (experimental) into build-linux/.
#
#   ./setup-linux.sh /path/to/AlterEcho.iso     or the extracted game folder
#
# No Wine or MinGW needed. This:
#
#   1. runs ./setup.sh with NATIVE=1, which checks your copy of the game,
#      unpacks it and translates its code to C. Without the game argument
#      the code translated earlier (by either setup) is used as it is;
#   2. fetches DXVK Native -- DXVK's Direct3D 11 as Linux libraries, the same
#      renderer run.sh uses under Wine -- from DXVK's GitHub release, and
#      vkd3d-utils (the HLSL shader compiler) unless it is already installed;
#      both go into build-linux/deps, nothing is installed system-wide;
#   3. configures and compiles with your system compiler.
#
# Then ./run-linux.sh. Needs: python3, git, gcc, cmake, ninja, pkg-config,
# curl, the SDL2, libepoxy and OpenSSL development files, and a Vulkan driver.
# Expect 25-45 minutes (JOBS=2 compiles faster if you have the memory).
set -euo pipefail

cd "$(dirname "$0")"
P="$(pwd)"
T="${XBOXRECOMP:-$P/../xboxrecomp}"
DEPS="$P/build-linux/deps"
DXVK_VERSION=3.1.1
VKD3D_VERSION=1.19

say()  { printf '\n== %s\n' "$*"; }
fail() { printf '\nsetup-linux: %s\n' "$*" >&2; exit 1; }

[ "$(id -u)" -ne 0 ] || fail "run this as your own user, not root"
say "Checking tools and libraries"
missing=()
for t in python3 git gcc cmake ninja curl tar pkg-config; do
    command -v "$t" >/dev/null || missing+=("$t")
done
[ ${#missing[@]} -eq 0 ] || fail "missing: ${missing[*]} (install them with your package manager)"
pkg-config --exists sdl2 || fail "SDL2 development files not found (libsdl2-dev / SDL2-devel / sdl2)"
pkg-config --exists epoxy || fail "libepoxy development files not found (libepoxy-dev / libepoxy-devel / libepoxy)"
pkg-config --exists libcrypto || fail "OpenSSL development files not found (libssl-dev / openssl-devel / openssl)"
echo "ok"

if [ $# -ge 1 ]; then
    NATIVE=1 "$P/setup.sh" "$1"
elif [ -n "$(ls src/recomp/gen/*.c 2>/dev/null)" ]; then
    say "Using the code translated earlier (src/recomp/gen); give your game to translate again"
else
    fail "usage: ./setup-linux.sh <game.iso | extracted game folder>"
fi
mkdir -p "$DEPS"

say "DXVK Native $DXVK_VERSION"
if [ ! -f "$DEPS/dxvk-native/usr/lib/libdxvk_d3d11.so.0" ]; then
    curl -fL --progress-bar -o "$DEPS/dxvk-native.tar.gz" \
        "https://github.com/doitsujin/dxvk/releases/download/v$DXVK_VERSION/dxvk-native-$DXVK_VERSION-steamrt-sniper.tar.gz"
    mkdir -p "$DEPS/dxvk-native"
    tar xzf "$DEPS/dxvk-native.tar.gz" -C "$DEPS/dxvk-native"
    rm -f "$DEPS/dxvk-native.tar.gz"
fi
echo "in $DEPS/dxvk-native"

say "vkd3d-utils (D3DCompile)"
if ldconfig -p 2>/dev/null | grep -q 'libvkd3d-utils\.so\.1'; then
    echo "installed system-wide"
elif [ -f "$DEPS/vkd3d/usr/lib/libvkd3d-utils.so.1" ]; then
    echo "in $DEPS/vkd3d"
elif command -v pacman >/dev/null; then
    # Arch's package, unpacked here rather than installed.
    command -v zstd >/dev/null || fail "missing: zstd (to unpack the vkd3d package)"
    curl -fL --progress-bar -o "$DEPS/vkd3d.pkg.tar.zst" \
        "https://geo.mirror.pkgbuild.com/extra/os/x86_64/vkd3d-$VKD3D_VERSION-1-x86_64.pkg.tar.zst"
    mkdir -p "$DEPS/vkd3d"
    tar --zstd -xf "$DEPS/vkd3d.pkg.tar.zst" -C "$DEPS/vkd3d" usr/lib
    rm -f "$DEPS/vkd3d.pkg.tar.zst"
    echo "in $DEPS/vkd3d"
else
    echo "not found: install your distribution's vkd3d-utils package"
    echo "(Debian/Ubuntu libvkd3d-utils1, Fedora vkd3d-utils). Without it the"
    echo "game still runs, drawing with the slow software rasteriser."
fi

say "Configuring"
cmake -S "$P" -B "$P/build-linux" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DXBOXRECOMP_DIR="$T" \
    -DXBOX_DXVK_NATIVE_INCLUDE="$DEPS/dxvk-native/usr/include/dxvk" >/dev/null

say "Compiling (10-30 minutes)"
ninja -C "$P/build-linux" -j "${JOBS:-1}"

say "Done"
echo "Play with ./run-linux.sh"
