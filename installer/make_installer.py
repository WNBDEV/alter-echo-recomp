#!/usr/bin/env python3
"""Build the installers (on Linux, with MinGW-w64 for the Windows one).

    python3 installer/make_installer.py [--worktree]

dist/AlterEchoInstaller.exe is installer/installer.c compiled, with a payload
appended:

    python/              Python's embeddable package for Windows (pinned below)
    alter-echo-recomp/   this project at HEAD (--worktree: the working tree,
                         tracked and new files, for testing before a commit)
    xboxrecomp/          the toolkit at the commit tools/setup_game.py pins
    bootstrap.py         installer/bootstrap.py

dist/AlterEchoInstaller.sh is installer/install-linux.sh with a .tar.gz of
alter-echo-recomp/ and xboxrecomp/ appended (Linux brings its own Python).

Each gets only its own platform's files from the project (WINDOWS_ONLY and
LINUX_ONLY below); the toolkit goes in whole.

No game files go in: the installers build the game from the player's disc.
"""
import hashlib
import io
import os
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import time
import urllib.request
import zipfile

# Payload paths that are executable (scripts), for the Linux archive.
EXECUTABLE = set()

# Project files one installer leaves out because only the other platform, a
# from-source Wine build or development uses them. installer/ is in neither:
# the Windows one carries bootstrap.py on its own.
DEV_ONLY = ("installer/", "tools/level_sweep.sh")
WINDOWS_ONLY = ("run.bat",)
LINUX_ONLY = (".sh",)                       # setup-linux.sh, run-linux.sh, ...
WINE_ONLY = ("setup.sh", "run.sh", "build.sh")


def project_files_for(project, platform):
    keep = {}
    for name, data in project.items():
        rel = name[len("alter-echo-recomp/"):]
        if rel.startswith(DEV_ONLY):
            continue
        if platform == "windows" and rel.endswith(LINUX_ONLY):
            continue
        if platform == "linux" and (rel in WINDOWS_ONLY or rel in WINE_ONLY):
            continue
        keep[name] = data
    return keep

P = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = os.path.join(os.path.dirname(P), "xboxrecomp")
OUT = os.path.join(P, "build-installer")
DIST = os.path.join(P, "dist")

PY_EMBED = ("https://www.python.org/ftp/python/3.14.9/python-3.14.9-embed-amd64.zip",
            "3c01d7cfc666f746a6f064f60fb2156ab0ec464d3d738ab796371a46a9bb24a2")

# The embeddable package only looks where its ._pth file says. Besides the
# standard library: site-packages, where bootstrap.py puts capstone, and the
# toolkit, so `python -m tools.recomp` finds it from any folder.
PTH = "python314.zip\n.\nLib\\site-packages\n..\\xboxrecomp\nimport site\n"


def fail(msg):
    sys.exit("make_installer: " + msg)


def git(args, cwd):
    return subprocess.run(["git"] + args, cwd=cwd, check=True, stdout=subprocess.PIPE).stdout


def python_embed():
    cache = os.path.join(OUT, "cache", os.path.basename(PY_EMBED[0]))
    if not os.path.exists(cache):
        os.makedirs(os.path.dirname(cache), exist_ok=True)
        with urllib.request.urlopen(PY_EMBED[0]) as r, open(cache, "wb") as f:
            f.write(r.read())
    with open(cache, "rb") as f:
        data = f.read()
    if hashlib.sha256(data).hexdigest() != PY_EMBED[1]:
        fail("%s does not match its pinned checksum" % cache)
    files = {}
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for name in z.namelist():
            if not name.endswith("/"):
                files["python/" + name] = z.read(name)
    files["python/python314._pth"] = PTH.replace("\n", "\r\n").encode()
    return files


def from_archive(repo, ref, prefix):
    files = {}
    data = git(["archive", "--format=tar", ref], repo)
    with tarfile.open(fileobj=io.BytesIO(data)) as t:
        for m in t.getmembers():
            if m.isfile():
                files[prefix + m.name] = t.extractfile(m).read()
                if m.mode & 0o111:
                    EXECUTABLE.add(prefix + m.name)
    return files


def from_worktree(repo, prefix):
    files = {}
    names = git(["ls-files", "--cached", "--others", "--exclude-standard", "-z"], repo).split(b"\0")
    for n in names:
        n = n.decode()
        path = os.path.join(repo, n)
        if n and os.path.isfile(path):
            with open(path, "rb") as f:
                data = f.read()
            files[prefix + n] = data
            if os.stat(path).st_mode & 0o111:
                EXECUTABLE.add(prefix + n)
    return files


def toolkit_commit():
    with open(os.path.join(P, "tools", "setup_game.py")) as f:
        m = re.search(r'^TOOLKIT_COMMIT = "([0-9a-f]{40})"', f.read(), re.M)
    if not m:
        fail("no TOOLKIT_COMMIT in tools/setup_game.py")
    return m.group(1)


def compile_stub():
    stub = os.path.join(OUT, "stub.exe")
    subprocess.run(["x86_64-w64-mingw32-gcc", "-O2", "-municode", "-mwindows", "-s",
                    os.path.join(P, "installer", "installer.c"), "-o", stub,
                    "-lcomdlg32", "-lshell32", "-lole32", "-luuid"], check=True)
    with open(stub, "rb") as f:
        return f.read()


def windows_installer(project, toolkit):
    files = python_embed()
    for name, data in list(project.items()) + list(toolkit.items()):
        if name.endswith(".bat"):
            data = data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
        files[name] = data
    with open(os.path.join(P, "installer", "bootstrap.py"), "rb") as f:
        files["bootstrap.py"] = f.read()

    stub = compile_stub()
    out = os.path.join(DIST, "AlterEchoInstaller.exe")
    with open(out, "wb") as f:
        f.write(stub)
        for name in sorted(files):
            raw = name.encode("utf-8")
            f.write(struct.pack("<I", len(raw)) + raw + struct.pack("<Q", len(files[name])))
            f.write(files[name])
        f.write(struct.pack("<Q", len(stub)) + b"AEINST01")
    total = sum(len(v) for v in files.values())
    print("%s: %d files, %.1f MB (payload %.1f MB)" % (out, len(files),
          os.path.getsize(out) / 1e6, total / 1e6))


def linux_installer(project, toolkit):
    files = dict(project, **toolkit)
    buf = io.BytesIO()
    now = int(time.time())
    with tarfile.open(fileobj=buf, mode="w:gz", format=tarfile.PAX_FORMAT) as t:
        for name in sorted(files):
            info = tarfile.TarInfo(name)
            info.size = len(files[name])
            info.mode = 0o755 if name in EXECUTABLE else 0o644
            info.mtime = now
            t.addfile(info, io.BytesIO(files[name]))
    with open(os.path.join(P, "installer", "install-linux.sh"), "rb") as f:
        head = f.read()
    if not head.endswith(b"\n__ARCHIVE_BELOW__\n"):
        fail("installer/install-linux.sh must end with the __ARCHIVE_BELOW__ line")
    out = os.path.join(DIST, "AlterEchoInstaller.sh")
    with open(out, "wb") as f:
        f.write(head + buf.getvalue())
    os.chmod(out, 0o755)
    print("%s: %d files, %.1f MB" % (out, len(files), os.path.getsize(out) / 1e6))


def main():
    worktree = "--worktree" in sys.argv[1:]
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(DIST, exist_ok=True)
    if not worktree and git(["status", "--porcelain", "--untracked-files=no"], P).strip():
        print("note: uncommitted changes are not included (HEAD is); --worktree includes them")

    project = (from_worktree(P, "alter-echo-recomp/") if worktree
               else from_archive(P, "HEAD", "alter-echo-recomp/"))
    toolkit = from_archive(T, toolkit_commit(), "xboxrecomp/")
    windows_installer(project_files_for(project, "windows"), toolkit)
    linux_installer(project_files_for(project, "linux"), toolkit)


if __name__ == "__main__":
    main()
