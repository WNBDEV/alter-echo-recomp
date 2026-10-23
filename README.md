# Alter Echo for PC

**A static recompilation of Alter Echo (Outrage Games / THQ, Xbox, 2003) that
runs natively on Linux.**

The game's own Xbox program is translated to C and compiled for the PC. The
console's hardware -- graphics chip, sound processor, controller, kernel -- is
reimplemented by the [xboxrecomp](https://github.com/sp00nznet/xboxrecomp)
toolkit; this project uses [a fork of it](https://github.com/FILL-IN/xboxrecomp)
that adds a Direct3D 11 graphics backend and the fixes this game needed. It is
not an emulator: the game's code runs as ordinary PC code.

> [!IMPORTANT]
> **No game files are included.** You need your own copy of the North American
> (NTSC) Xbox release, as a disc image or its extracted files. Setup checks that
> it is exactly that release before building anything.

| | |
|---|---|
| **Status** | Playable start to finish (played through on the Wine build) |
| **Frame rate** | 60 fps, as on the console; the heaviest moments can dip (see [Known limits](#known-limits)) |
| **Picture** | 2x the original resolution by default (1280x960), up to 4x; the console's 4:3 with black bars |
| **Input** | Xbox 360-compatible controller with rumble, or keyboard and mouse |
| **Platforms** | Linux with Wine and DXVK (recommended) · native Linux (experimental) · Windows (untested) |

## Contents

- [Requirements](#requirements)
- [Installing (Wine build, recommended)](#installing-wine-build-recommended)
- [Installing (native Linux build, experimental)](#installing-native-linux-build-experimental)
- [Updating](#updating)
- [Playing](#playing)
- [Controls](#controls)
- [Settings](#settings)
- [Troubleshooting](#troubleshooting)
- [Reporting a problem](#reporting-a-problem)
- [Known limits](#known-limits)
- [For testers and developers](#for-testers-and-developers)
- [How it works](#how-it-works)
- [Credits and license](#credits-and-license)

## Requirements

### Hardware

The game itself is light on the graphics card and leans on single-core CPU
speed. Only one machine has been measured (i7-4790K, GTX 960, 16 GB), so the
minimum column is an estimate scaled from it.

| | Minimum (estimated) | Recommended |
|---|---|---|
| **CPU** | 4 threads, per-core speed of a 3rd/4th-generation Core i5 at ~3.5 GHz or a first-generation Ryzen: mostly 60 fps, heavy moments around 30 | Faster per core than an i7-4790K (roughly Intel 8th generation or Ryzen 3000 and newer): steady 60 fps |
| **Graphics** | A GPU with **Vulkan 1.3** drivers, which current DXVK requires: NVIDIA GTX 900 series or newer, AMD Radeon HD 7000 / R7 / R9 or newer on Mesa, Intel 6th-generation (Skylake) graphics or newer | Same; any of these handles 2x resolution or more |
| **Memory** | 4 GB (needed while building; the game uses well under 1 GB) | 8 GB (faster builds) |
| **Disk** | 5 GB (about 3 GB of game files and 2 GB of build) | 5 GB |

Older graphics cards such as NVIDIA's GTX 600/700 series and Intel's 4th/5th
generation graphics lack Vulkan 1.3 and are not supported. On weak integrated
graphics, set `resolution_scale = 1` (the original 640x480).

### Software

| | Wine build | Native Linux build |
|---|---|---|
| **To build** | python3, git, cmake, ninja, MinGW-w64 (`x86_64-w64-mingw32-gcc`) | python3, git, gcc, cmake, ninja, pkg-config, curl, and the SDL2, libepoxy and OpenSSL development files |
| **To play** | Wine and DXVK (`setup_dxvk` or your distribution's dxvk package) | A Vulkan driver; setup fetches DXVK Native and vkd3d into the project folder |

Either setup fetches the xboxrecomp toolkit next to this folder
(`../xboxrecomp`) at the version this project needs, or checks the copy that is
already there.

## Installing (Wine build, recommended)

```bash
git clone https://github.com/FILL-IN/alter-echo-recomp.git
cd alter-echo-recomp
./setup.sh "/path/to/Alter Echo (USA).iso"     # or the folder of extracted game files
./run.sh
```

`setup.sh` takes 20-40 minutes the first time. It:

1. checks your tools, fetches (or checks) the toolkit, and sets up a small
   Python environment in `.venv` (this needs an internet connection);
2. checks your copy of the game is the supported release;
3. unpacks the disc image into `game-files/` (about 3 GB), if you gave it one;
4. extracts the game's code modules and translates all of the game's code to C;
5. compiles it into `build-win/alter_echo_recomp.exe`;
6. creates a Wine prefix with DXVK in `~/.local/share/wineprefixes/alter-echo-recomp`.

Useful variations:

- `CHECK_ONLY=1 ./setup.sh <copy>` checks your copy and tools without building.
- `JOBS=2 ./setup.sh <copy>` compiles two files at a time: faster, if you have
  8 GB of RAM or more.

> [!NOTE]
> The translated C code is generated from your copy of the game. Keep it to
> yourself: it is the game's code in another form.

## Installing (native Linux build, experimental)

The same game compiled as a Linux program. No Wine or MinGW needed.

```bash
./setup-linux.sh "/path/to/Alter Echo (USA).iso"   # or the folder of extracted game files
./run-linux.sh
```

`setup-linux.sh` takes 25-45 minutes. It checks your copy and translates it
(the same steps as `setup.sh`, without the Windows build and the Wine prefix),
fetches DXVK Native and the vkd3d shader compiler into `build-linux/deps`
(nothing is installed system-wide), and compiles. If `setup.sh` has already
translated your copy, `./setup-linux.sh` without the argument reuses that
translation and skips straight to compiling.

vkd3d is fetched automatically on Arch-based systems. Elsewhere, install your
distribution's package first: `libvkd3d-utils1` on Debian/Ubuntu,
`vkd3d-utils` on Fedora. Without it the game still runs, but draws with a
software renderer that is far slower.

Window, sound and controller go through SDL2; graphics use the same Direct3D 11
renderer as the Wine build, on DXVK Native. Both builds share `settings.ini`,
the shader cache and your saves. The native build has been played much less
than the Wine build.

## Updating

```bash
git pull
./setup.sh "/path/to/your/copy"           # the Wine build
./setup-linux.sh "/path/to/your/copy"     # the native build
```

Setup translates and builds again, so the update is complete. If the toolkit next door is older than the
version this project needs, setup stops and prints the command that updates it.

## Playing

Start the game with `./run.sh` (or `./run-linux.sh` for the native build). The
first run creates `settings.ini` from `settings.example.ini`.

| Key | What it does |
|---|---|
| **Alt+Enter** or **F11** | Toggle fullscreen |
| **F1** | Show or hide the frame counter |
| **F2** | Switch between 60 fps and a steady 30 (the game keeps its speed either way) |
| **Click in the window** | Capture the mouse for camera control |
| **Alt+Tab** | Release the mouse |
| **Alt+F4** or the window's close button | Quit the program |

The pause menu's **Quit** only leaves the level for the main menu; save first,
then close the window to exit.

**Saves** are kept where the console keeps them: the `UDATA/` folder inside
your game folder. Both builds use the same saves.

## Controls

A controller and the keyboard and mouse work at the same time. Set
`input = controller` or `input = keyboard` in `settings.ini` to use only one.

| Keyboard and mouse | Controller | In Alter Echo |
|---|---|---|
| W A S D | Left stick | Move |
| Mouse (or I J K L) | Right stick | Camera |
| Space | A | Jump, roll; select in menus |
| Left click | X | Attack (sword combos) |
| Right click | Y | Jump attack in the air, grenade |
| F | B | Gun smash, roll attack; cancel in menus |
| Shift | Left stick click | Block while held; Shift+Space rolls |
| C | White | Block |
| Q / E | Left / right trigger | Morph to gun form / sword form |
| Mouse wheel, Tab | Black | Next gun |
| Middle click, R | Right stick click | Time dilation |
| Enter, Escape | START | Pause; "Done" in the Options menu |
| Backspace | BACK | |
| Arrow keys | D-pad | Menus; picking targets in time dilation |

Good to know:

- **On-screen prompts show controller buttons.** (A) is Space, (B) is F, START
  is Enter or Escape.
- **In the Options menu, B leaves without keeping changes;** START keeps them.
  Settings with choices (Vibration, Pitch Axis) change with left/right, not A.
- **In time dilation, W A S D pick targets too,** as the arrow keys and d-pad
  do. Outside time dilation the d-pad switches targets in a fight, so W A S D
  only do this while time dilation is running.
- **The camera turns and tilts the way you push** the right stick or move the
  mouse. On the Xbox it went the other way (push right, the view turned left;
  push up, it tilted down); `camera_flip_x = 0` and `camera_flip_y = 0` bring
  that back.
- **The mouse acts as the right stick:** moving it faster tilts the stick
  further. A very fast flick turns no faster than the stick pushed all the way.
- **Every key can be rebound** in `settings.ini` (see [Settings](#settings)).

## Settings

All settings live in `settings.ini`. For a one-off change, the launchers also
accept the underlying `RECOMP_*` variables, which win over the file: for
example `RECOMP_SCALE=1 ./run.sh` runs once at the original resolution. Each
variable's name is in `run.sh`.

### Picture

| Setting | Default | What it does |
|---|---|---|
| `resolution_scale` | `2` | Render resolution: `1` = 640x480 (the console), `2` = 1280x960, `3` = 1920x1440, `4` = 2560x1920 |
| `fullscreen` | `0` | `1` starts fullscreen (Alt+Enter or F11 toggles either way) |
| `texture_filtering` | `16` | `0` = top texture level only (distant surfaces shimmer), `1` = mipmaps as the game asks, `2`/`4`/`8`/`16` = plus anisotropic filtering, sharper floors and walls seen at an angle |
| `sharpen` | `0` | `0` (off) to `1` (strongest). The game's textures were made for a 480i TV; `0.3`-`0.5` suits 2x |
| `show_fps` | `0` | `1` shows the frame counter from the start (F1 toggles it) |

### Controller, keyboard and mouse

| Setting | Default | What it does |
|---|---|---|
| `rumble` | `1` | Controller vibration |
| `input` | `auto` | `auto` = controller, keyboard and mouse together; `controller` = controller only; `keyboard` = keyboard and mouse only |
| `mouse_sensitivity` | `1.0` | How fast the mouse turns the camera; `0` turns mouse look off |
| `mouse_invert_y` | `0` | `1` makes the mouse alone tilt the other way |
| `camera_flip_x` | `1` | `1` = the camera turns the way you push; `0` = the console's way |
| `camera_flip_y` | `1` | `1` = the camera tilts the way you push; `0` = the console's way |
| `mouse_deadzone` | `0.40` | Where the smallest mouse movement starts on the stick (the game ignores the first 40% of stick travel). Lower it if small movements do nothing, raise it if the camera jumps when you barely move |

### Key bindings

Each `bind_` setting lists the keys that press one controller control, for
example `bind_a = Space, Enter`. Use `none` for no key.

| Setting | Default | Setting | Default |
|---|---|---|---|
| `bind_move_up` | `W` | `bind_lt` | `Q` |
| `bind_move_down` | `S` | `bind_rt` | `E` |
| `bind_move_left` | `A` | `bind_black` | `WheelUp, WheelDown, Tab` |
| `bind_move_right` | `D` | `bind_white` | `C` |
| `bind_look_up` | `I` | `bind_lstick_click` | `Shift` |
| `bind_look_down` | `K` | `bind_rstick_click` | `MouseMiddle, R` |
| `bind_look_left` | `J` | `bind_start` | `Enter, Escape` |
| `bind_look_right` | `L` | `bind_back` | `Backspace` |
| `bind_a` | `Space` | `bind_dpad_up` | `Up` |
| `bind_b` | `F` | `bind_dpad_down` | `Down` |
| `bind_x` | `MouseLeft` | `bind_dpad_left` | `Left` |
| `bind_y` | `MouseRight` | `bind_dpad_right` | `Right` |

Key names: `A`-`Z`, `0`-`9`, `F3`-`F10`, `F12`, `Space`, `Enter`, `Escape`,
`Tab`, `Backspace`, `Shift`, `Ctrl`, `Alt`, `CapsLock`, `Up`, `Down`, `Left`,
`Right`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`,
`Numpad0`-`Numpad9`, `Minus`, `Equals`, `LeftBracket`, `RightBracket`,
`Semicolon`, `Apostrophe`, `Comma`, `Period`, `Slash`, `Backslash`, `Grave`,
`MouseLeft`, `MouseRight`, `MouseMiddle`, `Mouse4`, `Mouse5`, `WheelUp`,
`WheelDown`. F1, F2 and F11 are taken by the window.

### Advanced

| Setting | Default | What it does |
|---|---|---|
| `direct_render_targets` | `1` | Set to `0` only if a glow, sky or water effect looks broken |

## Troubleshooting

**The mouse doesn't move the camera.** Click inside the game window to capture
the mouse. Check that `mouse_sensitivity` is above `0` and `input` is not
`controller`.

**The controller isn't detected (native build).** Turn it on or plug it in; it
is picked up within a second, even mid-game. The log shows
`[INPUT] pad 1: <name>` when it is found.

**The native build runs at about 20 fps.** On Wayland, a window that is covered
by other windows can be slowed down by the desktop. Keep it visible, or start
with `SDL_VIDEODRIVER=x11 ./run-linux.sh`.

**The native build is very slow everywhere.** The graphics libraries are
missing and the game fell back to its software renderer. Rerun
`./setup-linux.sh` and check its messages about DXVK Native and vkd3d.

**`settings.ini: unknown setting '...' (ignored)`.** A setting name is
misspelled; compare it with `settings.example.ini`.

**Starting over with Wine.** Delete `~/.local/share/wineprefixes/alter-echo-recomp`
and run `./setup.sh` again; it recreates the prefix.

## Reporting a problem

Every launch writes a log to `logs/` (`run-<date>-<time>.txt` for the Wine
build, `native-<date>-<time>.txt` for the native build). The launcher prints
its name when the game closes, and the newest ten are kept.

For a crash, a freeze, a slowdown or a sound problem:

1. Note when it happened and what was going on: the area, a fight, a video,
   how long you had been playing.
2. Zip that log (`zip report.zip logs/run-<date>-<time>.txt`, or your file
   manager's Compress) and attach it to an issue on the project page, with
   what you saw.
3. If it happens in one place, add a save from just before: saves are the
   folders under `UDATA/` in your game folder.

The log starts with the system it ran on (CPU, RAM, graphics card and driver,
OS, and the Wine version for the Wine build) and your settings, so you don't
need to list them. It contains no names, except that in the Wine build one
graphics line includes your account's user name in a Windows-style path
(`C:\users\<name>\...`).

Along the way it records the frame rate every ten seconds, any frame that took
25 ms or more and why, the sound cutting out, memory running short, and the
files the game opened.

## Known limits

- **Frame drops in the heaviest moments.** On an i7-4790K, sword combos landing
  many hits (Fatal Flurry), form swaps and big explosions can drop to 40-48
  fps, and a few areas dip to about 50 at moments (L07_A2a, L06_A9, the
  ending). The cost is in the game's own code on one CPU thread, so a faster
  CPU helps.
- **60 fps is the cap,** as on the console; the game is built around it.
- **Windows is untested,** and the setup scripts are Linux-only.
- **The native Linux build is experimental** and has been played much less than
  the Wine build.
- **The pause menu's Pitch Axis option** changes only one special camera mode,
  not the normal camera; this is how the original game behaves. Use
  `camera_flip_y` instead.
- **The sound is the console's dry mix.** The game uses no reverb or effects.
- **Cutscene close-ups look soft at 2x.** Edges render at full resolution, but
  the textures are the originals, made for a 480i TV, and the cutscene camera
  magnifies them. `sharpen = 0.3` or so helps.

## For testers and developers

### Starting at any level

The game kept its developers' command line, and `RECOMP_ARGS` hands it one:

```bash
RECOMP_ARGS="L05_A1 -bypass" ./run.sh    # straight into area L05_A1, no menus
```

- Area names are the files in your game folder's `ODB Data/ODB MEDIA/clf`
  (`L00_A1` to `L09_A9`).
- Without `-bypass`, the game starts normally and the main menu's Start Game
  goes to the named area instead of the first one.
- The game's other switches pass through too (`-nomovies`, `-noai`,
  `-allowskip`, ...; the full list is in `src/game_args.c`).
- An area started this way has none of a real playthrough's progress, and
  Nevin may not take control (START opens the pause menu, the sticks move
  nothing). Load a save for anything that needs playing.

`tools/level_sweep.sh` uses this to boot every area in turn and report how each
one does.

### More

- **Runtime switches:** every `RECOMP_*` environment variable (rendering,
  audio, input, diagnostics) is listed in the toolkit's README.
- **Regenerating the C code:** `tools/regen_modules.sh` runs the whole
  translation pipeline; `setup.sh` calls it.
- **Development log:** [DEVLOG.md](DEVLOG.md) is the full history of the port:
  how each problem was found and fixed, with every fix numbered.

## How it works

1. **Translation.** Setup reads the game's Xbox executable (`default.xbe`),
   disassembles it and translates every function to C. Alter Echo also loads
   about 300 small code modules (enemy and object scripts) from its data
   files at run time; these are extracted and translated ahead of time too.
2. **Hardware.** The toolkit reimplements what the game expects of the Xbox:
   the kernel's services, memory layout, the controller over USB, the audio
   processor, and the NV2A graphics chip. Graphics commands are turned into
   Direct3D 11 draws, which DXVK runs on Vulkan.
3. **The result** is an ordinary program: the Wine build is a Windows `.exe`
   run through Wine, the native build a Linux executable.

## Credits and license

- The [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) toolkit by sp00nz
  and its contributors, MIT licensed. This project builds on
  [a fork](https://github.com/FILL-IN/xboxrecomp) of it.
- DXVK, vkd3d, Wine, SDL2 and capstone, fetched or used at build time under
  their own licenses.
- This project's own code is MIT licensed (see [LICENSE](LICENSE)).

Alter Echo is a trademark of its respective owner. This project is not
affiliated with or endorsed by the game's developer or publisher, and
includes none of the game's files.
