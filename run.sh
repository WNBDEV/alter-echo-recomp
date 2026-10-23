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
# XDG_RUNTIME_DIR points at the owner's session: without it Wine cannot reach
# PipeWire, falls back to raw ALSA, and takes the speakers from the desktop
# for as long as the game runs.
if [ "$(id -u)" -eq 0 ]; then
    owner="$(stat -c %U "$0")"
    exec sudo -u "$owner" -H \
        --preserve-env=DISPLAY,XAUTHORITY,WAYLAND_DISPLAY,WINEDEBUG \
        XDG_RUNTIME_DIR="/run/user/$(id -u "$owner")" "$0" "$@"
fi

cd "$(dirname "$0")"

# What this build needs to get from boot to gameplay, on unless already set
# (any of them can be overridden from the environment, 0 to turn one off):
#   EXT_VMA      address space for the runtime-loaded game modules
#   AC97_READY   audio hardware present (DirectSound initialises)
#   (the audio DSP's command word is completed by the toolkit, found from
#   the APU's registers; RECOMP_DSP_ACK=<addr> is no longer needed)
#   USB          the controller arrives over the emulated USB port
#   KEYBOARD     the keyboard and mouse drive pad 1 (see below)
#   IRQL_LOCK    DPCs and the game thread do not race (controller setup)
#   PB_EXEC      the GPU command executor -- without it nothing is drawn
#   TITLE_KEVENTS, VBLANK   events and the frame clock D3D waits on
#   FB_WINDOW    the window that shows the frames
# Without these the game runs but draws nothing and opens no window.
#
# Keys and mouse (window focused; click in it to use the mouse): WASD move,
# mouse camera, the rest and how to change them in settings.ini. A
# controller works as well, at the same time.
#   GPU          draw with the graphics card (Direct3D 11 via DXVK); 0 falls
#                back to the software rasteriser
#   HEAP_VFREE   video buffers the game frees go back to the heap. Without
#                it every video leaked them, and some minutes into play a
#                tutorial video found no memory and the game hung. (The full
#                RECOMP_HEAP_RECLAIM stalls this game before its first frame.)
#   CONTIG_FREE  the same for contiguous memory: freed blocks are reused.
#                Each full-screen video leaked ~4 MB of the 64 MB window, so
#                about a dozen in a session would have used it up; with it
#                the window stays flat (17.7 MB after five attract plays, 35
#                without). The "20 fps natively" once blamed on it was
#                Wayland throttling a window that was not on screen.
for v in EXT_VMA AC97_READY USB KEYBOARD IRQL_LOCK PB_EXEC TITLE_KEVENTS \
         VBLANK FB_WINDOW GPU HEAP_VFREE CONTIG_FREE; do
    eval ": \"\${RECOMP_$v:=1}\""
    export "RECOMP_$v"
done

# settings.ini (from settings.example.ini on first run): friendly names for
# the switches below. The environment wins over the file, the file over the
# defaults.
SETTINGS=settings.ini
if [ ! -f "$SETTINGS" ] && [ -f settings.example.ini ]; then
    cp settings.example.ini "$SETTINGS"
fi
if [ -f "$SETTINGS" ]; then
    while IFS='=' read -r key val || [ -n "$key" ]; do
        key="${key%%#*}"; key="$(printf '%s' "$key" | tr -d ' \t\r')"
        val="${val%%#*}"; val="$(printf '%s' "$val" | tr -d ' \t\r')"
        [ -n "$key" ] && [ -n "$val" ] || continue
        case "$key" in
            resolution_scale)      var=RECOMP_SCALE ;;
            fullscreen)            var=RECOMP_FULLSCREEN ;;
            rumble)                var=RECOMP_RUMBLE ;;
            reverb)                continue ;;   # see settings.example.ini history:
                                                   # the game sends nothing to the
                                                   # console's reverb, so it has none
            direct_render_targets) var=RECOMP_RT_DIRECT ;;
            texture_filtering)     var=RECOMP_TEXFILTER ;;
            sharpen)               var=RECOMP_SHARPEN ;;
            input)                 var=RECOMP_INPUT ;;
            show_fps)              var=RECOMP_SHOW_FPS ;;
            mouse_sensitivity)     var=RECOMP_MOUSE_SENS ;;
            mouse_invert_y)        var=RECOMP_MOUSE_INVERT ;;
            camera_flip_x)         var=RECOMP_CAMERA_FLIP_X ;;
            camera_flip_y)         var=RECOMP_CAMERA_FLIP_Y ;;
            mouse_deadzone)        var=RECOMP_MOUSE_DEADZONE ;;
            bind_*)                var="RECOMP_BIND_$(printf '%s' "${key#bind_}" | tr a-z A-Z)" ;;
            *) echo "settings.ini: unknown setting '$key' (ignored)" >&2; continue ;;
        esac
        [ -n "${!var:-}" ] || export "$var=$val"
    done < "$SETTINGS"
fi

# Rendering and memory, each overridable the same way:
#   RECOMP_SCALE=2      render at 2x the console's 640x480 (1280x960); 1 for
#                       the original, 3 or 4 for more if the GPU keeps up
#   RECOMP_STACK_KB     the main-thread stack region. 8 MB was the toolkit's
#                       default and came out of the game's heap: levels left
#                       285 KB free and a move video then crashed or hung the
#                       game. The thread uses ~20 KB (it asks for 256 KB).
#   RECOMP_RT_DIRECT=1  effects that sample a frame they rendered (sky, glow)
#                       read it on the GPU, not through a readback each frame
export RECOMP_SCALE="${RECOMP_SCALE:-2}"
export RECOMP_STACK_KB="${RECOMP_STACK_KB:-2048}"
export RECOMP_RT_DIRECT="${RECOMP_RT_DIRECT:-1}"

# The camera turns and tilts the way the stick or mouse goes (the console
# swings it the other way; camera_flip_x / camera_flip_y = 0 in settings.ini
# keep that).
export RECOMP_CAMERA_FLIP_X="${RECOMP_CAMERA_FLIP_X:-1}"
export RECOMP_CAMERA_FLIP_Y="${RECOMP_CAMERA_FLIP_Y:-1}"

# Tells the game this script has applied the settings and opens the log, so
# it does not do that itself (it does when started directly: src/launch.c).
export RECOMP_LAUNCHER=run.sh

# Raster threads, for the software rasteriser (RECOMP_GPU=0) only: there the
# GPU is emulated on the CPU, and every thread given to it is a core kept busy. Four is the measured sweet spot on a 4-core/8-thread
# i7-4790K: 7.3 fps at ~3 cores of load, against 8.6 fps at ~5 with seven
# threads, which drove the package to 99-100 C. More frames, more heat:
# RECOMP_RASTER_THREADS=7 ./run.sh.
export RECOMP_RASTER_THREADS="${RECOMP_RASTER_THREADS:-4}"

export WINEPREFIX="$HOME/.local/share/wineprefixes/alter-echo-recomp"
export WINEDEBUG="${WINEDEBUG:--all}"
# No Wine Mono or Gecko: after a Wine update the prefix is refreshed on the
# next launch, and that would otherwise stop on a dialog offering them.
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-mscoree,mshtml=}"

mkdir -p logs
log="logs/run-$(date +%Y%m%d-%H%M%S).txt"
echo "stderr -> $log"

# Keep the newest ten logs of this kind (one is written per launch, and they
# were never removed: a player's folder reached gigabytes).
ls -1t logs/run-*.txt 2>/dev/null | tail -n +10 | while IFS= read -r old; do
    rm -f -- "$old"
done || true

# The log opens with the system it ran on, so a report needs nothing else:
# no host or user names, just the hardware, the software and the settings.
{
    echo "== Alter Echo recomp, $(date '+%Y-%m-%d %H:%M:%S')"
    echo "build: $(git describe --always --dirty 2>/dev/null || echo unknown);" \
         "toolkit $(git -C ../xboxrecomp describe --always --dirty 2>/dev/null || echo unknown)"
    echo "os: $(sed -n 's/^PRETTY_NAME=//p' /etc/os-release 2>/dev/null | tr -d '"');" \
         "kernel $(uname -r); session ${XDG_SESSION_TYPE:-unknown}"
    echo "cpu: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//')," \
         "$(nproc) threads; ram $(awk '/MemTotal/ {printf "%.1f GB", $2 / 1048576}' /proc/meminfo)"
    echo "gpu: $(lspci 2>/dev/null | grep -Ei 'vga|3d|display' | sed 's/^[^ ]* //' | paste -sd ';' -)" \
         "$(sed -n 's/.*Kernel Module *\([0-9.]*\).*/(nvidia \1)/p' /proc/driver/nvidia/version 2>/dev/null)"
    echo "wine: $(wine --version 2>/dev/null || echo unknown)"
    echo "settings: $(env | grep '^RECOMP_' | sort | paste -sd ' ' -)"
    echo "=="
} > "$log"

# RECOMP_EXE runs another build (a test link) without replacing this one.
status=0
wine "${RECOMP_EXE:-build-win/alter_echo_recomp.exe}" "$@" 2> >(tee -a "$log" >&2) || status=$?
echo "Log saved to $log -- attach it to a report (README: Reporting a problem)." >&2
exit "$status"
