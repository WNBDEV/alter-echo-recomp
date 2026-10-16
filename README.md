# Alter Echo for PC — static recompilation

Alter Echo (Outrage Games / THQ, Xbox, 2003) running natively on a PC: the
game's own Xbox program translated to C and compiled, with the console's
hardware -- graphics chip, sound, controller, kernel -- reimplemented by the
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) toolkit. Not an
emulator of the CPU: the game's code runs as ordinary PC code.

**No game files are included.** You need your own copy of the North American
(NTSC) release, as a disc image or its extracted files. The build script
checks it is that exact release.

What works: the whole game as far as it has been played (the first missions
start to finish), at a steady 60 fps, rendered at 2x the original resolution
or more, fullscreen, with controller rumble and correct audio. Rendering runs
on the GPU (Direct3D 11, through DXVK on Linux).

## What you need

- Linux with Wine and DXVK (the game is built as a Windows program; a native
  Linux build is in progress, below), or Windows (untested so far)
- To build: python3, cmake, ninja, MinGW-w64 (`x86_64-w64-mingw32-gcc`), and
  the xboxrecomp toolkit checked out next to this folder (`../xboxrecomp`)
- 20-40 minutes for the first build and several GB of free RAM
- A controller (Xbox 360 or compatible) or the keyboard

## Build from your copy

```bash
./setup.sh "/path/to/Alter Echo (USA).iso"      # or the extracted game folder
./run.sh
```

`setup.sh` checks your copy, unpacks it if it is a disc image, extracts the
game's code modules, translates everything to C, compiles it and prepares the
Wine prefix. `CHECK_ONLY=1 ./setup.sh <copy>` checks your copy and tools
without building. The C code it generates comes from your copy; keep it to
yourself.

## Playing

`./run.sh` starts the game. Settings live in `settings.ini` (created from
`settings.example.ini` on first run): resolution scale, fullscreen, rumble.
While playing, **Alt+Enter** or **F11** toggles fullscreen; the picture
keeps the console's 4:3 with black bars. An Xbox 360 pad works directly
(with rumble); the keyboard works too -- arrows d-pad, Enter START,
Z/X/A/S = A/B/X/Y, numpad 8/4/2/6 left stick, I/J/K/L right stick, 1/3
triggers, Q/E white/black, Backspace BACK. Saves go where the console keeps
them (`UDATA/` in the game folder).

## Native Linux build (experimental)

The same game compiled as a Linux program -- no Wine. It needs the C code
`setup.sh` generated, so run that first, then:

```bash
./setup-linux.sh      # fetches DXVK Native + vkd3d-utils into build-linux/deps, compiles
./run-linux.sh        # same settings.ini and keys as run.sh
```

Window, sound and controller go through SDL2. Graphics use the same Direct3D
11 renderer as the Wine build, on DXVK Native (DXVK built as Linux libraries,
talking to Vulkan directly); shaders compile with vkd3d-utils, the compiler
Wine's own uses, so `shader-cache/` is shared between the two builds. Neither
library is installed system-wide, and if either is missing the game falls
back to the software renderer (correct, but far slower). Saves are the same
`UDATA/` folder the Wine build uses.

Status: see "Native Linux" under Development notes. `run.sh` remains the
way to play.

## Known limits

- Played through the first missions; later levels may show problems not yet
  seen. Logs are in `logs/`.
- 60 fps is the cap, as on the console (the game is built around it).
- The sound is the console's dry mix: the game uses no reverb or effects.

---

# Development notes

The rest of this file is the working log of the port: what failed, how it
was found and what fixed it. The decision notes behind the project
(emulate vs port, measurements) are in `~/Games/Xbox/alter-echo-notes.md`.

## Native Linux (16 Oct 2026, evening)

`build-linux/` (`setup-linux.sh`, `run-linux.sh`): boots to the title screen
natively -- GPU rendering through DXVK Native on Vulkan, sound through SDL2
(PipeWire), pad and keyboard through SDL2, 60 fps. Not yet played past the
title. What it took, in the toolkit: vectored exception handlers on
sigaction (every device trap), the guest's 4 GB window held as one
reservation, partition images and the utility drive's 64 KB clusters on the
POSIX file layer, handle validation in the Win32 layer, guest functions named
like libc ones (`close`, `write`) renamed by the lifter, and vkd3d-utils'
ms_abi D3DCompile bridged to DXVK's calling convention.

Open: after the fourth Bink video in a session the video's sound turns into a
loud looping buzz (both builds). Contiguous memory is never reused (~4.5 MB
per video), and the APU decides "RAM or contiguous window" for a physical
address by the allocator's high-water mark, which then passes the sound
buffer. RECOMP_CONTIG_FREE (reuse) fixes the buzz but the game then runs at
20 fps; not on by default until that is understood. RECOMP_APU_VOICES traces
voices, RECOMP_CONTIG_TRACE the arena, RECOMP_AUDIO_MUTE silences output.

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

| 17 | ~2 flips/min after the copyright screen; D3D saw 10 of ~1,500 vblanks in 25 s | The NV2A poll loop zeroes `PMC_INTR_0`/`PCRTC_INTR_0` continuously (and never sleeps), so it beat D3D's DPC to every delivered vblank. Vblanks are now marked unacknowledged (`XBOX_NV2A_VBLANK_UNACKED`, an undefined PCRTC bit); the poll loop leaves them until the ISR's write-one-to-clear drops the mark. 1,116 vblanks in 25 s after | `../xboxrecomp/src/kernel/xbox_memory_layout.{c,h}`, `kernel_bridge.c` |
| 18 | Software method 9 sometimes "NOT handled after 500 ms" | Deadlock: the executor (on the poll thread) waited for the DPC, while D3D's vblank ack loop in the DPC waited for the poll thread to clear PMC's PCRTC bit. The stall loop now clears acknowledged vblanks itself; the timer thread is woken by an event instead of a 10 ms sleep, so methods are delivered in ~2 ms | `kernel_bridge.c` |
| 19 | 18 unresolved calls to `0x00104330` (a vtable `jmp` thunk) + 15 into D3D function tables | Seeded 90 16-byte-aligned starts after `ret`/`jmp` + padding (boundary verified by linear decode) plus the observed ones | `seed_functions.json` |
| 20 | Unresolved stubs were silent | The generator now makes each stub report its first call (`[STUB] ... reached from ...`); none fire in current runs | `../xboxrecomp/tools/recomp/translator.py`, `kernel_bridge.c` |
| 21 | Loading screen black; the 2D objects on subchannels 1-3 (NV039, NV09F image blit, NV062 surfaces) were ignored | The executor now binds objects through RAMHT and performs NV09F blits; the game copies its back buffer into a post-effects texture that way. The "Loading" screen appears | `../xboxrecomp/src/kernel/nv2a_pb_exec.c` |
| 22 | With the PFB flush acknowledged promptly, the game stalled in D3D's `BlockOnTime` with GET = PUT: 29 fences never executed | Two toolkit bugs. (a) The pushbuffer was scanned as `[last PUT, PUT)` with the ring's seam guessed; it is now executed from GET to PUT following the lap's JUMP, GET is published after every command, and the initial PUT of 0 is no longer walked as commands. (b) The real cause of the lost fences: the PGRAPH handler's write-one-to-clear was a no-op on plain memory, so D3D's DPC ran it dozens of times per trap and set `BlockOnTime`'s event each time; a late set woke the next wait early and D3D overwrote unread pushbuffer. PGRAPH writes are now trapped while a trap is pending and `PGRAPH_INTR` gets real W1C semantics (one event per trap). Found with a page watch on the overwritten word, D3D's fence table (`dev+0x4C`) checked against executed releases, and per-trap event counts | `../xboxrecomp/src/kernel/nv2a_pb_scan.c`, `xbox_memory_layout.c`, `kernel_bridge.c` |
| 23 | Main thread spends ~90% of the loading screen in D3D's KickOff flush spin, the timer thread in the vblank ack spin | Both wait on the poll thread, which is busy rasterising. A thread now clears those two bits once a millisecond (`RECOMP_BUSY_ACK=0` turns it off). Polling with `Sleep(0)` first cost more than it saved (3.5 vs 4.6 fps); at 1 ms it gives 6.6 vs 5.2 fps, and the loader, which runs on the main thread, reads 60% more in the same time | `../xboxrecomp/src/kernel/xbox_memory_layout.c` |
| 24 | Raster workers idle: only triangles with a bounding box of 8,192+ pixels were split across threads, and the loading screen is ~50k small triangles a frame (a distortion grid) | Triangles are queued per draw and rasterised together, each worker taking every n-th row of all of them, so per-pixel order is kept. 5.3 vs 4.5 fps, identical frames. `RECOMP_RASTER_BATCH=0` goes back | `../xboxrecomp/src/kernel/nv2a_pb_exec.c` |
| 25 | Gameplay ~6.3 fps. `RECOMP_WAIT_PROFILE=1` (new) showed the main thread 93% in `BlockOnTime`'s wait (GPU-bound); `RECOMP_GPU_PROFILE=1` (new) showed the pool 85% busy, so the cost is per pixel | Combiner mappings decoded once per input instead of per channel (bit-identical over 2M random programs); power-of-two fast paths for swizzled addressing and wrap (identical over every coordinate checked); raster pool default ncpu-1 instead of ncpu-4. 8.1 fps on the same scene | `../xboxrecomp/src/kernel/nv2a_combiner.c`, `nv2a_pb_exec.c`, `src/d3d/d3d8_swizzle.h`, `kernel_bridge.c` |
| 26 | `[ICALL] Failed to resolve VA 0x0006B080`, ~45/s in gameplay | It starts after `ret 4`, a 4-entry jump table and nop padding; the detector read the table as code and folded it into `sub_0006AF00`. Seeded, plus 21 more found by scanning every function's range for an aligned prologue behind ret/jump table + padding. No unresolved indirect calls in gameplay after | `seed_functions.json` |
| 27 | No sound at all: `RECOMP_AUDIO_STATS=1` (new) showed up to 9 voices playing and signal in the mixbins, but peak 0 at XAudio2 | The output stage cleared the buffer holding the DSP mixdown and filled it from the test tone and software mixer only, and submitted 4x real time (3 of 4 buffers dropped). It now plays the mixdown with those mixed on top, in whole buffers. `RECOMP_AUDIO_DUMP=<file>` (new) captures it: music and speech by spectrum, real-time rate. `logs/audio.wav` is 90 s from the intro | `../xboxrecomp/src/apu/apu_core.c`, `apu_xaudio2.c` |
| 28 | A white box over most of the view from the first tutorial popup ("Press A to jump") on | `RECOMP_FRAME_TRACE_METHODS` on that frame: the popup stamps the stencil buffer with a safe-area quad drawn with stencil func NEVER / fail REPLACE, which writes no colour on hardware. The rasteriser had no stencil at all. Added an 8-bit stencil buffer per zeta surface with the full test and ops. Also a per-draw vertex cache (verified against fresh runs) | `../xboxrecomp/src/kernel/nv2a_pb_exec.c` |
| 29 | Flat white triangles on brightly lit walls | Vertex colour outputs were not saturated to [0, 1] as the NV2A (and xemu) does; a diffuse of 2.3 scaled every texel. Clamped. What white remains is holes where nothing draws in some frames (`RECOMP_DEBUG_HOLES=1` paints them magenta), not shading | `nv2a_pb_exec.c` |
| 30 | Gunshots, impacts and sword hits thin and metallic ("tinny"); music fine | `RECOMP_AUDIO_STATS` now also counts voices by source rate: in play over half the voice-frames come from samples below 16 kHz and more from 16-24 kHz. The voice processor ignored the pitch register and read one source sample per 48 kHz output sample, so those played 2-4.4x too fast. Four-point Catmull-Rom resampler per voice. Same actions on the user's save, before and after: spectral centroid of each attack 4.5-7.7 kHz -> 1.2-2.2 kHz, no clipping (`logs/audio-ab/` has the clips) | toolkit `src/apu/apu_vp.c` |
| 31 | (same report) | The stopgap mixdown (fix 27) put centre into the left channel only, LFE into the right only, and summed the I3DL2 reverb and effect sends in raw. Now a speaker fold-down: front, back and crosstalk pairs to their own side, centre -3 dB into both, LFE low-passed into both, sends dropped (with no GP DSP they are dry duplicates). `RECOMP_APU_MIXDOWN_ALL=1` restores the plain sum | toolkit `src/apu/apu_dsp.c` |
| 32 | Black triangles on faces in cutscenes (user report) | Face culling was never implemented: SET_CULL_FACE_ENABLE / CULL_FACE / FRONT_FACE were ignored and every back face drawn, and odd strip triangles had their winding flipped. Both renderers cull now. `RECOMP_CULL=2` (the opposite winding) turns the cave inside out, which confirms the convention; `=0` disables. Ruled out on the way: polygon offset (never sent) and NaN vertex colours (`RECOMP_VSH_NAN=1`, new: none in 18 M vertices). Not confirmed as the user's triangles: the one dark mark captured, under Nevin's eye, survives culling and is not in his face texture | toolkit `nv2a_pb_exec.c`, `nv2a_gpu_d3d11.c` |
| 33 | `[ICALL] Failed to resolve VA 0x00146140`, 512 times in a 20-minute session | A virtual method reached only through its vtable (a draw through the renderer interface). Scanning every pointer from the data sections into `.text` found it as the only 16-byte-aligned, prologue-shaped target missing from the dispatch table. Seeded | `seed_functions.json` |
| 34 | 27-37 fps in heavier scenes, game thread ~80% busy | `perf` on a live session: the game thread sat in D3D's KickOff and its flush helper (`0x16C7D0`, `0x16C7A0`), spinning on PFB `0x100410` bit 16 until the busy ack thread cleared it, once a millisecond, many times a frame. The PFB page is now write-trapped and the flush completes on the write. Same scene on the user's save: 54 -> 66 fps, ~1.4 -> ~0.9 cores | toolkit `xbox_memory_layout.c` |
| 35 | (same profile) GPU thread a third busy with bookkeeping | The pushbuffer scanner's method inventory searched its table linearly for every method executed. Direct index | toolkit `nv2a_pb_scan.c` |
| 36 | Hang a minute into play from the "Stealth" save: frozen frame, game thread waiting forever | At the tutorial video `TD_TUTOR.bik` the heap was exhausted: memory `NtAllocateVirtualMemory` takes from the heap was never returned by `NtFreeVirtualMemory`, so every video leaked its buffers. With none left, the decoder thread got no stack and was run inline, and the game waited on itself. `RECOMP_HEAP_VFREE` (now a `run.sh` default) returns them: 4 MB live less after one video. The full `RECOMP_HEAP_RECLAIM` stalls this game's boot, so it stays off | toolkit `kernel_bridge.c`, `xbox_memory_layout.c`; `run.sh` |
| 37 | Uneven frame pacing: light scenes at 66 fps, menus at 129, and judder even at a steady rate | Nothing tied flips to a display rate, and the window redrew guest memory on its own 16 ms GDI loop. Frames now go from the GPU into a DXGI swap chain on the window, presented with vsync (`nv2a_gpu_present`): 60 fps on the 59.96 Hz monitor, no per-frame readback or copies. `RECOMP_GPU_PRESENT=0` restores the old path, which now caps flips at `RECOMP_FLIP_HZ` (60) | toolkit `nv2a_gpu_d3d11.c`, `fb_present.c` |
| 38 | Hitches the first time an effect appears | Each combiner/vertex-program shader was compiled on first draw (~8 ms each, some far more) and again every session. Compiled shaders are kept in `shader-cache/` | toolkit `nv2a_gpu_d3d11.c` |
| 39 | Videos (tutorials, the moves screen, the intro) full of striped 8x8 blocks on detailed edges | Compared with ffmpeg's decode of the same `.bik`: 96.6% of each frame bit-exact, 163 blocks wrong. The blocks traced to Bink's IDCT (`sub_001A6580`), whose `movsx ebp, bp` the lifter turned into a zero-extend: its 16-bit sign-extend list lacked `bp` and `sp`. Every negative row-6 coefficient became ~65,000. Fixed in the lifter (regression test included); the intro now matches ffmpeg | toolkit `tools/recomp/lifter.py` |
| 40 | (found on the way, with the conformance fuzzer now runnable here via gcc -m32) | Signed conditions after `add`/`sub` ignored overflow, and `sub`'s ordered ones were 32-bit only: 323 of 30,000 fuzz vectors wrong, now 0 | toolkit `lifter.py`, `tools/conformance` |
| 41 | Dark triangle under Nevin's eye in close-ups; holes in meshes | `RECOMP_TRI_PROBE` on the mark's pixel: no face triangle covered it, only the eyeball behind (brown iris texture). `ARRAY_ELEMENT32` was unhandled, so every odd-sized indexed draw lost its last triangle -- here the lower eyelid's. Handled; the face matches the user's reference | toolkit `nv2a_pb_exec.c` |
| 42 | (proactive) More lifter bugs | The conformance fuzzer, widened (every movsx/movzx register, multiplies, carry chains, partial registers, bt, cmov, shift flags, random chains), found 133 of 3,000 cases wrong: one-operand `mul`/`imul`/`div`/`idiv` evaluated at 32 bits whatever the width, no CF/OF from any multiply, `neg`'s signed and `jbe`/`ja` conditions, `rol`/`ror` carry, and `cwd`/register `xadd` unimplemented. Fixed; 180,000 vectors, 0 mismatches; game regenerated and the intro video still matches ffmpeg | toolkit `lifter.py`, `translator.py`, `tools/conformance` |
| 43 | A move video crashed the game (Wine page fault in its allocator); the earlier freeze's real cause | 25-minute soak with `RECOMP_HEAP_STATS`: after a level loads the heap was 285 KB from full, and a video's 1.6 MB request failed. The 8 MB stack region below the heap was the cost -- the thread uses ~20 KB. `RECOMP_STACK_KB=2048` (new, `run.sh` default): heap 48.5 -> 54.5 MB; the same soak (152 move videos) flat, no failed allocation | toolkit `xbox_memory_layout.c`; `run.sh` |
| 44 | Texture work in the GPU thread | Whole-texture decoding instead of a texel at a time; four-lane change hashing; `RECOMP_RT_DIRECT=1` (`run.sh` default) samples render targets on the GPU -- the sky's 448x448 target was read back, decoded and uploaded 60 times a second | toolkit `nv2a_pb_exec.c`, `nv2a_gpu_d3d11.c` |
| 45 | 640x480 | `RECOMP_SCALE=N` renders at N x (default 2, 1280x960): 60 fps, GPU ~28%. Guest memory stays console-sized; readbacks are filtered down | toolkit `nv2a_gpu_d3d11.c`, `fb_present.c`; `run.sh` |
| 46 | "Missing textures" in the second mission (Surface): large flat white and pink areas | Not textures -- missing geometry. `RECOMP_TRI_PROBE` on a white pixel found only the sky backdrop behind it, and the executor's overflow warning fired: terrain chunks have more than 4096 indices per batch and the rest were dropped. Limit raised to 65536; the same walk from the user's save now shows the cliffs (the pink was correctly fogged far terrain, fog end 4500 units) | toolkit `nv2a_pb_exec.c` |
| 47 | Window only, and stretched when resized | Borderless fullscreen on Alt+Enter/F11 (`fullscreen = 1` in settings.ini starts that way); the swap chain follows the window and the frame is letterboxed at 4:3 | toolkit `fb_present.c`, `nv2a_gpu_d3d11.c` |
| 48 | The Vibration option did nothing | XAPI's rumble output reports (a class SET_REPORT on the control pipe, in Alter Echo) were accepted and dropped; now decoded and passed to XInput (`rumble = 0` turns it off) | toolkit `ohci.c`, `usb_gamepad.c` |

## Where it stands (15 Oct 2026, ~01:35)

Boots, runs engine init, shows the copyright screen at a steady frame rate,
finishes it, and hangs entering state 10 (`0xB4880` → `sub_0002C810` →
camera update → slerp with t = inf on a NULL keyframe track).

**Root cause, and the big one: Alter Echo loads game code at runtime.** The
object scripts (Nevin, enemies, effects -- `object_*.dll`) are PE DLLs packed
inside the `.orf`/`.clf` archives, loaded by the game's own loader
(`InitializeDLL`, `DLLFixupNames`, "Image not a DLL"). None of that code was
translated, so the calls into them (`[ICALL] target 0x01F60E50 is not code`)
are skipped, Nevin never initialises ("Nevin wasn't loaded, can't continue!"),
no camera animation is created, and the camera code dereferences NULL.

Size of it: 2,918 module instances in 53 files, **314 unique images (210
names), 2.6 MB of code**. Every module is a plain PE DLL, preferred base
`0x10000000`, 60-90 KB, no imports, with relocations and the same ten exports
(`CreateInstance`, `CallInstanceEvent`, `InitializeDLL`, ...). That uniformity
makes a second, ahead-of-time stage feasible: extract and dedupe the modules,
give each a fixed guest base, translate each at that base, and make the game's
loader place each module at its base so its own relocation produces the image
that was translated.

Still open beyond that: audio (`RECOMP_AC97_READY=1` stalls engine init in
state 1); 193 unresolved call targets inside detected functions.

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

## Stage 2: runtime-loaded modules (in progress, 15 Oct 2026 ~04:45)

How it works:
- `tools/extract_modules.py`: pulls the 314 unique PE modules (209 object,
  57 level, 48 trigger scripts) out of the disc archives into `modules/`.
- `tools/build_modules.py`: gives each module a fixed effective base from
  `0x74000000` (23.7 MB of address space). It writes
  `build-modules/default_modules.xbe`, the XBE plus one executable section
  per module (`M0001`...) holding that module's `.text`, relocated to its
  base. It also writes `modules/module_seeds.json` (export and entry seeds)
  and generates `src/module_layout.c` (the relocation-site table).
- `tools/regen_modules.sh`: the whole pipeline on that XBE. Result: 22,751
  functions (13,600 from modules), 0 failed. It replaces the old
  regeneration commands.
- `src/module_loader.c` wraps the game's loader (`sub_0011D860`, so the
  generated body is `sub_0011D860_gen`). After a successful load it commits
  the module region, moves the image from the heap to its base, re-applies
  the relocations, and repoints the loader object. It also registers the
  region as code (`xbox_AddCodeRange`), which `RECOMP_ICALL` would otherwise
  reject as wild pointers.
- Run with `RECOMP_EXT_VMA=1` in addition to the usual switches.

Further fixes found along the way:
- **Import thunks.** Each module's `.text` opens with `int3` slots that its
  `DLLFixupNames` patches at load time with `jmp <engine function>`. Ahead of
  time they look like padding, so a call into one did nothing and unbalanced
  the stack. That was the 8-byte drift, found with an ABI-check build
  (`-DRECOMP_ABI_CHECK`). `build_modules.py` now finds the 8,518 slots the
  modules call and generates `src/module_thunks.c`. There each slot is
  hand-written as `ae_dyn_thunk(slot)`, which follows the patched `jmp` at
  run time (via `--manual-functions build-modules/thunk_functions.json`).
- **Code pointers.** Functions in the modules that are only reached through
  vtables or callbacks are found from the relocation table: any relocated
  dword in `.rdata`/`.data` (or a `push`/`mov` immediate in `.text`) that
  points into `.text`. That gives 650 seeds.
- **Audio.** With the modules running, the game plays 3D sound, so
  DirectSound must initialise (`RECOMP_AC97_READY=1`). Its DSP effects
  download then waits for the GP DSP, which is only a stub, to clear a
  command word. `RECOMP_DSP_ACK=0x808A8810` (the toolkit's probe) clears it.
  This is a stopgap until the DSP is emulated.
- **Repeated section names.** The XBE has four `BINKYUY2` sections, and
  `--extra-sections` finds sections by name, so three were never
  disassembled. The augmented XBE renames repeats (`BINKYUY2_2`...).

**Milestone (15 Oct 2026 ~05:15): the main menu renders** at ~35-45 fps:
Start Game / Load Game / Options / Extras / Credits (`logs/main-menu.png`).
The background is flat, since Bink video was failing (being fixed now).

Run it as your own user (not root): `./run.sh`. It now turns on everything
this build needs by default (the list and what each does is at the top of
`run.sh`); without those switches the game runs but opens no window, which
looked like it not starting at all.

Keys, with the window focused: arrows d-pad, Enter START, Z/X/A/S = A/B/X/Y,
numpad 8/4/2/6 left stick (movement), I/J/K/L right stick (camera), 1/3
triggers, Q/E white/black, Backspace BACK. An XInput controller works too.
Expect ~9 fps in the level: the GPU is emulated in software.

**Milestone (15 Oct 2026 ~05:50): a new game starts.** Intro video →
main menu (`logs/main-menu-full.png`) → Start Game → the intro cinematic
plays (`logs/intro-cinematic.png`, a few colour-speckle artifacts) → the
first level `L00_A1` loads with its modules.

Toolkit fix found on the way: `cpuid`, `pushfd` and `popfd` were never
lifted, so RAD's MMX check returned garbage and Bink took a fallback path
that called a NULL decoder pointer. They're now lifted, with `cpuid`
answering as the Xbox's Pentium III.

**Milestone (15 Oct 2026 ~10:30): gameplay.** Start Game -> intro
cinematic -> a few seconds of "Loading" -> the in-engine cutscene (Nevin wakes
up, subtitled dialogue) -> the player in control in the first corridor of
`L00_A1`, with the HUD (`logs/first-gameplay.png`; `logs/first-level-paused.png`
is the pause menu over the 3D scene). ~6 fps at first, ~8 after fix 25.

What it took, after the level's modules loaded: the loader runs on the main
thread, which spent ~90% of its time spinning in D3D's KickOff on the PFB
flush bit, waiting for the poll thread to finish rasterising (fixes 22-24).
Before them, the level was still on its loading screen after ten minutes.

Pad script that reaches gameplay unpaused: START every 3 s from 18 s to 39 s
(accepted on the title screen, selects Start Game, skips the cinematic), then
nothing; a press once the level runs opens the pause menu:
`RECOMP_PAD_SCRIPT=$(seq 18000 3000 39000 | sed 's/$/:start:300/' | paste -sd,)`.

To test from a save instead, drive the menus live, since the intro's length
drifts by 15 s between runs: start with `RECOMP_PAD_LIVE=logs/pad.live` and
`RECOMP_FB_WINDOW_DUMP_EVERY=2`, look at `logs/frame.bmp`, and append presses
(`echo a >> logs/pad.live`). From the main menu (one START once the logos
are skippable, ~24 s): `down`, `a` (Load Game), `a` (Save Game 1, the newest),
`a` (restore). In the level, RT/LT toggle the sword and gun forms; X, B and Y
attack in either.

**Milestone (15 Oct 2026, evening): rendering on the graphics card.** With
the toolkit's new Direct3D 11 backend (`RECOMP_GPU=1`, now `run.sh`'s
default; branch `gpu-backend` of the toolkit) the first level runs at 35 fps
instead of 7-9, at 20% CPU instead of 64% (CPU package
60-68 C instead of 85-100 on an i7-4790K; a GTX 960 ~13% busy). Vertex
programs still run on the CPU; rasterisation and the register combiners are
on the GPU. Later the same evening vertex programs moved to the GPU too
(translated to HLSL per program): 50-57 fps, still ~23% CPU, 62-68 C. Going
faster exposed the game's frame limiter, which waits on the
CRTC scanline register; that is emulated now. `RECOMP_GPU=0` goes back to the
software rasteriser, which stays as the reference.

**Next:**
- Speed. The software rasteriser is the limit: workers spend their time in
  `nv2a_rc_eval` (register combiners), `sample_tex` and `read_rgb`. The
  D3D11 backend in the toolkit (`src/nv2a/nv2a_pgraph_d3d11.c`) only
  translates another title's 2D menu, so a hardware path is a project of its
  own; profiling and tightening the per-pixel path is the near-term option.
- Holes: a few wedge-shaped gaps in one cutscene shot and a sliver on the
  first stairs, where no draw reaches the pixel in some frames (fix 29 has
  the probes). Not clipping precision (a guard band was tried and changed
  nothing). Occlusion-query results (zpass reports) are the next suspect.
- Play further: movement and combat with a real controller
  (`RECOMP_USB=1` takes XInput input), the first checkpoints.
- Audio plays at the right pitch now (fixes 27, 30, 31), still through the
  GP/EP stub: a stereo fold-down. No effects are missing that the title
  uses: across every RECOMP_AUDIO_STATS window recorded (menus, both morph
  forms in combat, videos) nothing reached mixbin 10 (I3DL2 reverb) or the
  effect sends above it, so the console mixes it dry too. The DSP ack is a
  hand-entered address; the GP DSP needs real command handling for both.
- Check whether fix 41 also cleared the "pressure spot" black triangles and
  the cutscene wedge holes (both look like missing last triangles).
- Bink speckles.
