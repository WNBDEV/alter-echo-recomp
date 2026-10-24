#!/usr/bin/env bash
# Alter Echo installer for Linux: builds the game from your own copy of it.
#
#   bash AlterEchoInstaller.sh          asks for your disc image and a folder
#   bash AlterEchoInstaller.sh --iso <disc image or game folder> --dir <install folder>
#
#   --yes       install missing packages without asking (sudo still asks
#               for your password)
#   --no-menu   do not add Alter Echo to the applications menu
#
# It installs the build tools your system lacks with its package manager
# (pacman, apt, dnf or zypper; it shows the list and asks first), unpacks the
# project and the xboxrecomp toolkit carried in this file into the folder,
# and builds the native Linux version with the project's setup-linux.sh.
# Outside the folder it only adds the menu entry
# ~/.local/share/applications/alter-echo.desktop. Run it again over the same
# folder to update: the game files, saves and settings.ini stay, and an update
# that changes only the runtime takes minutes, not a full build.
set -euo pipefail

ISO="" DIR="" YES=0 MENU=1
while [ $# -gt 0 ]; do
    case "$1" in
        --iso)     ISO="${2:?--iso needs a path}"; shift 2 ;;
        --dir)     DIR="${2:?--dir needs a path}"; shift 2 ;;
        --yes)     YES=1; shift ;;
        --no-menu) MENU=0; shift ;;
        -h|--help) sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $1 (--help lists them)" >&2; exit 2 ;;
    esac
done

say()  { printf '\n== %s\n' "$*"; }
fail() { printf '\nINSTALL FAILED: %s\n' "$*" >&2; exit 1; }

[ "$(id -u)" -ne 0 ] || fail "run this as your own user, not root (it asks for sudo when it needs it)"

# A path typed or dragged into the terminal: drop the quotes or file:// a
# file manager adds, expand ~, make it absolute.
clean_path() {
    local p="$1"
    p="${p#"${p%%[![:space:]]*}"}"; p="${p%"${p##*[![:space:]]}"}"
    case "$p" in \'*\') p="${p:1:${#p}-2}" ;; \"*\") p="${p:1:${#p}-2}" ;; esac
    p="${p#file://}"
    case "$p" in "~") p="$HOME" ;; "~/"*) p="$HOME/${p:2}" ;; esac
    realpath -m -- "$p"
}

printf 'Alter Echo installer for Linux\n'
if [ -z "$ISO" ] || [ -z "$DIR" ]; then
    [ -t 0 ] || fail "no terminal to ask in: give --iso and --dir"
fi
if [ -z "$ISO" ]; then
    printf '\nYour copy of the game: the disc image (.iso) or the folder of its files.\n'
    printf 'Type the path, or drag the file into this window, then press Enter.\n'
    read -r -p "> " ISO
fi
ISO="$(clean_path "$ISO")"
[ -e "$ISO" ] || fail "$ISO does not exist"
if [ -z "$DIR" ]; then
    printf '\nInstall folder (Enter for ~/Games/Alter Echo):\n'
    read -r -p "> " DIR
    DIR="${DIR:-$HOME/Games/Alter Echo}"
fi
DIR="$(clean_path "$DIR")"
mkdir -p "$DIR" || fail "cannot create $DIR"
avail_kb="$(df -Pk "$DIR" | awk 'NR==2 {print $4}')"
[ "${avail_kb:-0}" -ge $((6 * 1024 * 1024)) ] ||
    fail "$DIR has $((avail_kb / 1024 / 1024)) GB free; the game needs about 6 GB"

exec > >(tee -a "$DIR/install.log") 2>&1
say "Alter Echo installer, $(date '+%Y-%m-%d %H:%M:%S')"
echo "install folder: $DIR"
echo "your copy: $ISO"

# ---- build tools ----------------------------------------------------------

PM=""
for m in pacman apt-get dnf zypper; do
    if command -v "$m" >/dev/null; then PM="$m"; break; fi
done

# What each missing piece is called in each package manager.
package() {
    case "$PM:$1" in
        pacman:python)    echo python ;;
        apt-get:python)   echo python3 python3-venv ;;
        *:python)         echo python3 ;;
        apt-get:ninja|dnf:ninja) echo ninja-build ;;
        *:ninja)          echo ninja ;;
        pacman:pkg-config) echo pkgconf ;;
        dnf:pkg-config)   echo pkgconf-pkg-config ;;
        *:pkg-config)     echo pkg-config ;;
        pacman:sdl2)      echo sdl2-compat ;;
        apt-get:sdl2)     echo libsdl2-dev ;;
        dnf:sdl2)         echo SDL2-devel ;;
        zypper:sdl2)      echo libSDL2-devel ;;
        pacman:epoxy)     echo libepoxy ;;
        apt-get:epoxy)    echo libepoxy-dev ;;
        *:epoxy)          echo libepoxy-devel ;;
        pacman:ssl)       echo openssl ;;
        apt-get:ssl)      echo libssl-dev ;;
        dnf:ssl)          echo openssl-devel ;;
        zypper:ssl)       echo libopenssl-devel ;;
        pacman:vulkan)    echo vulkan-icd-loader ;;
        dnf:vulkan)       echo vulkan-loader ;;
        *:vulkan)         echo libvulkan1 ;;
        *)                echo "$1" ;;
    esac
}

# Not grep -q: it stops at the first match, ldconfig dies of SIGPIPE and
# pipefail turns the match into a failure.
has_lib() { ldconfig -p 2>/dev/null | grep "$1" >/dev/null; }

missing_pieces() {
    local m=()
    if ! command -v python3 >/dev/null || ! python3 -c 'import venv, ensurepip' 2>/dev/null; then m+=(python); fi
    for t in git gcc cmake ninja curl pkg-config; do
        command -v "$t" >/dev/null || m+=("$t")
    done
    pkg-config --exists sdl2 2>/dev/null || m+=(sdl2)
    pkg-config --exists epoxy 2>/dev/null || m+=(epoxy)
    pkg-config --exists libcrypto 2>/dev/null || m+=(ssl)
    has_lib 'libvulkan\.so\.1' || m+=(vulkan)
    echo "${m[@]:-}"
}

install_packages() {     # package names...
    case "$PM" in
        pacman)  sudo pacman -S --needed $( [ $YES = 1 ] && echo --noconfirm ) "$@" ;;
        apt-get) sudo apt-get update && sudo apt-get install $( [ $YES = 1 ] && echo -y ) "$@" ;;
        dnf)     sudo dnf install $( [ $YES = 1 ] && echo -y ) "$@" ;;
        zypper)  sudo zypper $( [ $YES = 1 ] && echo --non-interactive ) install "$@" ;;
    esac
}

say "Checking the build tools"
read -r -a need <<< "$(missing_pieces)"
if [ ${#need[@]} -gt 0 ]; then
    [ -n "$PM" ] || fail "missing: ${need[*]}. Install them with your package manager and run this again."
    pkgs=()
    for n in "${need[@]}"; do read -r -a p <<< "$(package "$n")"; pkgs+=("${p[@]}"); done
    echo "Missing: ${need[*]}"
    echo "Installing with $PM: ${pkgs[*]}"
    if [ $YES = 0 ]; then
        [ -t 0 ] || fail "missing: ${pkgs[*]}. Install them, or run this again with --yes"
        read -r -p "Go ahead (asks for your password)? [Y/n] " ok
        case "${ok:-y}" in [Yy]*) ;; *) fail "the build tools are needed; nothing was changed" ;; esac
    fi
    install_packages "${pkgs[@]}" || fail "$PM could not install ${pkgs[*]}"
    read -r -a still <<< "$(missing_pieces)"
    [ ${#still[@]} -eq 0 ] || fail "still missing after installing: ${still[*]}"
fi
echo "ok"

# The HLSL compiler the native build draws with. Optional (without it the
# game falls back to a slow software renderer) and not packaged everywhere,
# so trying is enough. On Arch setup-linux.sh fetches it, no sudo needed.
if ! has_lib 'libvkd3d-utils\.so\.1' && [ -n "$PM" ] && [ "$PM" != pacman ]; then
    case "$PM" in
        apt-get) cands="libvkd3d-utils1" ;;
        dnf)     cands="vkd3d-utils libvkd3d-utils" ;;
        zypper)  cands="libvkd3d-utils1" ;;
    esac
    say "Installing vkd3d-utils (the shader compiler; optional)"
    for c in $cands; do install_packages "$c" && break; done ||
        echo "not installed: the game will run, but draw with the slow software renderer"
fi

# ---- the project ----------------------------------------------------------

say "Unpacking the project and the toolkit"
# Into a folder of its own first, then only what changed is copied in: an
# update over an earlier install leaves the files that stayed the same as they
# were, so the build compiles only what changed, and the game's translation is
# kept if nothing it depends on changed (tools/setup_game.py).
stage="$DIR/.unpacking"
rm -rf "$stage"
mkdir -p "$stage" || fail "could not create $stage"
line="$(awk '/^__ARCHIVE_BELOW__$/ {print NR + 1; exit}' "$0")"
tail -n +"$line" "$0" | tar xzf - -C "$stage" || fail "could not unpack into $stage"
update=0 old_installer=0
if [ -d "$DIR/alter-echo-recomp/src/recomp/gen" ]; then
    update=1
    [ -f "$DIR/alter-echo-recomp/src/recomp/gen/.translation" ] || old_installer=1
    python3 "$stage/alter-echo-recomp/tools/setup_game.py" \
        --stamp-old "$DIR/alter-echo-recomp" "$DIR/xboxrecomp" || true
fi
python3 "$stage/alter-echo-recomp/tools/setup_game.py" --sync "$stage" "$DIR" ||
    fail "could not copy the files into $DIR"
rm -rf "$stage"
if [ $old_installer = 1 ]; then
    # The installers up to 1.1.0 unpacked with tar, which gives each file the
    # time the installer was made: an update then left the earlier runtime's
    # compiled objects looking newer than its new sources, and kept them. So
    # the runtime is compiled again, once; the game's translated code stays.
    find "$DIR/alter-echo-recomp/build-linux" \( -name '*.o' -o -name '*.a' \) \
        -not -path '*/src/recomp/gen/*' -delete 2>/dev/null || true
fi
echo "into $DIR"

jobs=1
[ "$(awk '/^MemTotal/ {print $2}' /proc/meminfo)" -ge $((12 * 1024 * 1024)) ] && jobs=2
if [ $update = 1 ]; then
    say "Updating the game (a few minutes if only the runtime changed, else 25-45)"
else
    say "Building the game (25-45 minutes; $jobs file$( [ $jobs = 2 ] && echo s) at a time)"
fi
cd "$DIR/alter-echo-recomp"
JOBS=$jobs ./setup-linux.sh "$ISO" || fail "building the game stopped; the messages above say why. Log: $DIR/install.log"
run="$DIR/alter-echo-recomp/run-linux.sh"

if [ $MENU = 1 ]; then
    apps="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
    mkdir -p "$apps"
    # Exec takes the path quoted, with " ` $ \ escaped, then the file's own
    # string escaping on top: hence two backslashes.
    exec_path="$(printf '%s' "$run" | sed 's/[\\"`$]/\\\\&/g')"
    cat > "$apps/alter-echo.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Alter Echo
Comment=Alter Echo, recompiled for PC
Exec="$exec_path"
Path=$DIR/alter-echo-recomp
Icon=applications-games
Terminal=false
Categories=Game;
EOF
    command -v update-desktop-database >/dev/null && update-desktop-database "$apps" 2>/dev/null || true
fi

say "Installed"
[ $MENU = 1 ] && echo "Start Alter Echo from your applications menu, or run:" || echo "Start Alter Echo with:"
printf '  "%s"\n' "$run"
echo "Settings: $DIR/alter-echo-recomp/settings.ini (made on first run)"
echo "To remove it: delete \"$DIR\"$( [ $MENU = 1 ] && echo " and $apps/alter-echo.desktop")"
exit 0
__ARCHIVE_BELOW__
