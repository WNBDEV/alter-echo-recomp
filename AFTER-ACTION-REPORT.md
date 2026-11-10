# After-action report

What it took to bring Alter Echo (Xbox, 2003) to the PC by static
recompilation, from the first pipeline run on 14 October 2026 to survival mode
and the performance work of 10 November: what went right, what went wrong,
where the time goes, and what we would tell anyone attempting the same.

[DEVLOG.md](DEVLOG.md) is the full record, with every problem numbered (fixes
1-81). This report draws the lessons out of it. The toolkit is the
[xboxrecomp](https://github.com/WNBDEV/xboxrecomp) fork, branch `gpu-backend`.

## Contents

- [In one page](#in-one-page)
- [What we built](#what-we-built)
- [What went right](#what-went-right)
- [What went wrong](#what-went-wrong)
- [Where the time goes](#where-the-time-goes)
- [Dos and don'ts](#dos-and-donts)
- [Still open](#still-open)

## In one page

- **It works.** Every one of the 51 areas loads and runs; the game has been
  played through on Linux, natively and under Wine, at 60 fps in nearly all
  of it. Installers build it on the player's own PC from their copy of the
  disc: nothing of the game is distributed.
- **The translation was the easy part.** The first run translated 8,356 of
  8,356 functions with none failing, in a day. The weeks went on what the game
  expects of the hardware and kernel around it: the GPU, the sound chip, USB,
  interrupts, memory, and the code it loads at run time.
- **Runtime-loaded code was the biggest surprise.** The game's object and
  level scripts are 314 DLLs inside its data archives. Nothing ran until they
  were extracted, given fixed addresses and translated ahead of time too.
- **A scripted test harness paid for itself many times over.** Booting any
  area by name, a pad driven from a file, frame dumps and a sweep of every
  level let most problems be found and checked without anyone playing.
- **Our worst mistakes were around the player's machine, not the code:**
  killing their game, losing their logs, taking their speakers, running as
  root, and builds that crashed their PC.
- **Performance is bounded by emulation, not by the clock.** The game's code
  takes 2-4 times the instructions it did on the Xbox, and the graphics chip's
  work, free on the console, costs a CPU thread as much again. In the heaviest
  fights the two take turns. A faster CPU barely helps; less work does.

## What we built

| When | What | Fixes |
|---|---|---|
| 14 Oct | The pipeline on `default.xbe`: 8,359 functions found, 8,356 translated, 1.09 million lines of C | 1-5 |
| 14-15 Oct | Booting: the hard disk's cluster size, USB and the controller, interrupts, the GPU's command stream and its fences | 2-24 |
| 15 Oct | Stage 2: the 314 runtime-loaded modules translated ahead of time (22,751 functions in all) | see the timeline |
| 15-22 Oct | Playable: sound, textures, videos, lifter bugs, memory that only grew, crashes and hangs | 25-56 |
| 16-23 Oct | Native Linux: DXVK Native, SDL2, a POSIX file layer | timeline, 65-66, 72 |
| 22-24 Oct | For players: keyboard and mouse, camera directions, logs, installers for Windows and Linux; 1.0.0 released 24 Oct | 57-70 |
| 24-29 Oct | Widescreen (1.1.1), fast updates and faster installs (1.1.2) | 71-74 |
| 29-30 Oct | New Game+ with tiers 1-9 (1.2.0) | 75-78 |
| 10 Nov | Survival mode's first floor; heavy fights; a frame profiler | 79-81 |

Today the translation is 23,459 functions in 2.04 million lines of C in 60
files, plus about 3,000 lines of hand-written C in this project and 76
commits of our own in the toolkit. A clean
install takes about 12 minutes on an i7-4790K: about 4 to translate, 7 to
compile.

## What went right

**Translate everything, then find what is missing at run time.** The pipeline
translates every function it can find, and an indirect call to an address it
did not know is logged (`[ICALL] Failed to resolve VA ...`) instead of
crashing silently. Each such address became a seed for the next run (fix 5,
`seed_functions.json`), or showed a lifter bug to fix (fixes 26, 33, 49).
By the sweep of every level (22 October) none were left.

**Runtime-loaded code, done ahead of time.** All 314 modules are plain DLLs of
the same shape (one preferred base, no imports, the same ten exports). So a
second stage extracts them, gives each a fixed address from `0x74000000`,
translates them with the game, and wraps the game's own loader so it places
each module where it was translated. The modules' import slots, which their
loader patches with jumps into the engine, looked like padding ahead of time
and had to be found and turned into dispatchers (8,518 of them). This one idea
made the game run at all.

**The game's own development features.** The retail build still parses a
development command line; it just never gets one. Handing it `RECOMP_ARGS`
(`src/game_args.c`) gave level warps, `-bypass` and `-noai` for free, and with
them `tools/level_sweep.sh`, which boots all 51 areas in turn: all loaded on
the first try, which no amount of playing would have shown that early.

**Testing without a player.** A pad driven by appending lines to a file
(`RECOMP_PAD_LIVE`), frame dumps of the window and of the GPU at full
resolution, a log line for every slow frame and why, and copies of the saves
bind-mounted over the real ones in a private namespace. With those, a fix
could be checked end to end: boot, menus, a level, a fight, death, the main
menu, screenshots of each.

**Wrapping instead of editing generated code.** A hand-written `sub_X` that
calls the translated body as `sub_X_gen` (the translator does the renaming)
is how the module loader, the command line, New Game+, survival and the
native `_ftol2` hook in. The generated C is never edited and can be
regenerated at any time.

**Using the game's own engine for the extras.** The level scripts call the
engine through a table of 730 named functions (`SA_Matcen_EnableMatcen`,
`SA_ScreenText_AppendText`, ...). Calling the same functions from C made New
Game+'s banner and survival's spawners, waves and screen text a matter of
finding the right ones. The scripts themselves can be read: each import stub
at `+0x1000 + 5i` is name `i` in the DLL's own name table, so a disassembly
can be annotated with the engine calls it makes. Cut main-menu entries
("arena", "Secrets") became New Game+ and Survival.

**A numbered fix log, written as we went.** Every problem, how it was found,
what fixed it and which files changed. When something came back weeks later,
or a decision had to be explained, the answer was already written down.

**Measuring before changing, and checking after.** The audio "is the music
right?" question was settled by aligning a recording of the output with the
disc's own audio, sample by sample. The performance work only kept what a
measurement showed; one change that measured as noise was reverted.

**Pinning the toolkit by commit.** The setup checks out the toolkit at
`TOOLKIT_COMMIT`. A player's install is always the combination we tested,
and a project change that needs a toolkit change moves the pin with it.

## What went wrong

**We killed the player's game.** On 23 October a measurement script found
two game processes, took the first, which was the player's own session, and
stopped it mid-play. The fix in practice: never act on a process found by
name; record the PID you start, and refuse to test while a game of theirs is
running.

**We lost the player's logs, twice.** The launcher keeps only the newest ten
logs, so a series of test launches pushed out a log the player had saved
for a report (23 October). On 10 November a clean-up moved the player's own
session log out of the folder, taken for a test's, and a test launch pruned
their oldest one. Both were restored from a backup taken at the start. Back
up their logs before testing, and only move files whose names you recorded
when you created them.

**Other ways a test run reached into the player's machine.** A run without
`XDG_RUNTIME_DIR` made Wine fall back to raw ALSA and took the speakers away
from the desktop (21 October). A left-over "hold" script killed the next
test's game when its timer ran out, and two forgotten game copies ran at 130%
CPU for 13 minutes and spoiled every measurement taken meanwhile (22 October).
Test windows open on the player's screen, so a player at the keyboard can
press keys in them.

**Running as root.** The early sessions ran as root, leaving root-owned files
across the project that had to be migrated on 24 October. Run as the user who
owns the files.

**Builds that crashed the PC.** The player's PC hard-reset twice during
compiles (23 and 30 October) with the same machine-check error on two cores,
the second time during a single three-job build. Translated C is heavy to
compile (GCC needs 1.4-1.6 GB for the largest files), and that is enough to
expose marginal hardware. One build at a time, and ask before a heavy one.

**Misleading measurements.**
- A test window that Wayland considered hidden had its presents throttled to
  about 20 fps, which a whole session blamed on a memory setting. Run tests
  with `SDL_VIDEODRIVER=x11`, or make sure the window is visible.
- The music "sounded wrong" until it turned out the reference was: ffmpeg
  decodes Xbox ADPCM with 65 samples a block, the Xbox with 64.
- The GPU thread's time "running commands" was mostly a sleep: the 60 fps
  pacing sleeps inside the command loop, about 4 seconds in every 10. That
  hid its real cost and made two good optimisations look like nothing until
  CPU time was measured instead of wall time.
- A code comment said the pacing "never waits" while the window is visible.
  It slept about 40% of the time.
- Frames counted as slow in a 90-second fight varied from 23 to 36 between
  two runs of the same build: enemies move differently each time. That is
  fine for a big change and useless for a 5% one.

**Translator details that cost a rebuild.** Adding a wrap changes every
translated file, so each new hook was an 11-minute rebuild; hooks are
batched now. And the translator finds wraps by matching `void sub_X(void)` at
the start of a line, so wraps written with a macro are not seen.

**First guesses that were wrong.** Leaving a level by switching straight to
the main menu left the level loaded under it, with the death screen on top;
the pause menu's Quit goes through a menu that unloads the area first.
Survival's spawners refused a second wave until they were switched off and on
again. New Game+'s first version kept the gun in hand without the gun, so it
could be fired until put away and then never picked again. Each was found by
testing the whole path, not just the part that changed.

**Unsolved.** All sound was lost for good once, after a music change in
L03_A2 (30 October), and ten minutes of scripted play could not bring it
back. An Arana fight soft-locked once. A sync node's effect looked boxy once.
Problems seen once need the player's save, or the game left open for
inspection, to be solved.

## Where the time goes

Measured on 10 November in a survival fight (waves of Plastids, a scripted
pad playing sword combos) on an i7-4790K held at 3.5 GHz, with the frame
profiler this work added (`RECOMP_FRAME_STATS`, `RECOMP_TRACE`; fix 81).

**Most of the time it holds 60 fps:** a frame every 16.7 ms. Averaged over a
whole heavy fight, 17-18 ms (56-59 fps). The heaviest ten seconds averaged
21.1 ms, with one frame in twenty over 33 ms. In a 24-minute session of the
player's, the average was 58.9 fps and the worst ten seconds 46 fps.

**Two threads do the work:**

| Per frame, heaviest ten seconds | The game's code | The GPU emulation |
|---|---|---|
| On the CPU | 12.4 ms | 11.9 ms |
| Waiting for the other | 7.5 ms | 7.4 ms |
| Instructions (whole fight, average) | 23 million | 39 million |
| Instructions per cycle | 1.38 | 1.19 |

**Why they wait for each other.** About once a frame (five times in the
heaviest fights) the game locks a buffer the GPU may still be reading. The
Xbox's Direct3D then waits until the GPU has carried out every command sent so
far. On the console the GPU is a separate chip and that wait is short. Here
the GPU is a thread on the same CPU, as slow as the game, so the two take
turns: each has about 12 ms of work, which fits a frame, but one after the
other they need 20 ms or more. The graphics card itself is about a quarter
busy.

**Why a 733 MHz console kept up.** On the Xbox the game's code ran natively
and the GPU did its own work. Here the game's code is translated C, with the
CPU's registers, flags and floating-point stack kept in memory and every
float rounded to single precision through a function; it executes 2-4 times
the instructions the console's whole frame budget allowed. And the GPU's work
is emulated on the CPU, costing more instructions than the game itself.

**Why a faster clock barely helps.** At 1.2-1.4 instructions per cycle, with
about three million last-level cache misses a second on each of the two busy
threads, much of the time is spent waiting on memory, which does not get
faster with the clock. And the turn-taking stays. The player saw no gain at
4.5 GHz.

**What fix 80 did** (frames of 25 ms or more in a 90-second fight: 36-61
before, 19-36 after):

| Change | Effect |
|---|---|
| Built for the player's own CPU (`-march=native`, with `-ffp-contract=off` so a * b + c keeps its two roundings) | About a third fewer slow frames |
| The GPU thread handles indices, vertex data and constants in bulk, finds a draw's vertices without scattering over 1.5 MB, and stops counting every command | About 20% less CPU per command (64-70 ns to 53-55 ns) |
| The C library's float-to-integer routine written natively | About 2-3% of the game thread |
| Not changed: the 60 fps pacing | With it off, the game still held 60 and dropped as often |

**What would help next, biggest first:** keeping the game's floating-point
stack in local variables inside each translated function (a translator
change, 1-3 days, every float operation retested); fewer and cheaper draws in
the GPU emulation (merging particle draws, a cheaper widescreen HUD test);
and letting the two threads overlap by splitting the GPU emulation into a
stage that reads commands at once and one that draws on another core (1-2
days, and the riskiest). [TODO.md](TODO.md) keeps the details.

## Dos and don'ts

### The translation

- **Do** find every piece of code the game runs, including code it loads at
  run time from its data. Look for DLLs or overlays in the archives early.
- **Do** log every indirect call the translation cannot resolve, with its
  address, and feed those addresses back as seeds.
- **Do** keep the x87 floating point the way the console ran it: single
  precision (precision control 24), the guest's rounding mode for integer
  stores, and no fused multiply-add when compiling the generated C
  (`-ffp-contract=off`, especially with `-march=native`). Games compare
  values with their own stored copies and break on one rounding of
  difference.
- **Do** hook by wrapping (`sub_X` calling `sub_X_gen`), never by editing
  generated code, and write each wrap out in full where the translator's
  scanner can see it.
- **Don't** decode the data sections as code because the executable marks
  them executable. Alter Echo's `.rdata` and `.data` are, and decoding them
  lost hundreds of real functions found through vtables.
- **Don't** expect the first translation to be the hard part. Budget for the
  hardware.

### The hardware around it

- **Do** reproduce the hardware's handshakes, not just its registers: the
  GPU's fences and software-method interrupts, vblank acknowledgement, the
  sound chip's command words. Most early hangs were a game waiting for an
  acknowledgement that never came.
- **Do** make every wait event-driven, with a timeout. A `Sleep(1)` in a loop
  costs a millisecond each time natively, and the game multiplies it.
- **Do** watch memory over a long session, level after level. Contiguous
  memory only ever grew, about 4 MB a video (fix 51), and a 75-minute session
  ran out of memory in the end (fix 58).

### Testing

- **Do** build a scripted test harness first: boot any level by name, drive
  the pad from a file, dump frames, sweep every level. Look for the game's
  own development switches; they may still be in the retail build.
- **Do** use the game's extras as test benches. Survival mode gave the heavy
  fight on demand that the performance work had been waiting for since 23
  October.
- **Do** isolate tests from the player's data (copies of the saves,
  bind-mounted) and from their session (your own PID only, never by name).
- **Don't** test while the player is playing, or leave anything behind that
  will act later (timers, held processes, stray copies).
- **Don't** let your test runs fill rotating log folders that hold the
  player's logs; back theirs up first, and clean up only what you recorded
  creating.

### Measuring performance

- **Do** measure CPU time per unit of work (time per GPU command word, per
  frame per thread), not wall time; wall time includes every sleep.
- **Do** A/B in one binary with an environment switch, alternating runs,
  two or more each. Counts of slow frames vary by half again between
  identical runs.
- **Do** look at both threads at once (a timeline, `RECOMP_TRACE`) before
  optimising one. Two busy threads waiting on each other look like two
  half-idle threads.
- **Do** check whether the graphics card is busy before blaming it (here it
  was a quarter busy), and whether a faster clock helps before buying one.
- **Don't** trust comments about timing; measure them.
- **Don't** keep an optimisation you cannot measure.

### Working with the player

- **Do** run as the user who owns the files, never as root, and pass the
  desktop session's environment (`XDG_RUNTIME_DIR`, the display) to tests.
- **Do** ask before heavy builds on their machine, one at a time.
- **Do** keep extras opt-in and plug and play: their own menu entry, no
  settings to find, the original game untouched unless picked.
- **Do** write it all down as you go: a numbered fix log, the test recipes,
  the decisions and why.

## Still open

- **Survival mode:** the other floors (the training arenas of each chapter,
  then the Paavo Echo fights), a wave and time counter on screen, wave
  tuning ([TODO.md](TODO.md)).
- **Performance:** the levers listed under
  [Where the time goes](#where-the-time-goes).
- **Seen once, unsolved:** the lost sound in L03_A2, the Arana soft-lock, the
  boxy sync node effect.
- **Untested platforms:** Windows on a real PC (only under Wine so far),
  Steam Deck and other read-only systems.
- **The toolkit** is upstream v0.13.1 plus our 76 commits; upstream was 226
  commits further on in October, many in the translator. Merging is a full
  regenerate and retest, and may bring speed with it.
