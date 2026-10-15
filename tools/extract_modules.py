#!/usr/bin/env python3
"""
Extract Alter Echo's runtime-loaded object modules from the game data.

The game's object scripts (object_Nevin.dll and ~200 others) are PE DLLs
packed inside the .orf and .clf archives, loaded at run time by the game's
own loader. A static recompile of default.xbe never sees them, so they have
to be found, deduplicated and translated separately.

This walks every file under the game directory, finds each embedded PE image
("MZ" + a valid "PE\\0\\0" i386 header), cuts it out by its section extents,
and keeps one copy per distinct content (SHA-1). Output:

    modules/<name>-<sha1[:8]>.dll    one per unique image
    modules/manifest.json            per image: name, sha1, size, image size,
                                     entry RVA, preferred base, the files it
                                     was found in, and how many copies

Usage: python3 tools/extract_modules.py [game_dir] [out_dir]
       (defaults: game/ and modules/, relative to the project root)
"""
import hashlib
import json
import os
import re
import struct
import sys

MZ = re.compile(rb"MZ\x90\x00")
NAME = re.compile(rb"object_[A-Za-z0-9_]+\.dll")


def export_name(img, secs, export_rva):
    """The DLL's own name from its export directory (object_*.dll for object
    scripts, level_*.dll for level scripts), falling back to the first
    object_*.dll string in the image."""
    def file_off(rva):
        for va, vsize, raw_ptr, raw_size in secs:
            if va <= rva < va + max(vsize, raw_size):
                return raw_ptr + (rva - va)
        return None
    if export_rva:
        d = file_off(export_rva)
        if d is not None and d + 16 <= len(img):
            n = file_off(struct.unpack_from("<I", img, d + 12)[0])
            if n is not None:
                e = img.find(b"\0", n)
                if e > n:
                    return img[n:e].decode("latin1")
    m = NAME.search(img)
    return m.group(0).decode() if m else None


def pe_image(data, off):
    """Return (image_bytes, info) for a PE at `off`, or None."""
    try:
        pe = off + struct.unpack_from("<I", data, off + 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            return None
        machine, nsec = struct.unpack_from("<HH", data, pe + 4)
        if machine != 0x14C:
            return None
        opt = pe + 24
        opt_size = struct.unpack_from("<H", data, pe + 20)[0]
        entry = struct.unpack_from("<I", data, opt + 16)[0]
        base = struct.unpack_from("<I", data, opt + 28)[0]
        image_size = struct.unpack_from("<I", data, opt + 56)[0]
        end = struct.unpack_from("<I", data, opt + 60)[0]  # SizeOfHeaders
        sh = opt + opt_size
        secs = []
        for i in range(nsec):
            s = sh + 40 * i
            vsize, va, raw_size, raw_ptr = struct.unpack_from("<IIII", data, s + 8)
            secs.append((va, vsize, raw_ptr, raw_size))
            end = max(end, raw_ptr + raw_size)
        export_rva = struct.unpack_from("<I", data, opt + 96)[0]
    except struct.error:
        return None
    img = data[off:off + end]
    if len(img) != end:
        return None
    return img, {
        "name": export_name(img, secs, export_rva),
        "entry_rva": entry,
        "preferred_base": base,
        "image_size": image_size,
        "file_size": end,
    }


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    game = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "game")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, "modules")
    os.makedirs(out, exist_ok=True)

    uniq = {}
    for dirpath, _, files in os.walk(game, followlinks=True):
        for fn in sorted(files):
            if fn.lower().endswith(".xbe"):
                continue
            path = os.path.join(dirpath, fn)
            with open(path, "rb") as f:
                data = f.read()
            for m in MZ.finditer(data):
                got = pe_image(data, m.start())
                if not got:
                    continue
                img, info = got
                h = hashlib.sha1(img).hexdigest()
                rel = os.path.relpath(path, game)
                if h not in uniq:
                    info["sha1"] = h
                    info["copies"] = 0
                    info["sources"] = []
                    uniq[h] = (img, info)
                info = uniq[h][1]
                info["copies"] += 1
                if rel not in info["sources"]:
                    info["sources"].append(rel)

    manifest = []
    for h, (img, info) in sorted(uniq.items(),
                                 key=lambda kv: (kv[1][1]["name"] or "", kv[0])):
        stem = (info["name"] or "unnamed.dll")[:-4]
        info["file"] = f"{stem}-{h[:8]}.dll"
        with open(os.path.join(out, info["file"]), "wb") as f:
            f.write(img)
        manifest.append(info)
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)

    copies = sum(i["copies"] for i in manifest)
    names = len({i["name"] for i in manifest})
    print(f"{copies} module copies -> {len(manifest)} unique images "
          f"({names} names) in {out}")


if __name__ == "__main__":
    main()
