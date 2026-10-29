"""The second half of AlterEchoInstaller.exe, run with the Python it unpacked.

    python.exe bootstrap.py <install folder> <disc image>

Downloads the free build tools -- WinLibs' MinGW-w64 GCC, which brings CMake
and Ninja, and the capstone disassembler for this Python -- checks each
against the checksum pinned here, then builds the game with
alter-echo-recomp/tools/setup_game.py, the same script Linux uses.
Everything goes into the install folder:

    python/              this Python (from the installer)
    xboxrecomp/          the toolkit's sources (from the installer)
    alter-echo-recomp/   the project: the game files, the build, settings,
                         logs and saves end up in here
    tools/mingw64/       GCC, CMake and Ninja (downloaded)
    install.log          everything printed here
"""
import hashlib
import os
import subprocess
import sys
import time
import urllib.request
import zipfile

# Pinned downloads: (url, sha256, size in bytes). Change them together with
# the checksum, never the URL alone.
WINLIBS = ("https://github.com/brechtsanders/winlibs_mingw/releases/download/"
           "16.2.0posix-23.1.3-14.0.0-ucrt-r3/"
           "winlibs-x86_64-posix-seh-gcc-16.2.0-mingw-w64ucrt-14.0.0-r3.zip",
           "3c69305081e4a647144339a936244fcf7d3d9e558319e1ad7be2cd2886d4b6a1",
           276414382)
# The same capstone version the Linux builds use (tools/setup_game.py), so a
# Windows translation is the same as a Linux one.
CAPSTONE = ("https://files.pythonhosted.org/packages/70/39/"
            "2138d890a8e827636b9de9924fbc8527fe83e38b6d26605b30ac55e30ebe/"
            "capstone-5.0.7-py3-none-win_amd64.whl",
            "4ab8bcb7da8f221ff45926ca168ca33e76f7237d06fbf3c10780002faa2670e1",
            None)

INST = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else ""
LOG = None


def say(msg=""):
    print(msg, flush=True)
    if LOG:
        LOG.write(msg + "\n")
        LOG.flush()


def fail(msg):
    say("\nSETUP FAILED: " + msg)
    say("The log is at " + os.path.join(INST, "install.log"))
    sys.exit(1)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download(what, pin, dest):
    url, want, size = pin
    if os.path.exists(dest) and sha256(dest) == want:
        say("%s: already downloaded" % what)
        return
    say("Downloading %s" % what)
    part = dest + ".part"
    try:
        with urllib.request.urlopen(url, timeout=60) as r, open(part, "wb") as f:
            total = int(r.headers.get("Content-Length") or size or 0)
            done, shown = 0, 0.0
            while True:
                chunk = r.read(1 << 20)
                if not chunk:
                    break
                f.write(chunk)
                done += len(chunk)
                if time.time() - shown > 2 and total:
                    print("  %d of %d MB" % (done >> 20, total >> 20), flush=True)
                    shown = time.time()
    except Exception as e:
        fail("could not download %s (%s). Check the internet connection and run "
             "the installer again; what is already downloaded is kept." % (url, e))
    got = sha256(part)
    if got != want:
        os.remove(part)
        fail("%s did not match its checksum (got %s, expected %s)" % (what, got, want))
    os.replace(part, dest)


def fix_gcc_specs(mingw):
    """Let GCC link from a folder with spaces in its name ("Alter Echo").

    MinGW's built-in specs add %:if-exists(default-manifest.o%s), and GCC
    splits the result of a spec function at spaces, so ld looks for
    "Z:/.../Alter" and "Echo/.../default-manifest.o". The file is always in
    WinLibs, so a specs file next to libgcc.a that names it plainly (one
    argument, spaces kept) replaces the built-in specs.
    """
    gcc = os.path.join(mingw, "bin", "gcc.exe")
    libdir = os.path.dirname(subprocess.check_output([gcc, "-print-libgcc-file-name"],
                                                     text=True).strip())
    specs = os.path.join(libdir, "specs")
    if os.path.exists(specs):
        return
    text = subprocess.check_output([gcc, "-dumpspecs"], text=True)
    old = "%:if-exists(default-manifest.o%s)"
    if old not in text:
        return
    with open(specs, "w", newline="\n") as f:
        f.write(text.replace(old, "default-manifest.o%s"))


def main():
    global LOG
    if len(sys.argv) < 3:
        print("usage: python.exe bootstrap.py <install folder> <disc image>")
        sys.exit(2)
    src = os.path.abspath(sys.argv[2])
    tools = os.path.join(INST, "tools")
    os.makedirs(tools, exist_ok=True)
    LOG = open(os.path.join(INST, "install.log"), "a", encoding="utf-8")
    say("== Alter Echo installer, %s" % time.strftime("%Y-%m-%d %H:%M:%S"))
    say("install folder: %s" % INST)
    say("disc image: %s" % src)

    # 1. capstone, into this Python's site-packages (python314._pth lists it).
    site = os.path.join(os.path.dirname(sys.executable), "Lib", "site-packages")
    if subprocess.call([sys.executable, "-c", "import capstone"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) != 0:
        whl = os.path.join(tools, "capstone-5.0.7-py3-none-win_amd64.whl")
        download("capstone 5.0.7 (the disassembler)", CAPSTONE, whl)
        os.makedirs(site, exist_ok=True)
        with zipfile.ZipFile(whl) as z:
            z.extractall(site)

    # 2. GCC, CMake and Ninja.
    mingw = os.path.join(tools, "mingw64")
    if not os.path.isfile(os.path.join(mingw, "bin", "gcc.exe")):
        z = os.path.join(tools, "winlibs-gcc-16.2.0-ucrt.zip")
        download("the build tools: GCC 16.2, CMake, Ninja (264 MB)", WINLIBS, z)
        say("Unpacking the build tools")
        with zipfile.ZipFile(z) as zf:
            zf.extractall(tools)
        if not os.path.isfile(os.path.join(mingw, "bin", "gcc.exe")):
            fail("the build tools unpacked, but %s has no bin\\gcc.exe" % mingw)
        os.remove(z)
    fix_gcc_specs(mingw)

    # 3. The game itself.
    # setup_game.py picks how many files to compile at once (JOBS=N, which
    # the installer's /jobs N sets, overrides it).
    env = dict(os.environ, PATH=os.path.join(mingw, "bin") + os.pathsep + os.environ.get("PATH", ""))
    say("Building the game (15-30 minutes the first time, an update that changes only the "
        "runtime a few)")
    cmd = [sys.executable, "-u", os.path.join(INST, "alter-echo-recomp", "tools", "setup_game.py"),
           src, "--toolkit", os.path.join(INST, "xboxrecomp")]
    p = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         cwd=os.path.join(INST, "alter-echo-recomp"))
    for raw in p.stdout:
        say(raw.decode("utf-8", "replace").rstrip("\r\n"))
    if p.wait() != 0:
        fail("building the game stopped (exit code %d); the messages above say why" % p.returncode)
    say("\n== Installed. Start Alter Echo from the Start Menu.")


if __name__ == "__main__":
    main()
