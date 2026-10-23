# Alter Echo for PC — static recompilation

Alter Echo (Outrage Games / THQ, Xbox, 2003) running natively on a PC: the
game's own Xbox program translated to C and compiled, with the console's
hardware -- graphics chip, sound, controller, kernel -- reimplemented by the
[xboxrecomp](https://github.com/sp00nznet/xboxrecomp) toolkit (this project
uses [a fork of it](https://github.com/FILL-IN/xboxrecomp) carrying its GPU
backend and fixes). Not an emulator of the CPU: the game's code runs as
ordinary PC code.

**No game files are included.** You need your own copy of the North American
(NTSC) release, as a disc image or its extracted files. The build script
checks it is that exact release.

What works: the whole game as far as it has been played (the first missions
start to finish), at a steady 60 fps, rendered at 2x the original resolution
or more, fullscreen, with controller rumble and correct audio. Rendering runs
on the GPU (Direct3D 11, through DXVK on Linux).

## What you need

- Linux, either with Wine and DXVK (the game is built as a Windows program;
  the main way to play) or as a native Linux program without Wine
  (experimental, below). Windows: untested so far
- To build with Wine: python3, cmake, ninja, git and MinGW-w64
  (`x86_64-w64-mingw32-gcc`). For the native build instead: python3, git,
  gcc, cmake, ninja, pkg-config, curl and the SDL2, libepoxy and OpenSSL
  development files. Either setup fetches the toolkit next to this folder
  (`../xboxrecomp`) at the version this project needs, or checks the one
  already there
- 20-40 minutes for the first build and several GB of free RAM
- A controller (Xbox 360 or compatible), or a keyboard and mouse

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
`settings.example.ini` on first run): resolution scale, fullscreen, texture
filtering (mipmaps and 16x anisotropic by default), sharpening (off by
default; 0.3-0.5 crisps up the 480i-era textures at 2x), rumble, and the
keyboard and mouse (below).
While playing, **Alt+Enter** or **F11** toggles fullscreen; the picture
keeps the console's 4:3 with black bars. **F1** shows or hides a frame
counter in the corner. **F2** switches between 60 fps and a steady 30
(the game keeps its speed either way). **Alt+F4** or the window's close
button closes the program (the pause menu's Quit only leaves the level);
save first. Saves go
where the console keeps them (`UDATA/` in the game folder).

### Controls

An Xbox 360 pad works directly, with rumble. The keyboard and mouse work at
the same time as a pad (`input = auto`); `input = controller` or
`input = keyboard` in `settings.ini` picks one. **Click in the window** to
use the mouse (the pointer hides and stays inside); **Alt+Tab** lets go of
it.

| Key | Pad | In Alter Echo |
|---|---|---|
| W A S D | left stick | move |
| mouse (or I J K L) | right stick | camera |
| Space | A | jump; select in menus |
| left click | X | attack |
| right click | Y | jump attack in the air, grenade |
| F | B | gun smash, roll attack; cancel in menus |
| Shift | left stick click | block while held; Shift+Space rolls |
| C | White | block |
| Q / E | left / right trigger | morph to gun / sword form |
| mouse wheel, Tab | Black | next gun |
| middle click, R | right stick click | time dilation |
| Enter, Escape | START | pause; "Done" in the Options menu |
| Backspace | BACK | |
| arrow keys | d-pad | menus; picking targets (in time dilation W A S D do it too) |

The game's own prompts show pad buttons: (A) is Space, (B) is F, START is
Enter or Escape. In the Options menu, B leaves without keeping changes;
START keeps them. Settings with choices (Vibration, **Pitch Axis** for the
camera's up/down) change with **left/right**, not A.

Every key can be changed in `settings.ini` (`bind_a = Space`, several keys
with commas, `none` for no key; the list of key names is there), and so can
the mouse: `mouse_sensitivity` (0 turns mouse look off), `mouse_invert_y`,
and `mouse_deadzone`. The camera turns and tilts the way the stick or mouse
goes; on the Xbox it swung the other way (push right, the view turned left;
push up, it tilted down), and `camera_flip_x = 0` / `camera_flip_y = 0` bring
that back for the stick and mouse alike. The mouse moves the camera by tilting the right stick,
as far as its speed asks; the game ignores the stick's first 40%, and the
mouse starts just past that, so small movements still turn. A very fast
flick turns no faster than the stick pushed all the way.

### Starting at any level

The game kept its developers' command line, and `RECOMP_ARGS` hands it one:

```bash
RECOMP_ARGS="L05_A1 -bypass" ./run.sh    # straight into L05_A1, no menus
```

The names are the area files in `ODB Data/ODB MEDIA/clf` (`L00_A1` to
`L09_A9`). Without `-bypass` the game starts normally and the menu's Start
Game goes to the named area instead of the first one. The game's other
switches pass through too (`-nomovies`, `-noai`, `-allowskip`, ...; the list
is in `src/game_args.c`). It's a test tool: an area started this way has none
of the progress a real playthrough would have carried into it, and Nevin
may not take control there (L02_A1 with `-bypass`: START opens the pause
menu, the sticks move nothing); load a save for anything that needs
playing.
`tools/level_sweep.sh` uses it to boot every area in turn and report how each
one does.

## Native Linux build (experimental)

The same game compiled as a Linux program -- no Wine or MinGW needed:

```bash
./setup-linux.sh "/path/to/Alter Echo (USA).iso"   # or the extracted game folder
./run-linux.sh                                      # same settings.ini and keys as run.sh
```

`setup-linux.sh` checks your copy and translates it -- the same steps as
`setup.sh`, without the Windows build and the Wine prefix; if `setup.sh` has
run already it reuses that, and the argument can be left out -- then fetches
DXVK Native and the vkd3d shader compiler into `build-linux/deps` and
compiles (25-45 minutes). vkd3d is fetched by itself on Arch-based systems;
elsewhere install your distribution's package (Debian/Ubuntu
`libvkd3d-utils1`, Fedora `vkd3d-utils`).

Window, sound and controller go through SDL2. Graphics use the same Direct3D
11 renderer as the Wine build, on DXVK Native (DXVK built as Linux libraries,
talking to Vulkan directly); shaders compile with vkd3d-utils, the compiler
Wine's own uses, so `shader-cache/` is shared between the two builds. Neither
library is installed system-wide, and if either is missing the game falls
back to the software renderer (correct, but far slower). Saves are the same
`UDATA/` folder the Wine build uses.

Status: see "Native Linux" under Development notes. `run.sh` remains the
way to play.

## Reporting a problem

Every launch writes a log, `logs/run-<date>-<time>.txt` (`logs/native-...`
for the Linux build), and the launcher prints its name when the game closes.
The newest ten are kept. For a crash, a freeze, a slowdown or a sound
problem:

1. Note when it happened and what was going on: the area, a fight, a video,
   how long you had been playing.
2. Zip that log (`zip report.zip logs/run-<date>-<time>.txt`, or your file
   manager's Compress) and attach it to an issue on the project page, with
   what you saw.
3. If it happens in one place, a save from just before helps: saves are the
   folders under `UDATA/` in the game folder.

The log starts with the system it ran on (CPU, RAM, graphics card and
driver, OS, Wine version) and your settings, so there is no need to list
them. It holds no names except, in one graphics line, your account's user
name in a Windows-style path (`C:\users\<name>\...`). What it records along
the way: frame rate every ten seconds, frames that took 25 ms or more and
why, the sound cutting out or the mix going over full scale, memory running
short, and files the game opened.

## Known limits

- Played through the first missions. Every later area boots and looks
  right in a screenshot when started directly (see "Every level, booted" below), but none
  has been played through: scripted events, boss fights and saving there are
  untested. Logs are in `logs/` (see "Reporting a problem").
- A few later areas dip below 60 fps at moments: to ~50 in L07_A2a, L06_A9
  and the ending, on an i7-4790K with a GTX 960.
- 60 fps is the cap, as on the console (the game is built around it).
- The sound is the console's dry mix: the game uses no reverb or effects.
- Close-ups in the in-engine cutscenes look soft at 2x: edges are rendered
  at full resolution, but the textures are the original ones, made for a
  480i TV, and the cutscene camera magnifies them. That is the game's own
  art, not a rendering fault; `sharpen = 0.3` or so in settings.ini helps.

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

The buzz after the fourth Bink video (both builds) is fix 50.
RECOMP_APU_VOICES traces voices, RECOMP_APU_PHYS_STATS how the APU's reads
are resolved, RECOMP_CONTIG_TRACE the arena, RECOMP_AUDIO_MUTE silences output.

**Update, 22 Oct.** Rebuilt with everything through fix 53, it plays a new
game through the opening cutscene into the first room at 60 fps, sound
through SDL (PipeWire), no unresolved calls; RECOMP_CONTIG_FREE is now on
here too. The "flat 20 fps with RECOMP_CONTIG_FREE" was not the reuse: a
window Wayland considers hidden (a test run behind other windows) has its
presents throttled to ~20 fps, reuse or not; with the window visible, or
SDL_VIDEODRIVER=x11, it is 60 either way. Still open natively:
- No swap chain: `xbox_FramebufferWindowHwnd` has no HWND for the SDL
  window, so `nv2a_gpu_present` declines and every frame is read back and
  shown through guest memory -- the picture is 640x480 however high
  RECOMP_SCALE renders, and RECOMP_GPU_SHOT has nothing to shoot. DXVK
  Native can take the SDL window for a swap chain; that hook is the work.
- SIGTERM does not stop it (`timeout`, `kill` without -9); closing the
  window does.

**Update, 23 Oct.** Both of those are done: the SDL window is the swap
chain's (`presenting straight to the window (1280x960 now, 4:3 letterboxed),
vsync`, the same as under Wine), and SIGTERM stops it. `setup-linux.sh` no
longer needs `setup.sh`'s Wine and MinGW: it runs it with `NATIVE=1`, which
stops after the translation. Checked on a clean machine -- fresh clones, an
empty HOME, a PATH with no Wine and no MinGW, no system vkd3d: 24 minutes,
translation identical to the dev build (64/64 files), DXVK Native and vkd3d
fetched into `build-linux/deps`, boots on the GPU at 60 fps. Still played far
less than the Wine build.

## Memory over a session (22 Oct 2026)

Will a whole playthrough fit? The runtime returns only some freed memory (the
full RECOMP_HEAP_RECLAIM stalls boot), so this was measured: 14 level loads
in two sessions, alternating Surface: Arana (`L01_A2`) and Surface: Master
(`L01_MASTER`) through the pause menu's Quit, with RECOMP_HEAP_STATS and
RECOMP_CONTIG_TRACE. The heap grows on first visits (30.4 MB at boot, 32.8 MB
after the first level) and then stays put to the kilobyte through every later
load, ~23 MB never touched at the top of its 55.8 MB. Contiguous memory
(textures, video, sound) reaches 20.1 MB by the seventh load and stays there:
each load allocates ~700 blocks and frees them on leaving (fix 51 reuses
them), ~44 MB of the 64 MB window spare. No leak in either; a later level
would have to need far more than the console itself has to run out.

## Every level, booted (22 Oct 2026)

Only L00 and L01 (7 of the 51 area files) had ever been loaded. The game
still parses a development command line (sub_000164F0: a level name and
nineteen -switches, into the settings block at 0x1E7B68), but the retail
build calls it with argc = 1. `src/game_args.c` wraps it and passes
RECOMP_ARGS as the argv it never got, built on the guest stack; the boot path
(sub_0001FDE0, with -bypass) and the menu's Start Game (sub_000B56B0) then
start the named area instead of L00_A1. Both were tried.

`tools/level_sweep.sh` booted each of the 51 areas with -bypass and watched
it for 40 s. All 51 loaded in about 2 s and were still running when stopped:
no crashes, no [ICALL] misses, no instruction outside the five every run
reaches, no new GPU methods. The frames (one per area, console-sized) show each drawn as expected:
the late areas' enemies and bosses, their cutscenes with subtitles, the
ending (L09_A9). The `_Master` files all open on the same corridor: they
hold shared content, not a place to play. Left idle, Nevin died in L09_A5.

Not all of them held 60 fps (i7-4790K, GTX 960, 2x): L09_A9 33, L07_A2a 37,
L06_A2 38, L08_A2 42, L02_A4 44, and 46-54 in nine more. Fix 54 took the
executor's cost per frame down by up to half; swept again, those fourteen
hold 58-60, with dips to 48-50 in L07_A2a, L06_A9 and the ending (L09_A9).

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

The names `merge_names.py` applies come from `tools/ghidra_names.json` in this
project (`--names-json`, as `tools/regen_modules.sh` runs it): the 196 MSVC
runtime routines Ghidra recognised, merged from its export once. The export
itself (`tools/ghidra_naming/export/` in the toolkit) is local and not in git,
so a fresh checkout has none. To redo it: the toolkit's `run_ghidra.sh` is
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
| 49 | Second mission (Surface: Arana, `L01_A2`): `[ICALL] Failed to resolve VA 0x74662750` on loading the area's save, then a NULL call from the main loop millions of times (19 Oct, a frozen frame); replayed from the same save, the "Disc Read Error" screen instead | A 9-way switch in `object_Buddies.dll` (`sub_74662570`, jump table at `0x74662B80`). The function list ended the method at the `ret` before its arms and took the `mov edi, edi` aligning the table for a function of its own, so each arm was translated as a separate entry -- except arm 2, which branches back to the shared epilogue and cannot run alone, and had no body. A weak entry under 4 bytes before a function's own table no longer bounds it: the method owns its arms as an inline switch. The same layout in `object_L09_A7_Miniboss.dll` (`sub_74DB7120`) is lifted the same way; no other function's code changed. Every indexed tail jump in the generated code now has a body for each arm. Same save before/after: old build fails as above, new build loads it and plays the fight that follows at 55-60 fps, no unresolved calls | toolkit `tools/recomp/translator.py`, `test_trailing_jump_tables.py` |
| 50 | From the fourth Bink video in a session, the video's sound a loud looping buzz (both builds) | Physical P is both the RAM at guest VA P and the contiguous window's `0x80000000 + P`; on hardware one store, here two. The APU guessed which from the contiguous allocator, which leaks ~4.5 MB per video, and on the fourth play of the attract intro its high-water mark passed the Bink sound buffer -- heap memory -- so the APU read the sound from the window. But the title knows: on hardware the only way to get a RAM page's physical address is `MmGetPhysicalAddress`, so every RAM page a bus master sees went through it. It now remembers per page which store the VA was in, and the APU asks that before guessing. Idle at the title, 8 minutes, before/after: `RECOMP_APU_PHYS_STATS` (new) shows the title overriding the guess 0 times in plays 1-3 and on every Bink read from play 4 (10 M reads by play 5); the 4th play's sound matches the 1st to a median 0.03 dB (before: 0.92 dB, 28 s off by more than 1.5 dB). Same fps. `RECOMP_CONTIG_FREE` is no longer needed for this | toolkit `kernel_memory.c`, `xbox_memory_layout.c`, `apu_core.c`, `apu_vp.c` |
| 51 | (found with fix 50) Contiguous memory only ever grew: ~4 MB per full-screen video, 15.2 MB at boot and 35 MB after five attract plays, so about a dozen videos in a session would have used up the 64 MB window | `RECOMP_CONTIG_FREE` (freed contiguous blocks are reused) is now a `run.sh` default: the same idle run stays at 17.7 MB from the second play on, 60 fps at the title and 29-30 in videos as before, the sound unchanged. Loading the Surface: Arana save and fighting in it: 50-57 fps (40-60 without), no unresolved calls. The 20 fps it gave was the native build only; `run-linux.sh` leaves it off | `run.sh` |
| 52 | (proactive) DirectSound's start-up waited on a DSP command word that `run.sh` completed at a hand-entered address, `RECOMP_DSP_ACK=0x808A8810` -- right for this boot's allocation layout and nothing else; any change in where contiguous memory lands (fix 51 reuses it) could have moved it and hung sound setup | The word is byte 0x810 of the GP DSP's scratch memory, whose pages are listed in the scatter-gather table at the APU's `GPSADDR` register; the toolkit now completes the table's first page + 0x810 every APU frame. It derives 0x008A8810 -- the hard-coded address -- and with the address gone from `run.sh` the game boots, completes command 3, loads a level with sound, audio levels as before | toolkit `apu_dsp.c`; `run.sh`, `run-linux.sh` |
| 53 | (user report, a regression from fix 51) In the Surface: Arana level intro ("I need to find Arana...") Nevin had nothing above the neck, for the whole shot -- with or without mipmaps or GPU vertex programs, fine with `RECOMP_CONTIG_FREE=0` | The GPU backend knows surfaces by guest address and was never told memory was freed: once freed contiguous blocks were reused, a render target it still held over a freed block either stood in for the new data there or was read back over it -- here the talking head's. `MmFreeContiguousMemory` now queues the range and the GPU thread drops what overlaps it, without a readback (the title's Direct3D waits for the GPU before freeing), before its next command. Same restore before/after: no head, then the head, the log saying "dropped 1 surface(s) over memory the title freed". Found with GPU shots taken only while a trigger file exists (`RECOMP_GPU_SHOT_TRIGGER`, new). Also from this round: mip levels are now the game's own, decoded, not generated (they match to a mean 0.13 levels) | toolkit `nv2a_gpu_d3d11.c`, `xbox_memory_layout.c`, `nv2a_pb_exec.c` |
| 54 | Four later areas at 36-44 fps (L07_A2a, L06_A2, L08_A2, L02_A4; the level sweep) | Not the graphics card (16-21% busy, no faster at 1x): `RECOMP_WAIT_PROFILE` had the game thread 79% waiting on D3D's GPU wait, so on the executor thread. `perf` with LBR call stacks (Wine's memcpy can't be unwound otherwise) put a third of the executor in ucrtbase: program draws uploaded all sixteen vertex inputs, 256 bytes a vertex, whatever the program read (now one packed array per input it reads, each in its own vertex-buffer slot), and three getenv checks still ran per texture, per draw and per APU register access (now read once). Executor CPU per frame, AI on, before/after: L07_A2a 16.7 -> 8.1 ms (36 -> 57-60 fps), L02_A4 13-16 -> 7-9, L06_A2 8.5-12.3 -> 4.5-6.7, L08_A2 10.2 -> 8.4. Frames match | toolkit `nv2a_gpu_d3d11.c`, `nv2a_pb_exec.c`, `apu_mmio_hook.c` |
| 55 | (user report) 19-35 fps in the first boss fight (L02_A4) after dying and continuing; fine before | Reproduced by warping in, walking Nevin at the boss until he died and continuing: 25 fps for ~50 s. A new ten-second line in the log (render targets made and why, readbacks and who asked, textures that could not be sampled from a GPU surface and why, image blits) showed 18,000-23,500 readbacks per 10 s, 7 s of every 10 waiting: the title blits ~1,200 GPU-drawn rectangles a second there, and each blit was done in guest memory too, so both surfaces were read back first. A blit whose source the GPU holds is now done on the GPU only. On the way: the render-target cache went from 8 to 32 (effect surfaces had evicted each other in the user's log), and surfaces are made at the size the title samples them at, not the clip it draws (an effect growing in the corner of a 64x64 texture was rebuilt at every step). The same death and continue: 60 fps, 150 readbacks per 10 s; frames match | toolkit `nv2a_gpu_d3d11.c`, `nv2a_pb_exec.c` |
| 56 | (user report) The game froze for good in the first boss fight's bombing run: frame stopped, window open | The log: `software method 0100=00000005 NOT handled after 500 ms`, then 0 fps. D3D's BlockOnTime queues that NOP and sleeps on an event its ISR sets on seeing it; a debugger on the frozen game showed the game thread in that wait and the executor idle. The NV2A interrupt status registers are updated by several threads (timer vblank, executor software methods, the title's acknowledgements, poll threads) with plain read-modify-writes, so one thread can wipe another's bit; lose PMC's PGRAPH bit and the ISR never sees the method. Widening the vblank update to 2 ms reproduced the same freeze within seconds of entering a level. All those updates are atomic now; under the same widening, four minutes of the boss fight with a death and continue lost nothing. Rare before (none in a week of logs); fixes 54-55's faster frames likely made it likelier | toolkit `kernel_bridge.c`, `xbox_memory_layout.c` |
| 57 | (user request) Playing on PC: the keyboard had a fixed test layout (numpad to move, Z/X/A/S for the face buttons), no mouse, and nothing on screen said how fast it ran | Keyboard and mouse now stand in for the pad with WASD, mouse camera and every key rebindable in `settings.ini` (`bind_*`), plus `input = auto/controller/keyboard`; F1 shows a frame counter, drawn at the present from a built-in 5x7 font. The mouse is read raw while the window holds it (WM_INPUT under Wine, SDL's relative mode natively), and its motion is turned into a right-stick tilt that drains over 25 ms, so further means further and stopping stops. The game's stick dead zone, measured with a steady fake mouse (`RECOMP_MOUSE_FAKE`) and the scene's shift between timed frames, is about 0.40 of full tilt: up to there the camera stood still, so the smallest mouse motion starts there. The window loop now wakes on input instead of sleeping 16 ms, so keys and mouse arrive at once. Layout taken from the game's move list (X attacks, Y specials, A jumps, B smashes, left stick click blocks, triggers morph, Black cycles guns) | toolkit `xinput_device.c`, `fb_present.c`, `nv2a_gpu_d3d11.c`; `settings.example.ini`, `run.sh`, `run-linux.sh` |
| 58 | (user report) The game crashed in L08_MASTER while browsing View Moves, 75 minutes and eleven areas into a session: `xbox_HeapAlloc: out of memory (requested 2295776, used 55442544/57147392)`, then an access violation | The title used the null it got for a Bink buffer. The heap's top was full while megabytes sat free in pieces: freed blocks are only reused whole (the full reclaim mode stalls this game at boot), and frees stopped merging at the empty slot an earlier merge left. Now, at the moment a request would fail and only then, the free runs are merged, a free run at the top goes back to the bump pointer, and the request comes from the top or is split out of the smallest free block that fits. A randomised test of the allocator (alloc/free mixes on 24-56 MB heaps, contents and block table checked every 50 steps) found no overlap or corruption and a half to a tenth of the failures; in game, View Moves with 41 videos and a capped heap (`RECOMP_HEAP_KB`, new) ran normally. Not replayed: the crash needed 75 minutes of a particular session. Also new in the log: every frame 25 ms or more after the one before, with where its time went (waiting for the game, textures, shaders, Present), for the elevator dip in L06_A8B (48 fps on the F1 counter) and future reports | toolkit `xbox_memory_layout.c`, `nv2a_gpu_d3d11.c` |
| 59 | (user report) In fights -- shooting plastids, getting shot, Nevin crying out -- the sound crackled and cut out | Not the mix: a fight peaked at 82% of full scale, and the game sets every voice and mix bin the same headroom (7 and 1), so there was nothing to rebalance. It was the sound thread falling behind: with every core loaded it made 73 of the 469 output buffers needed per 10 s, each after the output had run dry. It gave up the CPU after every frame, 1,500 times a second, so the game could take its lock -- costly on a busy machine -- and threw away any time it woke more than 5 ms late instead of catching up; and the output kept one 21 ms buffer in hand. Now it steps aside only while the game is waiting for the lock, catches up unless 100 ms behind, and after any dropout puts two buffers of silence ahead of the sound (six may queue). Same fight, same load: 468-470 buffers per 10 s, the output never dry; boot no longer drops out either (9 times before). The final mix is limited instead of clipped (no change unless it would go over full scale), and the log now says when sound was lost or the mix went over. Both builds | toolkit `src/apu/apu_core.c`, `apu_xaudio2.c`, `apu_dsp.c`, `apu_vp.c` |
| 60 | (user question, before release) Would a player's log help a bug report? It was ~43 MB an hour, never removed (382 here, 2.4 GB), said nothing about the machine, and the README did not say what to do with it | The trace lines that were 85% of it (one per pushbuffer update, one per 200 draws) were on because the executor switch every player runs also armed the tracer; they, the executor's ten-second detail, every file read after the first 200 (failures still logged) and a kernel summary every 2 s now need a trace switch. Standing in a level the log grows ~70 KB an hour (1.1 MB before the last two). Both launchers keep the newest ten logs, start each with the system (CPU, RAM, graphics card and driver, OS, kernel, Wine, every RECOMP_ setting; no host or user names) and print the log's name on exit; README "Reporting a problem" says what to send | `run.sh`, `run-linux.sh`, README; toolkit `kernel_bridge.c`, `nv2a_pb_exec.c`, `nv2a_pb_scan.c`, `xbox_memory_layout.c` |
| 61 | (user request) A way to compare the game at 60 fps with a steady 30 | F2 switches between the two while playing (`RECOMP_FPS30=1` starts at 30): the swap chain presents on every second vblank and the flip cap behind it halves, so a hidden window and the native build hold 30 too; the slow-frame log's bar doubles with it. The game keeps real time at 30: Save Game 1's opening cutscene ended 10.6 s after the level opened at 60 and 10.7 s at 30, and the unplayed fight after it ended at 60.8 s and 61.6 s (the game reads the clock, not the frame count). F2 can no longer be bound to a pad button | toolkit `fb_present.c`, `nv2a_gpu_d3d11.c`, `nv2a_pb_exec.c`, `xinput_device.c`; `settings.example.ini` |
| 62 | (user report) The camera turned against the stick and mouse (push right, the view turned left), and in time dilation only the arrow keys picked targets, not WASD | Both as on the Xbox: the game swings its camera round Nevin the way the stick pushes, and its options flip only the pitch; time dilation reads the D-pad alone (its tutorial: "You can also use the directional pad while in Time Dilation"; the controller's left stick does nothing there either). The right stick's left/right is now flipped for pad, mouse and look keys (`camera_flip_x`, on by default; `RECOMP_CAMERA_FLIP_X`): a fake mouse moving right now turns the view right, and with the flip off the scene moves the other way, as reported. While time dilation runs, WASD press the D-pad too; not otherwise, because in a fight the D-pad picks the target and walking would keep changing it. The time dilation manager is the object at 0x5416B0, its state at +0x9C 5 in play and 0-4 from the right stick click to the end, found from guest memory snapshots; in game, D then moved the target marker to the right-hand enemy | toolkit `xinput_device.c`, `xinput_xbox.h`; `src/title_input.c`, `src/main.c`, `settings.example.ini`, `run.sh`, `run-linux.sh` |
| 63 | (user report) Up and down were still the console's way: mouse or stick up tilted the view down | The same flip for the right stick's up/down (`camera_flip_y`, on by default; `RECOMP_CAMERA_FLIP_Y`), for pad, mouse and look keys together; the game's Pitch Axis option still flips it back on top. Checked by the user with a real mouse and an Xbox controller | toolkit `xinput_device.c`; `settings.example.ini`, `run.sh`, `run-linux.sh` |
| 64 | (clean-machine setup test) A new Wine prefix stopped on Wine's dialog offering to download Wine Mono, which the player had to click through | setup.sh makes the prefix with Mono and Gecko disabled (`WINEDLLOVERRIDES="mscoree,mshtml="`); the game uses neither .NET nor the HTML engine. run.sh sets the same, so the prefix refresh after a Wine update cannot raise it either. A fresh prefix now takes 19 s with no dialog and still gets DXVK, and the game boots from it. The rest of the clean test passed at toolkit 2b35e86: fresh clones, an empty HOME, translation identical to the dev build (64/64 files, modules too), compile, boot to the attract video | `setup.sh`, `run.sh` |
| 65 | (release prep) The native Linux build could not be set up without Wine: `setup-linux.sh` needed `setup.sh`, which refused to run without Wine and MinGW and then built the Windows `.exe` and a Wine prefix | `setup-linux.sh <copy>` now does it all: it runs `setup.sh` with `NATIVE=1` (check the copy, unpack, translate, stop) and checks up front for everything the native compile needs, SDL2, libepoxy and OpenSSL included, where a missing one used to fail half way. Clean-machine test with Wine and MinGW off the PATH and no system vkd3d: setup in 24 minutes, translation identical to the dev build, DXVK Native and vkd3d fetched into `build-linux/deps`, boots on the GPU through the SDL window's swap chain at 60 fps | `setup.sh`, `setup-linux.sh`, README |
| 66 | (user report) In the native Linux build the controller did nothing; keyboard and mouse worked | Nothing ever started SDL's game controller support (`xbox_InputInit` was never called; XInput needs no setup, so the Windows build never missed it), so no pad was ever opened. It now starts on the first poll, and once a second pads that went away are closed and new ones opened, so a controller switched on mid-game is picked up too. Each is logged (`[INPUT] pad 1: Xbox 360 Controller`); in a test boot the user's pad was found at once | toolkit `xinput_device.c` |
| 67 | (user report) Frame drops to 40-48 fps in heavy moments -- the sword form's Fatal Flurry landing its hits, form swaps -- on both builds | The slow-frame log put the time in the GPU thread waiting for the game, and perf showed neither thread busy: they were waiting on each other through Sleep(1) polling. Natively each NV2A software method (about two a frame) cost 1.1-1.5 ms however fast the handler answered, 21-33% of the GPU thread's time, and interrupt delivery polled for DISPATCH_LEVEL the same way. Both now wake on an event: 0.14-0.34 ms a method, 1-4% of the GPU thread. Under Wine that stall was already cheap, so the Wine build is unchanged, and the heaviest moments remain bound by the game's own code on one thread; that part is still open (reproducing it needs a save next to a fight). Native logs now list the files the game opens, as Wine ones do | toolkit `kernel_bridge.c`, `kernel_hal.c`, `kernel_path.c` |

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
  This is a stopgap until the DSP is emulated. (Since fix 52 the toolkit
  finds that word itself and `run.sh` no longer sets the address.)
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
- ~~Holes: a few wedge-shaped gaps in one cutscene shot and a sliver on the
  first stairs.~~ Gone (22 Oct, after fixes 41 and 46): `RECOMP_DEBUG_HOLES`,
  now working on the GPU backend too, finds no undrawn pixel in any sampled
  frame of the new game's opening cutscene or the first room's stairs (every
  30th frame, GPU shots at 2x). Only the loading transition leaves the
  outermost pixel row and column undrawn, on a black screen.
- Play further: movement and combat with a real controller
  (`RECOMP_USB=1` takes XInput input), the first checkpoints.
- Audio plays at the right pitch now (fixes 27, 30, 31), still through the
  GP/EP stub: a stereo fold-down. No effects are missing that the title
  uses: across every RECOMP_AUDIO_STATS window recorded (menus, both morph
  forms in combat, videos) nothing reached mixbin 10 (I3DL2 reverb) or the
  effect sends above it, so the console mixes it dry too. ~~The DSP ack is a
  hand-entered address~~ (fix 52 derives it); real command handling would
  still need the GP DSP emulated.
- ~~Check whether fix 41 also cleared the "pressure spot" black triangles and
  the cutscene wedge holes~~ -- the wedge holes are gone (above).
- ~~Bink speckles.~~ Fix 39: the intro matches ffmpeg's decode.
