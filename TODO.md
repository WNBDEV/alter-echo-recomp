# To do

Ideas and known gaps parked for later. Nothing here is promised or scheduled.
Each entry says what is already known, so whoever picks it up doesn't start
from zero. If you run into one of these, or something new, see
[Reporting a problem](README.md#reporting-a-problem).

## Contents

- [New features](#new-features): how extras are added, widescreen follow-ups, tuning New Game+ tiers and marking its saves, spending money on New Game+, a survival mode
- [Performance](#performance)
- [Polish](#polish)
- [Platforms and installers](#platforms-and-installers)
- [Toolkit](#toolkit)
- [Reports wanted](#reports-wanted)

## New features

### How extras are added

The port stays the original game. Gameplay that the original doesn't have,
like New Game+ and the survival mode, is an extra (decided 29 Oct 2026):

- **The original game unless the player picks it.** An extra starts from its
  own menu or Start Menu entry, with no setting to turn it on (decided 30 Oct
  2026: players want plug and play). One that would change the normal game
  instead is off by default. Display options like widescreen aren't extras.
- **Its own code**, under `src/extras/`, one file or folder per extra. It hooks
  into the port in as few places as possible, so the port itself stays easy to
  read.
- **Its own README section**, marked as not part of the original game.
- **Same repository and installers.** A separate "plus" repository would carry
  a second copy of the installers, the setup and the runtime, and every fix to
  the port would have to be merged across. Split only if an extra needs changes
  to the translated code or the toolkit that can't be switched off.

### Widescreen: follow-ups

Widescreen is built and on by default (fix 71 in [DEVLOG.md](DEVLOG.md)): a
wider 3D view, no black bars, the HUD and text in shape in the middle, movies
in shape. It
does it in the graphics backend, not by patching the game: the game's one 4:3
constant turned out to be shared with unrelated maths. What could come next:

- **Sharper sides:** the frame is the console's width stretched. Rendering
  wider surfaces instead (a wider resolution scale across than down) would
  keep full detail.
- **HUD at the screen's edges:** `RECOMP_WS_HUD=sides` pins the health bar and
  counters to the corners, but tears apart text the game draws a word at a
  time (the title screen's "Please press START to begin."). Grouping a line's
  words before placing them would fix it.
- **Edge pop-in:** the game still culls with its 4:3 view. Not seen in
  testing; if something appears late at the sides, its culling field of view
  is the place to widen.

### New Game+: tuning the tiers

New Game Plus 1 to 9 is built (fix 77 in [DEVLOG.md](DEVLOG.md)): every hit on
Nevin is 2x to 10x harder, never more than half his health in one hit. The
numbers come from a few measured hits (a Plastid's 5, late-game shots of 2 and
10-12); the high tiers need playing to tune, and bosses were not measured.
Ideas left for the top tiers, not agreed: less healing, time dilation draining
faster.

The Load Game list shows only the area a save is in, so a New Game+ save looks
like a normal one until it loads and the "New Game Plus N" banner shows. Marking
them in the list (the tier next to the name) would need the Load Game screen
worked out first.

### New Game+: something to spend money on

On New Game+ every upgrade is often already bought, but money keeps coming in
with nothing to spend it on (the player, 30 Oct 2026). Options, best first:

1. **Cut upgrades.** Check the shop's 50 upgrade slots and the game's strings
   for upgrades that were cut but left in the data, like the cut "arena" menu
   entry New Game+ reuses. Bringing one back is the most faithful option.
2. **Cosmetics**, for example tints or effects for Nevin or his sword. They
   don't touch the New Game+ difficulty, but each needs the game's materials
   or effects worked out.
3. **Money carried into the survival mode** (below): start a run better
   equipped, or buy a continue.
4. **More damage or more health for Nevin.** Easiest to build, but both work
   against the harder tiers: extra health divides a tier's harder hits back
   out just as extra damage shortens the fights. More health could work if it
   is bought again each campaign and resets at the next tier, so a new tier
   still bites at first.

Not started; option 1 is a quick check of the data before choosing.

### Survival mode (like Devil May Cry 4's Bloody Palace)

Endless waves of enemies in an arena, getting harder each wave, with a wave
counter and a timer, until you die.

**What's known:** this is an idea, not researched yet. The engine (Outrage
Games, who also made Descent 3) has more of the building blocks than you'd
expect. Its level scripts call about 680 engine functions by name, including:

| Need | Engine function |
|---|---|
| Spawn enemies | "Matcens", Descent-style enemy spawners: `SA_Matcen_ActivateMatcen`, `SA_Matcen_ResetMatcenLimits`, `SQ_Matcen_GetNumAlive`; and `SA_Obj_CreateObject` |
| Make them harder | `SA_Squad_SetObjectDifficulty`, `SA_Squad_SetDefaultDifficulty`, `SA_Combat_ModifyDamage` |
| Wave number and timer on screen | `SA_ScreenText_*` (text, whole numbers, decimals, position, fade) and `SA_Timer_*` |
| Rewards between waves | `SA_Player_AddMoney`, time-dilation and invulnerability power-ups |
| Move between arenas | `SA_Script_ChangeLevel` |

At least one area already has a spawner that keeps going: in L02_A1, elite
Plastids respawn for as long as you stay and fight them.

**A first version could be:** one arena and a `settings.ini` switch that starts
there. Code added to the port watches the spawners. When a wave is cleared, it
resets them with a bigger count and higher difficulty, shows "Wave N" and the
time, and when you die it shows how far you got instead of reloading a
checkpoint.

**Hard parts:**
- The arena's own level script may lock doors, start cutscenes or end the fight
  on its own. We need arenas where it doesn't, or a way to switch the script off.
- Each area probably loads only its own enemies' models, so mixing enemy types
  may need arenas that already contain them.
- Dying normally reloads the last checkpoint. The mode has to catch that.
- Booting straight into an area with `-bypass` sometimes doesn't give you
  control of Nevin (seen in L02_A1), so starting the mode may need a fix there
  first.
- Scores have nowhere to be saved or shown except a file or the screen.

**Feasibility:** a rough version (endless waves in one arena) looks plausible.
A full Bloody Palace, with floors, different arenas, bosses, rewards and a menu
entry, is a big project. New Game+ (built first) found most of what it needs:
calling the game's script functions from the port, catching menu choices, and
reading and writing Nevin's state.

## Performance

### Frame drops in the heaviest fights

Fatal Flurry landing many hits, form swaps, grenades and explosions can drop to
40-48 fps. The waiting in the port's own code is fixed (fix 67 in
[DEVLOG.md](DEVLOG.md)). What's left is the game's own code taking longer than
a frame on its one thread.

**Lead:** the C library's float-to-integer routine (`_ftol2`, `sub_00151D3C`) is
about 7.5% of that thread's time, because it runs as translated x87 code. A
hand-written native version, added to the functions the translator skips (the
`manual_wraps.c` list in `tools/setup_game.py`), should cut most of that. Other hot spots:
`sub_00075480`, and on the graphics side `fetch_attr`,
`gpu_draw_batch_program` and `hash_lanes`.

**Needed:** a save made right before a big fight, so the drop can be repeated
and measured.

### Elevator dip in L06_A8B

Drops to about 48 fps during the swarm on the elevator. The game's log now
records every frame that takes 25 ms or more, and why. It needs a replay of
that spot with that log.

### Audio thread

`xbox_ContiguousOwns` is about 40% of the audio thread's time (a linear search
for every sample read). It's an easy speed-up, but only worth doing if audio CPU
time ever matters.

## Polish

### Thin lines in the health bar

Two 1-pixel vertical lines at about 1/5 and 4/5 of the health bar's width, at both
1x and 2x. They are probably seams where the bar's pieces meet
(texture edges or clamping). Not yet compared against xemu or a real Xbox.

## Platforms and installers

- **Windows on a real PC:** the installer and the game it builds have only been
  run under Wine. Reports from real Windows 10/11 PCs are the most useful thing
  right now; attach `install.log` if the installer stops.
- **Steam Deck, Bazzite, Fedora Silverblue and other read-only systems:**
  untested. The installer can't add build tools to a read-only system, and
  building from source hits the same wall. Running the installer inside a
  distrobox container is the likely route, and it needs testing.
- **Progress during translation:** the installer's "Translating the game's code
  to C" step goes quiet for 2-5 minutes while it writes the C code. It should
  name each part and show a running clock, so nobody thinks it has frozen.
- **The Linux command assumes the download folder:** `bash
  AlterEchoInstaller.sh` only works from the folder the file is in. The release
  notes could say `bash ~/Downloads/AlterEchoInstaller.sh`.
- **Unneeded test programs:** the installer also compiles the `d3d8_smoke` test
  programs, which players don't need.

### Faster installs

**Done for 1.1.2 (fix 74, not released yet).** A clean Linux install on the
same PC (i7-4790K, 4 cores and 8 threads, 16 GB) went from about 20 minutes to
11.7:

| Step | 24 Oct | 29 Oct | What changed |
|---|---|---|---|
| Translating the game's code to C | about 6.5 min | 4.1 min | The translator's 216-second "load" was decoding every function three times over, and capstone's `disasm()` decodes its whole buffer before handing back the first instruction, so a walk that wanted a few instructions decoded the rest of the function each time. Now each function is decoded once, and walks decode a window at a time. The load takes 84 s. The C that comes out is byte for byte the same. |
| Compiling the translated files | about 8-9 min, 2 at a time | 7.3 min, 8 at a time | GCC needed far less memory than the 2 GB per file guessed: 1.4-1.6 GB for the three largest files, under 700 MB for the rest. So the build now runs one file per thread, as many as free memory holds (`JOBS=N` on Linux, `/jobs N` on Windows to choose), and starts the largest first. |
| Unpacking the disc, downloads | about 2 min | 0.3 min | Disk and network; nothing changed here. The disc image was already in the file cache this time. |

The compile gained less than expected on this PC (the estimate was 9 minutes
in total): with 8 files at a time each one compiled about 3 times slower than
alone, because the 4 cores share their cache and DDR3 memory. There was no
thermal throttling. A newer PC with more cores should gain much more, but
that is not measured.

An update that changes only the translator now keeps every translated file
that comes out the same, so it compiles none of them again. Updating 1.1.1 to
1.1.2 should take about 4-5 minutes, not a full build: a translator change
tried on the test install took 4.3 minutes, kept all 62 translated files and
compiled 2 small files.

**Still open:**
- Translating still runs on one core for about 4 minutes. Most of that is
  capstone decoding about 2.2 million instructions in Python, once in the
  disassembler and once in the translator. Decoding in several processes at
  once could save 1-2 minutes, but it is more code to get right on Windows.
- Windows install times are estimated (15-30 minutes), not measured since
  the change.

## Toolkit

### Merge newer upstream xboxrecomp

This port's toolkit ([the fork](https://github.com/WNBDEV/xboxrecomp), branch
`gpu-backend`) is upstream v0.13.1 plus 69 commits of our own (about 16,000
lines: the Direct3D 11 backend and the fixes in DEVLOG.md). Upstream is now at
v0.16.0, 226 commits further on, with 36 files changed on both sides. Many of
its changes are to the disassembler and translator, so they would change the
generated C. That means a branch, a merge, regenerating, and a full retest (the
level sweep plus a play-through).

## Reports wanted

### Seen once

These happened once and couldn't be repeated on demand:

- **Arana fight soft-lock (L02_A1):** after the pit fight Arana just stood
  there, with no dialogue and no way on. Reloading fixed it. If it happens to
  you, don't reload: save into a new slot while stuck, and send that save with
  the log and what you did just before (time dilation, fast form
  swaps, farming the respawning Plastids).
- **Out of memory after a long session:** the game once crashed browsing View
  Moves in the pause menu, 75 minutes and eleven areas in. Fix 58 should stop
  it, but that session was never replayed. If you see `out of memory` in a log,
  please send it.
