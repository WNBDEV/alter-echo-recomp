#!/usr/bin/env bash
# Boot levels straight from the game's command line and record how each fares.
#
#   tools/level_sweep.sh                  every area in the game's clf/ folder
#   tools/level_sweep.sh L05_A1 L06_A2    just these
#   SETTLE=60 tools/level_sweep.sh ...    seconds to watch each level (40)
#
# Run it as yourself from the desktop session, with the Wine build built. Each
# level is one run of ./run.sh with RECOMP_ARGS="<level> -bypass"
# (src/game_args.c), muted and without rumble, stopped SETTLE seconds after
# its .clf opens. Results go to logs/sweep-<time>/: summary.tsv, plus each
# level's log and last frame (<level>.png, or .bmp without ImageMagick; the
# console-sized picture).
#
# summary.tsv columns:
#   level    the area
#   result   ok (still running when stopped), died (exited by itself),
#            no-load (its .clf never opened within LOAD_WAIT seconds)
#   load_s   seconds from launch to the .clf opening
#   fps      the last fps the GPU reported
#   icall    [ICALL] lines: calls to addresses with no translated code
#   crash    [CRASH] lines
#   unimpl   [UNIMPL] addresses beyond the five every run reaches
#   methods  distinct GPU methods the backend does not handle (149 is normal)
#   max_c    hottest CPU package temperature seen, if `sensors` is installed
set -uo pipefail

cd "$(dirname "$0")/.."
SETTLE="${SETTLE:-40}"
LOAD_WAIT="${LOAD_WAIT:-120}"
export WINEPREFIX="$HOME/.local/share/wineprefixes/alter-echo-recomp"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

if [ $# -gt 0 ]; then
    levels=("$@")
else
    mapfile -t levels < <(ls "game/ODB Data/ODB MEDIA/clf" | grep -i '^L.*\.clf$' |
                          sed 's/\.clf$//I' | sort -f)
fi

# What every run reaches (the kernel's interrupt and port no-ops).
base_unimpl='0x001B1B88|0x001B1B99|0x0016F79D|0x0016DAC0|0x00170759'

out="logs/sweep-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out"
printf 'level\tresult\tload_s\tfps\ticall\tcrash\tunimpl\tmethods\tmax_c\n' > "$out/summary.tsv"
echo "${#levels[@]} levels -> $out"

pkg_temp() {
    command -v sensors >/dev/null &&
        sensors 2>/dev/null | awk '/^Package id 0:/ {gsub(/[+°C]/, "", $4); print int($4); exit}'
}

stop_game() {
    kill -- -"$1" 2>/dev/null
    sleep 2
    kill -9 -- -"$1" 2>/dev/null
    wineserver -k 2>/dev/null
    sleep 2
}

for lv in "${levels[@]}"; do
    log="$out/$lv.log"
    RECOMP_ARGS="$lv -bypass" RECOMP_AUDIO_MUTE=1 RECOMP_RUMBLE=0 \
    RECOMP_FB_DUMP="$out/$lv.bmp" RECOMP_FB_WINDOW_DUMP_EVERY=30 \
        setsid ./run.sh > "$log" 2>&1 < /dev/null &
    pgid=$!
    start=$SECONDS result= load_s= max_c=$(pkg_temp)
    while [ -z "$result" ]; do
        if ! kill -0 "$pgid" 2>/dev/null; then
            result=died
        elif [ -z "$load_s" ] && grep -qi "clf\\\\$lv\\.clf" "$log"; then
            load_s=$((SECONDS - start))
        elif [ -n "$load_s" ] && [ $((SECONDS - start - load_s)) -ge "$SETTLE" ]; then
            result=ok
        elif [ -z "$load_s" ] && [ $((SECONDS - start)) -ge "$LOAD_WAIT" ]; then
            result=no-load
        else
            t=$(pkg_temp); [ -n "$t" ] && [ "$t" -gt "${max_c:-0}" ] && max_c=$t
            sleep 2
        fi
    done
    stop_game "$pgid"
    # run.sh tees the same stderr into logs/run-<time>.txt: keep one copy.
    dup=$(sed -n 's/^stderr -> //p' "$log" | head -1)
    [ -n "$dup" ] && rm -f "$dup"
    # The last frame as a PNG, when ImageMagick is there (each BMP is 900 KB).
    if command -v magick >/dev/null && [ -f "$out/$lv.bmp" ]; then
        magick "$out/$lv.bmp" "$out/$lv.png" && rm -f "$out/$lv.bmp"*
    fi

    fps=$(grep -oE '^\[GPU\] [0-9.]+ fps' "$log" | tail -1 | awk '{print $2}')
    icall=$(grep -c '^\[ICALL\]' "$log")
    crash=$(grep -c '^\[CRASH\]' "$log")
    unimpl=$(grep '^\[UNIMPL\]' "$log" | grep -oE 'at 0x[0-9A-F]+' | sort -u |
             grep -vcE "$base_unimpl")
    methods=$(grep -oE '\(([0-9]+) distinct\)' "$log" | tail -1 | tr -dc 0-9)
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$lv" "$result" "${load_s:--}" \
        "${fps:--}" "$icall" "$crash" "$unimpl" "${methods:--}" "${max_c:--}" |
        tee -a "$out/summary.tsv"
done

column -t "$out/summary.tsv"
