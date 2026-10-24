#!/usr/bin/env python3
"""Build Alter Echo for this PC from your own copy of the game.

    python3 tools/setup_game.py /path/to/AlterEcho.iso   an Xbox disc image (XISO)
    python3 tools/setup_game.py /path/to/game/folder     the disc's files (default.xbe
                                                         and the "ODB Data" folder)

Nothing from the game is in this repository. This takes your copy, checks it
is the release the project was made for, translates its code to C on this
machine, compiles that and, on Linux, sets up the Wine prefix run.sh uses.

The same script serves every way of installing: setup.sh and setup-linux.sh
call it on Linux, and the Windows installer runs it with its own private
Python, compiler, CMake and Ninja on PATH. Options (the environment variables
of the same name work too, as they did for setup.sh):

    --native        check and translate only (setup-linux.sh compiles)
    --check-only    check the copy and the tools, build nothing
    --translate     translate only, from the game already linked at game/
    --force         build even if default.xbe is not the supported release
    --jobs N        compile N files at a time (default 1: GCC needs ~2 GB each)
    --toolkit DIR   the xboxrecomp toolkit (default: ../xboxrecomp)

Expect 20-40 minutes.
"""
import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

P = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WINDOWS = os.name == "nt"

# The toolkit this project is built against: upstream xboxrecomp plus the work
# done for this game (the Direct3D 11 backend, the fixes in DEVLOG.md), on the
# fork's gpu-backend branch. Upstream alone does not build it. An existing
# checkout is never changed, only checked: it must contain this commit (a
# later one on the same branch is fine).
TOOLKIT_URL = os.environ.get("TOOLKIT_URL", "https://github.com/WNBDEV/xboxrecomp.git")
TOOLKIT_COMMIT = "e1d0435ec93f71fe435b8bf759b0c95bf6b19cf9"

# The release everything here was made against: Alter Echo, NTSC, version 1.
# The function list, the fixes and the addresses in the notes are specific to
# this exact executable, so another release would build and then misbehave.
KNOWN_XBE_SHA1 = "9b6a1b054bfe35f43c6478d46228b58afb759273"

# Code sections besides .text: Microsoft's libraries (DirectSound, Direct3D,
# the controller library XPP, the Bink decoder) live in their own. DOLBY is
# left out on purpose: it is the sound chip's DSP code, not x86.
EXTRA_SECTIONS = ("D3D,D3DX,DSOUND,XPP,BINK,BINK16,BINK16M,BINK16MX,BINK16X2,BINK32,"
                  "BINK32A,BINK32M,BINK32MX,BINK32X2,BINK4444,BINK5551,BINKYUY2")


def say(msg):
    print("\n== " + msg, flush=True)


def fail(msg):
    print("\nsetup: " + msg, file=sys.stderr, flush=True)
    sys.exit(1)


def flag(env, value):
    return value or os.environ.get(env, "0") == "1"


# ---- tools ---------------------------------------------------------------

def check_tools(native):
    need = []
    if not native:
        need += ["cmake", "ninja", "x86_64-w64-mingw32-gcc" if not WINDOWS else "gcc"]
        if not WINDOWS:
            need.append("wine")
    missing = [t for t in need if not shutil.which(t)]
    if missing:
        fail("missing: " + " ".join(missing) + " (install them with your package manager)")


def toolkit_archive_url():
    """github.com/<owner>/<repo>.git -> the zip of the pinned commit."""
    base = TOOLKIT_URL[:-4] if TOOLKIT_URL.endswith(".git") else TOOLKIT_URL
    return base.rstrip("/") + "/archive/" + TOOLKIT_COMMIT + ".zip"


def fetch_toolkit(T):
    say("Fetching the xboxrecomp toolkit into " + T)
    if shutil.which("git"):
        if subprocess.call(["git", "clone", "--quiet", TOOLKIT_URL, T]) != 0:
            fail("could not clone " + TOOLKIT_URL)
        if subprocess.call(["git", "-C", T, "checkout", "--quiet", TOOLKIT_COMMIT]) != 0:
            fail("%s has no commit %s" % (TOOLKIT_URL, TOOLKIT_COMMIT))
        return
    # No git (the Windows installer): the pinned commit as a zip archive.
    url = toolkit_archive_url()
    try:
        data = urllib.request.urlopen(url, timeout=120).read()
    except Exception as e:
        fail("could not download %s: %s" % (url, e))
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        top = z.namelist()[0].split("/")[0]
        z.extractall(os.path.dirname(T) or ".")
    os.rename(os.path.join(os.path.dirname(T) or ".", top), T)


def check_toolkit(T):
    if not os.path.exists(T):
        fetch_toolkit(T)
    if not os.path.isdir(os.path.join(T, "tools", "recomp")):
        fail("%s is not the xboxrecomp toolkit (set XBOXRECOMP= or --toolkit)" % T)
    if os.path.isdir(os.path.join(T, ".git")) and shutil.which("git"):
        if subprocess.call(["git", "-C", T, "merge-base", "--is-ancestor", TOOLKIT_COMMIT, "HEAD"],
                           stderr=subprocess.DEVNULL) != 0:
            fail('the toolkit at %s does not contain %s, the version this project needs. '
                 'Update it: git -C "%s" fetch && git -C "%s" checkout %s'
                 % (T, TOOLKIT_COMMIT, T, T, TOOLKIT_COMMIT))
    else:
        # A copy without its history (an archive): the next best check.
        try:
            with open(os.path.join(T, "src", "kernel", "xbox_memory_layout.c"),
                      encoding="utf-8", errors="replace") as f:
                ok = "xbox_PhysicalOrigin" in f.read()
        except OSError:
            ok = False
        if not ok:
            fail("the toolkit at %s is too old for this project" % T)


def has_capstone(py):
    try:
        return subprocess.call([py, "-c", "import capstone"], stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL) == 0
    except OSError:
        return False


def find_python():
    """A Python with capstone, which the disassembler needs.

    On Linux only a known environment is used, so a system capstone of
    another version cannot change the translation. On Windows the Python
    running this (the installer's own, with capstone) comes first."""
    venv_py = os.path.join(P, ".venv", "Scripts" if WINDOWS else "bin",
                           "python.exe" if WINDOWS else "python")
    cands = [os.environ.get("PY", "")]
    if WINDOWS:
        cands.append(sys.executable)
    cands += [os.path.expanduser("~/.venvs/xboxrecomp/bin/python"), venv_py]
    for c in cands:
        if c and os.path.isfile(c) and has_capstone(c):
            return c
    say("Creating a Python environment with capstone (.venv)")
    if subprocess.call([sys.executable, "-m", "venv", os.path.join(P, ".venv")]) != 0:
        fail("python -m venv failed (install Python's venv module)")
    # Pinned, and the same version the Windows installer fetches, so every
    # install translates the game the same way.
    if subprocess.call([venv_py, "-m", "pip", "install", "--quiet", "capstone==5.0.7"]) != 0:
        fail("could not install capstone")
    return venv_py


# ---- the game files ------------------------------------------------------

def check_xbe(path, force):
    with open(path, "rb") as f:
        sha = hashlib.sha1(f.read()).hexdigest()
    if sha != KNOWN_XBE_SHA1:
        if force:
            print("default.xbe is %s, not the supported release; continuing (--force)" % sha)
        else:
            fail("default.xbe is %s; this project supports the NTSC release (%s). "
                 "--force tries anyway, unsupported." % (sha, KNOWN_XBE_SHA1))
    else:
        print("default.xbe matches the supported release (NTSC, title 5451000F)")


def check_release(src, T, py, force):
    say("Checking it is the supported release")
    if os.path.isfile(src):
        tmp = tempfile.mkdtemp()
        try:
            r = subprocess.call([py, "-m", "tools.xiso", "get", src, "default.xbe", "-o", tmp],
                                cwd=T, stdout=subprocess.DEVNULL)
            if r != 0:
                fail("could not read %s as an Xbox disc image" % src)
            check_xbe(os.path.join(tmp, "default.xbe"), force)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    elif os.path.isdir(src):
        if not os.path.isfile(os.path.join(src, "default.xbe")):
            fail("no default.xbe in " + src)
        check_xbe(os.path.join(src, "default.xbe"), force)
    else:
        fail(src + " is neither a file nor a folder")


def link_game(game):
    """game/ -> the game files. A symbolic link on Linux; on Windows a
    junction, which needs no administrator rights or Developer Mode."""
    link = os.path.join(P, "game")
    if WINDOWS:
        if os.path.isdir(link):
            try:
                os.rmdir(link)              # an old junction goes; its target stays
            except OSError:
                fail("there is already a game folder at %s; move it away first" % link)
        if subprocess.call(["cmd", "/c", "mklink", "/J", link, game],
                           stdout=subprocess.DEVNULL) != 0:
            fail("could not link %s to %s" % (link, game))
    else:
        if os.path.islink(link):
            os.remove(link)
        elif os.path.exists(link):
            fail("there is already a game folder at %s; move it away first" % link)
        os.symlink(game, link)


# ---- translation (what tools/regen_modules.sh did) ----------------------

def run_logged(cmd, cwd, log):
    with open(log, "w") as f:
        r = subprocess.call(cmd, cwd=cwd, stdout=f, stderr=subprocess.STDOUT)
    if r != 0:
        fail("%s failed; see %s" % (" ".join(os.path.basename(c) for c in cmd[:3]), log))


def translate(T, py):
    """default.xbe plus every runtime-loaded module, to C in src/recomp/gen.

    1. tools/build_modules.py -> build-modules/default_modules.xbe, the seeds,
       src/module_layout.c (needs modules/ from tools/extract_modules.py);
    2. the pipeline on that XBE, with the module sections as extra code
       sections and the engine and module seeds merged;
    3. tools.recomp with --exclude-manual on src/module_loader.c and
       src/game_args.c (joined: it takes one file), so the functions they wrap
       (sub_0011D860, the module loader; sub_000164F0, the command-line parser)
       are emitted as sub_X_gen, and --manual-functions for the modules'
       import thunk slots, which src/module_thunks.c defines."""
    X = os.path.join(P, "build-modules", "default_modules.xbe")
    L = os.path.join(P, "logs")
    B = os.path.join(P, "build-modules")
    os.makedirs(L, exist_ok=True)

    if subprocess.call([py, os.path.join(P, "tools", "build_modules.py"),
                        os.path.join(P, "game", "default.xbe")]) != 0:
        fail("tools/build_modules.py failed")

    seen, out = set(), []
    for f in (os.path.join(P, "seed_functions.json"), os.path.join(P, "modules", "module_seeds.json")):
        with open(f) as fh:
            for e in json.load(fh):
                a = int(e["start"], 16)
                if a not in seen:
                    seen.add(a)
                    out.append(e)
    with open(os.path.join(B, "all_seeds.json"), "w") as fh:
        json.dump(out, fh, indent=1)
    print("%d seeds" % len(out))

    with open(os.path.join(B, "extra_sections.txt")) as fh:
        secs = EXTRA_SECTIONS + "," + fh.read().rstrip("\n")

    run_logged([py, "-m", "tools.xbe_parser", X, "--json",
                os.path.join(B, "default_modules_analysis.json")], T,
               os.path.join(L, "step1_xbe_modules.log"))
    run_logged([py, "-m", "tools.disasm", X, "-v", "--force", "--text-only",
                "--extra-sections", secs, "--seed-functions", os.path.join(B, "all_seeds.json")],
               T, os.path.join(L, "step3_disasm.log"))
    run_logged([py, "-m", "tools.func_id", X, "-v"], T, os.path.join(L, "step4_func_id.log"))
    run_logged([py, "-m", "tools.abi_analysis", X, "-v"], T, os.path.join(L, "step4.5_abi.log"))
    # Names from the project's own map (tools/ghidra_names.json: the 196 MSVC
    # runtime routines Ghidra recognised), not the toolkit's Ghidra export,
    # which is local to the machine that ran Ghidra.
    run_logged([py, os.path.join("tools", "ghidra_naming", "merge_names.py"), "--apply",
                "--names-json", os.path.join(P, "tools", "ghidra_names.json")],
               T, os.path.join(L, "step4.6_merge.log"))

    gen = os.path.join(P, "src", "recomp", "gen")
    if os.path.isdir(gen):
        for name in os.listdir(gen):
            if name.startswith("recomp_") and name.endswith(".c"):
                os.remove(os.path.join(gen, name))
    with open(os.path.join(B, "manual_wraps.c"), "wb") as w:
        for f in ("module_loader.c", "game_args.c"):
            with open(os.path.join(P, "src", f), "rb") as r:
                w.write(r.read())
    run_logged([py, "-m", "tools.recomp", X, "--all", "--split", "250", "--gen-dir", gen,
                "--exclude-manual", os.path.join(B, "manual_wraps.c"),
                "--manual-functions", os.path.join(B, "thunk_functions.json")],
               T, os.path.join(L, "step5_recomp.log"))

    for log, keys in (("step3_disasm.log", ("Total functions",)),
                      ("step5_recomp.log", ("functions (", "unresolved call"))):
        with open(os.path.join(L, log), errors="replace") as fh:
            for line in fh:
                if any(k in line for k in keys):
                    print(line.rstrip())


# ---- compile and the Wine prefix ----------------------------------------

def compile_game(T, jobs):
    say("Compiling (10-20 minutes)")
    cfg = ["cmake", "-S", P, "-B", os.path.join(P, "build-win"), "-G", "Ninja",
           "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_FLAGS_RELEASE=-O2", "-DXBOXRECOMP_DIR=" + T]
    if not WINDOWS:
        # On Linux MinGW is a cross-compiler, named by the toolkit's toolchain
        # file; on Windows it is the native compiler and CMake finds it.
        cfg.append("-DCMAKE_TOOLCHAIN_FILE=" + os.path.join(T, "cmake", "mingw-w64-x86_64.cmake"))
    if subprocess.call(cfg) != 0:
        fail("configuring the build failed")
    if subprocess.call(["cmake", "--build", os.path.join(P, "build-win"), "-j", str(jobs)]) != 0:
        fail("the build failed (if it died with a signal, memory ran out: --jobs 1)")


def wine_prefix():
    prefix = os.path.expanduser("~/.local/share/wineprefixes/alter-echo-recomp")
    say("Setting up the Wine prefix with DXVK (%s)" % prefix)
    env = dict(os.environ, WINEPREFIX=prefix, WINEDEBUG="-all")
    os.makedirs(prefix, exist_ok=True)
    # Without Wine Mono and Gecko (.NET and the HTML engine; the game uses
    # neither), or a new prefix stops on a dialog offering to download them.
    subprocess.call(["wineboot", "-u"], env=dict(env, WINEDLLOVERRIDES="mscoree,mshtml="),
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    dxvk = shutil.which("setup_dxvk") or (
        "/usr/share/dxvk/setup_dxvk.sh" if os.access("/usr/share/dxvk/setup_dxvk.sh", os.X_OK) else None)
    if dxvk:
        subprocess.call([dxvk, "install"], env=env, stdout=subprocess.DEVNULL)
    else:
        print("DXVK not found: install it (setup_dxvk, or winetricks dxvk) into %s," % prefix)
        print("or the game falls back to Wine's slower Direct3D.")


# ---- main ----------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="Build Alter Echo from your own copy of the game.")
    ap.add_argument("source", nargs="?", help="the disc image (.iso) or the extracted game folder")
    ap.add_argument("--native", action="store_true")
    ap.add_argument("--check-only", action="store_true")
    ap.add_argument("--translate", action="store_true")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--jobs", type=int, default=int(os.environ.get("JOBS", "1")))
    ap.add_argument("--toolkit", default=os.environ.get("XBOXRECOMP", os.path.join(P, "..", "xboxrecomp")))
    a = ap.parse_args()
    native, check_only, force = flag("NATIVE", a.native), flag("CHECK_ONLY", a.check_only), flag("FORCE", a.force)
    T = os.path.abspath(a.toolkit)
    src = os.path.abspath(a.source) if a.source else None
    os.chdir(P)                         # as setup.sh did: some steps expect it

    if not WINDOWS and os.geteuid() == 0:
        fail("run this as your own user, not root")
    if a.translate:                     # tools/regen_modules.sh
        py = os.environ.get("PY") or find_python()
        translate(T, py)
        return
    if not src:
        fail("usage: setup_game.py <game.iso | extracted game folder>")

    say("Checking tools")
    check_tools(native)
    check_toolkit(T)
    py = find_python()
    os.environ["PY"] = py

    check_release(src, T, py, force)
    if check_only:
        say("Your copy and tools are fine (--check-only: stopping here)")
        return

    if os.path.isfile(src):
        say("Unpacking the disc image into game-files (about 3 GB)")
        game = os.path.join(P, "game-files")
        os.makedirs(game, exist_ok=True)
        if subprocess.call([py, "-m", "tools.xiso", "unpack", src, "-o", game], cwd=T) != 0:
            fail("could not unpack " + src)
    else:
        game = src
    link_game(game)

    say("Extracting the game's runtime-loaded modules")
    if subprocess.call([py, os.path.join(P, "tools", "extract_modules.py"),
                        os.path.join(P, "game"), os.path.join(P, "modules")]) != 0:
        fail("tools/extract_modules.py failed")

    say("Translating the game's code to C (5-10 minutes)")
    translate(T, py)
    if native:
        say("Translated (--native: no Windows build or Wine prefix)")
        return

    compile_game(T, a.jobs)
    if WINDOWS:
        say("Done")
        print("Play: double-click run.bat (settings: settings.ini, made on first run)")
        return
    wine_prefix()
    say("Done")
    print("Play with ./run.sh  (settings: settings.ini, made on first run)")


if __name__ == "__main__":
    main()
