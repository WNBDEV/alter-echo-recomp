# To do

Ideas and known gaps parked for later. Nothing here is promised or scheduled.
Each entry says what is already known, so whoever picks it up doesn't start
from zero. If you run into one of these, or something new, see
[Reporting a problem](README.md#reporting-a-problem).

## Contents

- [New features](#new-features): widescreen follow-ups, New Game+, a survival mode
- [Performance](#performance)
- [Polish](#polish)
- [Platforms and installers](#platforms-and-installers)
- [Toolkit](#toolkit)
- [Reports wanted](#reports-wanted)

## New features

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

### New Game+

A fresh campaign from the first level that keeps your upgrades, forms and money
from a finished save.

**What's known:** the game already has most of the pieces.
- Its scripts can grant and take away abilities:
  `SA_Player_Upgrade_GivePlayerGunFormUpgrades`, the stealth-form version, the
  matching take-away functions, `SA_Player_AddMoney` and
  `SA_Morph_AddPlayerMorphForm`.
- The developer switch `-allmorphs` unlocks every form at the start, so the game
  already copes with a player who has everything early.
- Starting a new campaign at a chosen level already works (`RECOMP_ARGS`).

**Plan:**
1. Find where your upgrades, time-dilation meter and money live, both in memory
   and in the save file.
2. Add a `settings.ini` option, something like `new_game_plus = <save slot>`.
   When you start a new game, it copies those values from that save and saves
   into a new slot, leaving the finished save alone.
3. Play through the first few areas to check that nothing breaks.

**Risks:**
- **Story scripts:** early levels hand out abilities and run tutorials, and some
  scripts take abilities away. A script that expects you not to have a form yet
  could misbehave. Only a play-through will show.
- **Difficulty:** enemies won't get tougher on their own. The game has a damage
  hook (`SA_Combat_ModifyDamage`) and per-enemy difficulty
  (`SA_Squad_SetObjectDifficulty`), so tougher enemies are possible as a second
  step.
- **A "New Game+" entry in the main menu** is harder than a setting, because the
  menus come from the game's data files.

**Feasibility:** good odds for the `settings.ini` version. It's a few sessions
of work, mostly testing by playing.

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
entry, is a big project. It's harder than New Game+, and New Game+ builds the
skills it needs (finding the player's data, calling the game's script functions
from the port), so New Game+ would come first.

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
  to C" step goes quiet for 5-10 minutes while it writes the C code. It should
  name each part and show a running clock, so nobody thinks it has frozen.
- **The Linux command assumes the download folder:** `bash
  AlterEchoInstaller.sh` only works from the folder the file is in. The release
  notes could say `bash ~/Downloads/AlterEchoInstaller.sh`.
- **Unneeded test programs:** the installer also compiles the `d3d8_smoke` test
  programs, which players don't need.

### Faster installs

A first install takes about 17 minutes on a 16 GB, 8-thread PC (measured 24
Oct), and a much faster PC barely gains, because most of that time doesn't
use its extra cores or memory:

| Step | Time | Why |
|---|---|---|
| Translating the game's code to C | about 6.5 min | Runs on one core. 3.6 minutes of it is the translator loading its data files before it translates anything (`Loaded ... in 216.2s` in `logs/step5_recomp.log`). The translating itself takes 68 seconds. |
| Compiling the 60 translated files | about 8 min | At most 2 files at a time (1 under 12 GB of RAM), whatever the PC. |
| Unpacking the disc, downloads | about 2 min | Limited by the disk and the network. |

**Plan:**
1. Compile as many files at once as the PC can take, chosen from its RAM and
   threads, with `JOBS=N` (Linux) and `/jobs N` (Windows) to override it.
   First measure GCC's peak memory on the largest translated file
   (`recomp_0000.c`, which takes about a minute alone) instead of trusting the
   current guess of about 2 GB each.
2. Profile the translator's load step (`BatchTranslator` in the toolkit's
   `tools/recomp/`) with Python's `cProfile`. Loading a few JSON files shouldn't
   take minutes, so it's probably a slow loop or repeated work. Not confirmed
   yet.

**Expected:** about 5 minutes on a 32 GB, 16-thread PC, and about 9 on the one
measured.

**Notes:** fixing the load step changes the toolkit, so the update that
brings it re-translates and recompiles everything once. Builds with many
compiles at once put a heavy load on the machine, so test them one at a time.

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
