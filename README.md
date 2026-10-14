# Alter Echo — static recompilation

Alter Echo (Xbox, 2003, Title ID `0x5451000F`) recompiled to run on PC with
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp), built as a Windows `.exe`
with MinGW and run on Linux under Wine + DXVK.

The decision notes behind this (emulate vs port, measurements, pros and cons)
are in `~/Games/Xbox/alter-echo-notes.md`.

## Layout

| Path | What |
|---|---|
| `src/main.c` | Host entry point: loads the XBE, starts the kernel layer, calls the game. Entry point and paths filled in |
| `src/recomp_manual.c` | Hand-written overrides for functions that need fixing |
| `src/recomp/gen/` | **Generated** C from the pipeline. Don't hand-edit; regenerate instead (git-ignored) |
| `game/` | Symlink to the unpacked disc (`~/Games/Xbox/AlterEcho-files`) |
| `logs/` | Pipeline step logs, and run logs from `run.sh` |
| `build.sh` | Cross-compile with MinGW into `build-win/` |
| `run.sh` | Run under Wine in the `~/.local/share/wineprefixes/alter-echo-recomp` prefix (DXVK installed) |

The toolkit lives next door in `~/Projects/xboxrecomp` (the CMake file expects
it at `../xboxrecomp`), with its Python environment in `~/.venvs/xboxrecomp`.

## Build and run

```bash
./build.sh          # JOBS=2 ./build.sh if memory runs short
./run.sh            # crash output also saved to logs/run-<timestamp>.txt
```

**It will crash the first time.** That's expected: see "Step 8: Debug
Iteratively" in `../xboxrecomp/docs/GETTING_STARTED.md`. The crash report names
the guest function (`sub_XXXXXXXX`) and the recent indirect-call targets.

## Regenerating the C code

Run from `~/Projects/xboxrecomp`, with `PY=~/.venvs/xboxrecomp/bin/python` and
`XBE=~/Games/Xbox/AlterEcho-files/default.xbe`:

```bash
$PY -m tools.xbe_parser $XBE --json ~/Games/Xbox/AlterEcho-files/alterecho_analysis.json
$PY -m tools.disasm $XBE -v --force --text-only --extra-sections \
    D3D,D3DX,DSOUND,XPP,BINK,BINK16,BINK16M,BINK16MX,BINK16X2,BINK32,BINK32A,BINK32M,BINK32MX,BINK32X2,BINK4444,BINK5551,BINKYUY2 \
    --seed-functions ~/Projects/alter-echo-recomp/seed_functions.json
$PY -m tools.func_id $XBE -v
$PY -m tools.abi_analysis $XBE -v
$PY tools/ghidra_naming/merge_names.py --apply      # re-apply names; disasm wipes them
$PY -m tools.recomp $XBE --all --split 250 --gen-dir ~/Projects/alter-echo-recomp/src/recomp/gen
```

**Why those disassembly flags:** the game calls into Microsoft's libraries
(DirectSound, Direct3D, the controller library XPP, the Bink video decoder),
which live in their own sections, so `--text-only` alone leaves those calls
unresolved. But plain "all sections" mode is worse: Alter Echo's XBE marks
`.rdata` and `.data` executable, so they get decoded as code and the C++
vtables in them are never scanned. That lost ~430–590 real functions. DOLBY is
left out on purpose: it's code for the sound chip's DSP, not the x86 CPU.
(Only the first of the four `BINKYUY2` sections is picked up by name.)

The Ghidra export that `merge_names.py` reads is cached in
`tools/ghidra_naming/export/`. To redo it: the toolkit's `run_ghidra.sh` is
Windows-only, so the Linux equivalent is in `logs/README-ghidra.txt`.

## Pipeline results (14 Oct 2026)

| Step | Result |
|---|---|
| Parse | Entry `0x000561A2`, 28 sections, 111 kernel imports, XDK 5233 |
| Disassemble | 8,359 functions, 462,961 instructions, 84% reachable from known calls |
| Classify | 3,177 C++ vtable methods found; 10 C runtime functions by signature |
| Calling conventions | All 8,359 analysed; 1,630 `thiscall` (C++ methods) |
| Ghidra names | 196 recovered (no RTTI, nothing demangled); 107 applied |
| Generate C | **8,356 / 8,356 functions translated, 0 failed.** 1,090,984 lines of C in 34 chunks (43 MB; largest file 2.5 MB). 506 call targets that weren't detected as functions became stubs |
| Health report (`tools/doctor.py`, saved in `logs/doctor.txt`) | 87 instructions the translator doesn't handle: `in` ×29, `out` ×7, `insb` ×6, `aas` ×6, `arpl` ×5, `aad` ×4, `pushfd` ×3 and ~20 rarer ones. 0 unresolved indirect-call targets at this stage |

## Fixes so far

| # | Problem | Fix | Where |
|---|---|---|---|
| 1 | Build: `implicit declaration of recomp_dispatch_init` (GCC 14+ error, MSVC only warns) | Declared it in `src/main.c` | this project |
| 2 | Run 1: game booted, then quit to the dashboard with "invalid hard disk" before drawing anything | Alter Echo asks for a **64 KB** cache-drive cluster size (XBE init flags bits 30–31); the toolkit always reported 16 KB, and opened the cache volume's root as a raw image. Now the cache volume root maps to the `Cache` folder and reports the title's cluster size | `../xboxrecomp/src/kernel/kernel_path.c`, `kernel_file.c`, `kernel.h` (uncommitted local change in the toolkit clone; `git diff` there shows it) |
| 3 | Run 2: calls into DirectSound code (`0x0017BEF0`…) unresolved, then a crash | Disassemble the library code sections too (see flags above) | pipeline |
| 4 | Link: 250 `undefined reference to sub_…`, all from one chunk | A build was killed mid-compile and left `recomp_0012.c.obj` at 0 bytes but newer than its source, so ninja skipped it. Deleted it and rebuilt. If a link suddenly can't find a whole chunk's functions, check for 0-byte `.obj` files in `build-win/CMakeFiles/` | build dir |
| 5 | Run 3: crash in the controller library gone; one unresolved indirect call to `0x001AD7BD` (USB host setup in XPP, only reached by function pointer), then silence until timeout | Fed run-time-observed addresses back as seeds: `tools.seed_from_log` → `seed_functions.json` (also picked up thread start context `0x0005612E`), then `tools.disasm ... --seed-functions seed_functions.json` | pipeline, `seed_functions.json` |
| 6 | Run 4: no crash, no window, no file loads. Watchdog (`RECOMP_WATCHDOG_SECS`) showed the main loop (`sub_00017C10`) running state 1, whose input update (`sub_00052610`) waits for a connected pad with START held. No pad can ever appear: the toolkit's USB host model (`src/usb/ohci.c`) was linked but never initialised or given its register faults | `src/main.c` now calls `xbox_OhciInit()` and routes faults in its range to `xbox_OhciHandleMmio`, like the APU. Only active with `RECOMP_USB=1` | `src/main.c` |
| 7 | Run 5 (`RECOMP_USB=1`): USB controller up, pad arrives, then crash in the game's USB interrupt handler: `undecoded access at +0x00C: 44 23 04 11` | The MMIO decoder handled `and [mem], reg` but not the register-destination forms GCC emits when it folds a register read into an op (`and r8d,[rcx+rdx]`). Added `OR/AND/XOR r, r/m` and `CMP r, r/m`, plus tests | `../xboxrecomp/src/platform/mmio_decode.h`, `tests/mmio_decode/test_main.c` (uncommitted in the toolkit) |
| 8 | Run 6: interrupts reach the game, enumeration starts (4 setup packets), then stalls on an unresolved indirect call to `0x001AEBD3` (XPP) | Seeded it, plus the three other code gaps in XPP that no detected function covers (`0x001AE2B7`, `0x001AE5D5`, `0x001B1371`; each starts right after a `ret N`) | `seed_functions.json` |
| 9 | Run 7: pad fully enumerated and XAPI's connected mask shows it (slot 2), but `XInputOpen` returns NULL, last error `0x48F` (device not connected), and the game never retries | A race the console can't have. XPP's enumeration DPC sets the connected bit and *then* marks the device entry ready; the DPC runs on a host thread alongside the game, which saw the bit in between. Added `RECOMP_IRQL_LOCK`: crossing into DISPATCH_LEVEL takes one process-wide lock (bounded waits, so a leaked raise costs a delay, not a deadlock). With it the pad opens and ~100 reports/s flow | `../xboxrecomp/src/kernel/kernel_hal.c` (uncommitted in the toolkit) |
| 10 | Run 8: input works, presses arrive, still nothing: state 1 forever, no D3D, no file loads | State 1's per-frame update (`sub_0001D4A0`) is `jmp sub_00025F20`, the **whole engine init**, which ends by requesting state 2. It was never detected as a function, so it was an "unresolved" stub that just returns. Seeded it and 70 other direct call/jmp targets that lay in code no function covered and started after a `ret`/`jmp`/padding (most 16-byte aligned). The 187 unresolved targets *inside* existing functions are left alone for now | `seed_functions.json` |

| 11 | Run 9: engine init runs (state 1 → 2 → 3), shaders and UI textures load, D3D sends draws; then a NULL function pointer called in a hot loop right after an unresolved indirect call to `0x0001FBA0` | `0x0001FBA0` sat after `ret` + padding *inside* `sub_0001FB80`: the detector had merged two functions. Seeded 38 such verified boundaries (ret checked as a real instruction boundary; jump tables excluded) plus 5 state-machine handlers from the tables at `0x1CA380`/`0x1CA420`/`0x1CA4C0` that were never detected | `seed_functions.json` |
| 12 | Run 10 (`RECOMP_PB_EXEC=1`): 127 draws, 0 flips, game stuck in D3D `BlockOnTime` (`0x16C9D0`) waiting for GPU time `[dev+0x30]` to catch up | The executor dropped `SET_CONTEXT_DMA_SEMAPHORE` / `SET_SEMAPHORE_OFFSET` / `BACK_END_WRITE_SEMAPHORE_RELEASE`, which is how the GPU writes D3D's clock. Implemented them, with a real RAMHT → DMA-object lookup (the semaphore object is based at physical `0x2000`, not 0) | `../xboxrecomp/src/kernel/nv2a_pb_exec.c` |
| 13 | Run 11: clock advances, then D3D sleeps on an event (`dev+0x196C`) after queuing NOP(5) | NOP with a non-zero parameter is a software method: PGRAPH traps it and interrupts the CPU, and D3D's ISR sets the event. Added that (trap registers + PGRAPH error interrupt, delivered from the timer thread, stream stalls until handled). Needs `RECOMP_TITLE_KEVENTS=1` for the wait to block | `../xboxrecomp/src/kernel/kernel_bridge.c`, `nv2a_pb_exec.c` |
| 14 | Run 12: every NV2A interrupt "ISR not callable" | D3D's ISR (`0x0016F530`) is only reached through the interrupt object, so never detected. Seeded it and six other uncovered D3D functions | `seed_functions.json` |
| 15 | Run 13: software methods "declined" | D3D's ISR only claims and queues a DPC; the DPC's PGRAPH handler (`0x0016FD30`) does the work, and it zeroes `PGRAPH_FIFO` on entry and sets it to 1 on exit. Delivery now waits for that 1 and clears the status bits only then (the poll thread that zeroes them is the one stalled on the method, so it can't race) | `../xboxrecomp/src/kernel/kernel_bridge.c` |
| 16 | Run 14: game reaches state 9, presents frames, 2,084 triangles rasterised, **0 pixels written** | `RECOMP_PB_ZERO_DIAG` (new) showed alpha test on, and `RECOMP_FRAME_TRACE` showed vertex `w = inf`: D3D's XYZRHW path gives w = 1/rhw and the text has rhw = 0, so 1/w = 0, texcoords 0/0 = NaN, and NaN alpha failed the test. w of 0 or infinity is now treated as 1 for perspective, as xemu does. **First rendered frame: the copyright screen** (`logs/first-frame.png`) | `../xboxrecomp/src/kernel/nv2a_pb_exec.c` |

## Where it stands (15 Oct 2026, ~00:45)

Boots, runs its engine init, loads shaders and textures, reads a (virtual)
controller, and draws its copyright screen. It then presents only ~2 frames a
minute and doesn't move past that screen. Main thread mostly idle;
`KeSetEvent` is called ~6,500/s. Likely cause: frame pacing hangs on vblank,
and the NV2A poll loop's `NV2A_ACK` table zeroes `PMC_INTR_0` / `PCRTC_INTR_0`
continuously, racing the vblank interrupts `RECOMP_VBLANK` now delivers (its
comment predates any interrupt being raised). Proper fix: real
write-one-to-clear for those registers, or clearing them after the DPC has run,
the way fix 15 does for PGRAPH. Line 5 of the copyright text is also missing
its first words: possibly the second font page, not yet looked at.

Also still open: 187 unresolved call targets that land *inside* detected
functions (`recomp_stubs_unresolved.c`); `seed_text_functions.json` is unused.

### Running with a virtual controller

`run.sh` passes only display variables through `sudo`, so to set `RECOMP_*`
variables run it as your own user (not as root):

```bash
RECOMP_USB=1 RECOMP_IRQL_LOCK=1 ./run.sh               # pad driven by a real controller via XInput
RECOMP_USB=1 RECOMP_IRQL_LOCK=1 RECOMP_PAD_SCRIPT=3000:start,6000:a ./run.sh   # scripted presses, ms after enumeration
RECOMP_WATCHDOG_SECS=20 ./run.sh                       # dump guest stack if it stops making progress
```

The full set for the current bring-up (graphics through the toolkit's software
rasteriser, frames dumped to `logs/frame.bmp`):

```bash
RECOMP_USB=1 RECOMP_IRQL_LOCK=1 RECOMP_PB_EXEC=1 RECOMP_TITLE_KEVENTS=1 RECOMP_VBLANK=1 \
RECOMP_FB_WINDOW=1 RECOMP_FB_DUMP=logs/frame.bmp RECOMP_FB_WINDOW_DUMP_EVERY=120 ./run.sh
```

### What to expect first

- **Most of the 87 odd instructions are probably data decoded as code.** Opcodes like `aas`, `arpl`, `salc` and `bound` are almost never emitted by a 2003 compiler, and the spot checks I did sit at addresses the main disassembly doesn't line up with. They'll only matter if execution actually reaches them.
- **One is definitely real:** `sub_00011160` runs `pushfd`/`popfd` + `cpuid` to check for MMX support (`test edx, 0x800000`). If the first crash lands there, the fix is a manual override in `src/recomp_manual.c` that reports MMX as present.
- **The `in`/`out` cluster in `sub_0003EBE0`** might be real hardware port access. Worth looking at if a crash or hang points there.
