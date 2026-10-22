#!/usr/bin/env bash
# Regenerate the C code for default.xbe plus every runtime-loaded module.
#
#   tools/regen_modules.sh          (run as your own user, not root)
#
# Runs from the xboxrecomp checkout next to this project. Steps:
#   1. tools/build_modules.py  -> build-modules/default_modules.xbe, seeds,
#                                 src/module_layout.c  (needs modules/ from
#                                 tools/extract_modules.py)
#   2. the usual pipeline on that XBE, with the module sections as extra code
#      sections and the engine + module seeds merged
#   3. tools.recomp with --exclude-manual on src/module_loader.c and
#      src/game_args.c (joined: it takes one file), so the functions they
#      wrap (sub_0011D860, the module loader; sub_000164F0, the command-line
#      parser) are emitted as sub_X_gen, and --manual-functions for the
#      modules' import thunk slots, which src/module_thunks.c defines
set -euo pipefail

P="$(cd "$(dirname "$0")/.." && pwd)"
T="$P/../xboxrecomp"
PY="${PY:-$HOME/.venvs/xboxrecomp/bin/python}"
X="$P/build-modules/default_modules.xbe"
L="$P/logs"
mkdir -p "$L"

"$PY" "$P/tools/build_modules.py" "$P/game/default.xbe"   # needs capstone

"$PY" - "$P" <<'EOF'
import json, sys
p = sys.argv[1]
seen, out = set(), []
for f in (f"{p}/seed_functions.json", f"{p}/modules/module_seeds.json"):
    for e in json.load(open(f)):
        a = int(e["start"], 16)
        if a not in seen:
            seen.add(a)
            out.append(e)
json.dump(out, open(f"{p}/build-modules/all_seeds.json", "w"), indent=1)
print(f"{len(out)} seeds")
EOF

SECS="D3D,D3DX,DSOUND,XPP,BINK,BINK16,BINK16M,BINK16MX,BINK16X2,BINK32,BINK32A,BINK32M,BINK32MX,BINK32X2,BINK4444,BINK5551,BINKYUY2"
SECS="$SECS,$(cat "$P/build-modules/extra_sections.txt")"

cd "$T"
"$PY" -m tools.xbe_parser "$X" --json "$P/build-modules/default_modules_analysis.json" > "$L/step1_xbe_modules.log" 2>&1
"$PY" -m tools.disasm "$X" -v --force --text-only --extra-sections "$SECS" \
    --seed-functions "$P/build-modules/all_seeds.json" > "$L/step3_disasm.log" 2>&1
"$PY" -m tools.func_id "$X" -v > "$L/step4_func_id.log" 2>&1
"$PY" -m tools.abi_analysis "$X" -v > "$L/step4.5_abi.log" 2>&1
# Names from the project's own map (tools/ghidra_names.json: the 196 MSVC
# runtime routines Ghidra recognised), not the toolkit's Ghidra export, which
# is local to the machine that ran Ghidra -- a fresh checkout has none.
"$PY" tools/ghidra_naming/merge_names.py --apply \
    --names-json "$P/tools/ghidra_names.json" > "$L/step4.6_merge.log" 2>&1
rm -f "$P"/src/recomp/gen/recomp_*.c
cat "$P/src/module_loader.c" "$P/src/game_args.c" > "$P/build-modules/manual_wraps.c"
"$PY" -m tools.recomp "$X" --all --split 250 --gen-dir "$P/src/recomp/gen" \
    --exclude-manual "$P/build-modules/manual_wraps.c" \
    --manual-functions "$P/build-modules/thunk_functions.json" > "$L/step5_recomp.log" 2>&1

grep -E 'Total functions' "$L/step3_disasm.log" || true
grep -E 'functions \(|unresolved call' "$L/step5_recomp.log" || true
