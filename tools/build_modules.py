#!/usr/bin/env python3
"""
Lay out Alter Echo's runtime-loaded modules at fixed guest addresses, for
translation ahead of time and for the loader hook that puts them there.

The game loads its object/level/trigger scripts (PE DLLs, see
extract_modules.py) with its own loader, sub_0011D860: it reads the sections
back to back into a heap buffer ("image") and relocates them with delta
image - rawptr0 - ImageBase, every site at image + site_rva - rawptr0. Code that
lands at a heap address cannot be translated ahead of time. So each module is
given a fixed effective base B (RVA r lives at B + r), its code is translated
there, and at run time src/module_loader.c moves the freshly loaded image to
B + rawptr0 and re-applies the relocations for B.

Inputs : modules/manifest.json + modules/*.dll (from extract_modules.py),
         the original default.xbe
Outputs: build-modules/default_modules.xbe  the XBE plus one executable
             section per module (M0001...) holding its .text relocated to its
             base. Only the translation pipeline reads it; the game still
             runs the original XBE.
         build-modules/extra_sections.txt  those section names, for
             tools.disasm --extra-sections
         modules/layout.json        name -> base, sizes, export addresses
         modules/module_seeds.json  every export and entry point, as trusted
             function seeds for tools.disasm --seed-functions
         src/module_layout.c        the table the runtime hook uses: base,
             packed length and relocation sites per module
         src/module_thunks.c, build-modules/thunk_functions.json
                                    the import thunk slots module code calls,
             as hand-written functions (see thunk_slots)

Usage: python3 tools/build_modules.py [xbe]   (default: game/default.xbe)
"""
import json
import os
import struct
import sys

MODULE_REGION = 0x74000000      # above the RAM mirrors; needs RECOMP_EXT_VMA
SLOT_ALIGN = 0x10000
XBE_SECTION_PRELOAD_EXEC = 0x06  # preload | executable


def parse_pe(d):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    opt = pe + 24
    sh = opt + struct.unpack_from("<H", d, pe + 20)[0]
    image_base = struct.unpack_from("<I", d, opt + 28)[0]
    image_size = struct.unpack_from("<I", d, opt + 56)[0]
    entry = struct.unpack_from("<I", d, opt + 16)[0]
    export_rva, export_size = struct.unpack_from("<II", d, opt + 96)
    reloc_rva, reloc_size = struct.unpack_from("<II", d, opt + 96 + 5 * 8)
    secs = []
    for i in range(nsec):
        s = sh + 40 * i
        name = d[s:s + 8].rstrip(b"\0").decode()
        vsize, va, rsize, rptr = struct.unpack_from("<IIII", d, s + 8)
        secs.append(dict(name=name, va=va, vsize=vsize, rsize=rsize, rptr=rptr))
    return dict(image_base=image_base, image_size=image_size, entry=entry,
                export_rva=export_rva, reloc_rva=reloc_rva,
                reloc_size=reloc_size, secs=secs)


def rva_to_off(pe, rva):
    for s in pe["secs"]:
        if s["va"] <= rva < s["va"] + max(s["vsize"], s["rsize"]):
            return s["rptr"] + (rva - s["va"])
    raise ValueError(f"RVA {rva:#x} in no section")


def relocations(d, pe):
    """HIGHLOW site RVAs from .reloc (the only type these DLLs use)."""
    sites = []
    off = rva_to_off(pe, pe["reloc_rva"])
    end = off + pe["reloc_size"]
    while off + 8 <= end:
        page, size = struct.unpack_from("<II", d, off)
        if size < 8:
            break
        for i in range((size - 8) // 2):
            e = struct.unpack_from("<H", d, off + 8 + 2 * i)[0]
            kind, rel = e >> 12, e & 0xFFF
            if kind == 0:
                continue
            if kind != 3:
                raise ValueError(f"relocation type {kind} not handled")
            sites.append(page + rel)
        off += size
    return sites


def exports(d, pe):
    """{name or '#ordinal': rva} from the export directory."""
    out = {}
    if not pe["export_rva"]:
        return out
    e = rva_to_off(pe, pe["export_rva"])
    base_ord, nfunc, nname, af, an, ao = struct.unpack_from("<IIIIII", d, e + 16)
    funcs = [struct.unpack_from("<I", d, rva_to_off(pe, af) + 4 * i)[0]
             for i in range(nfunc)]
    named = set()
    for i in range(nname):
        nrva = struct.unpack_from("<I", d, rva_to_off(pe, an) + 4 * i)[0]
        ordi = struct.unpack_from("<H", d, rva_to_off(pe, ao) + 2 * i)[0]
        no = rva_to_off(pe, nrva)
        name = d[no:d.index(b"\0", no)].decode()
        out[name] = funcs[ordi]
        named.add(ordi)
    for i, rva in enumerate(funcs):
        if i not in named and rva:
            out[f"#{base_ord + i}"] = rva
    return out


def thunk_slots(d, pe, B):
    """Import thunk slots that the module's code calls or jumps to.

    Each module's .text opens with a run of int3: 5-byte slots that its
    DLLFixupNames export overwrites at load time with `jmp <engine function>`
    (E9 rel32), one per engine import it resolved by name. Translated ahead of
    time they are int3, so a call into one did nothing and left the guest
    stack 4 bytes short. They become hand-written functions instead
    (src/module_thunks.c -> ae_dyn_thunk), which read the patched jmp at run
    time and dispatch to its target."""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    text = pe["secs"][0]
    code = d[text["rptr"]:text["rptr"] + text["vsize"]]
    run = 0
    while run < len(code) and code[run] == 0xCC:
        run += 1
    lo, hi = B + text["va"], B + text["va"] + run - run % 5
    found = set()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.skipdata = True
    for ins in md.disasm(code, B + text["va"]):
        if ins.mnemonic in ("call", "jmp") and ins.op_str.startswith("0x"):
            t = int(ins.op_str, 16)
            if lo <= t < hi and (t - lo) % 5 == 0:
                found.add(t)
    return sorted(found)


def code_pointers(d, pe, sites, B, thunk_hi):
    """Function entries that are only reached through pointers.

    The relocation table lists every absolute address in the image, so a
    relocated dword that points into .text is a code address. In .rdata and
    .data those are vtable slots and callback tables -- functions nothing
    calls directly, which the disassembler cannot find and which otherwise
    fail at run time ("[ICALL] Failed to resolve"), leaking the stack bytes
    their callee would have popped. In .text, `push imm32` and
    `mov reg, imm32` immediates are callbacks being passed or stored; the other
    relocated dwords there are mostly switch tables, whose entries are labels
    inside a function, and are left alone."""
    base = pe["image_base"]
    text = pe["secs"][0]
    t_lo, t_hi = base + text["va"], base + text["va"] + text["vsize"]
    out = set()
    for site in sites:
        off = rva_to_off(pe, site)
        v = struct.unpack_from("<I", d, off)[0]
        if not (t_lo <= v < t_hi):
            continue
        target = B + (v - base)
        if target < thunk_hi:
            continue
        in_text = text["va"] <= site < text["va"] + text["vsize"]
        if in_text:
            op = d[off - 1]
            if not (op == 0x68 or 0xB8 <= op <= 0xBF):
                continue
        out.add(target)
    return out


def build_xbe(xbe, texts, out_path):
    """The XBE plus one executable section per module's relocated .text.

    `texts` is [(name, va, bytes)]. One section each rather than one region
    for all: the region is mostly gaps between slots, and the disassembler
    sweeps every byte of a code section."""
    d = bytearray(xbe)
    base = struct.unpack_from("<I", d, 0x104)[0]
    nsec = struct.unpack_from("<I", d, 0x11C)[0]
    hdr_va = struct.unpack_from("<I", d, 0x120)[0]
    old = bytes(d[hdr_va - base:hdr_va - base + 56 * nsec])

    def pad(n):
        d.extend(b"\0" * (-len(d) % n))

    pad(0x1000)
    headers = bytearray()
    names_at = []
    for name, va, img in texts:
        off = len(d)
        d.extend(img)
        pad(16)
        names_at.append((name, va, off, len(img)))
    for name, va, off, size in names_at:
        noff = len(d)
        d.extend(name.encode() + b"\0")
        headers += struct.pack("<IIIIIIIII", XBE_SECTION_PRELOAD_EXEC, va, size,
                               off, size, base + noff, 0, 0, 0) + b"\0" * 20
    # The XBE repeats some section names (Alter Echo has four BINKYUY2), and
    # tools.disasm --extra-sections finds sections by name, so all but one of
    # a repeated name went undisassembled -- their functions became
    # unresolved calls the first time a Bink video played. Give repeats their
    # own names in this copy: NAME_2, NAME_3...
    old = bytearray(old)
    seen, renamed = {}, []
    for i in range(nsec):
        name_va = struct.unpack_from("<I", old, 56 * i + 20)[0]
        o = name_va - base
        name = bytes(d[o:d.index(b"\0", o)]).decode("latin1")
        seen[name] = seen.get(name, 0) + 1
        if seen[name] > 1:
            new_name = f"{name}_{seen[name]}"
            noff = len(d)
            d.extend(new_name.encode() + b"\0")
            struct.pack_into("<I", old, 56 * i + 20, base + noff)
            renamed.append(new_name)
    pad(4)
    arr_off = len(d)
    d.extend(bytes(old) + headers)
    # The parser reads section headers and names as file offsets from the
    # base address, so both can live past the 4 KB header region.
    struct.pack_into("<I", d, 0x11C, nsec + len(texts))
    struct.pack_into("<I", d, 0x120, base + arr_off)
    with open(out_path, "wb") as f:
        f.write(d)
    return renamed


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    xbe_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "game", "default.xbe")
    mdir = os.path.join(root, "modules")
    manifest = json.load(open(os.path.join(mdir, "manifest.json")))

    cursor = MODULE_REGION
    layout, seeds, c_mods, thunks = [], [], [], []
    total_sites = 0
    for m in sorted(manifest, key=lambda m: m["name"]):
        d = open(os.path.join(mdir, m["file"]), "rb").read()
        pe = parse_pe(d)
        rawptr0 = pe["secs"][0]["rptr"]
        text = pe["secs"][0]
        if text["name"] != ".text" or text["rptr"] != text["va"]:
            raise ValueError(f"{m['name']}: first section is not .text at raw == RVA")
        packed = sum(s["rsize"] for s in pe["secs"])
        B = cursor
        cursor += (max(pe["image_size"], rawptr0 + packed) + SLOT_ALIGN - 1) & ~(SLOT_ALIGN - 1)
        sites = relocations(d, pe)
        ex = exports(d, pe)
        slots = thunk_slots(d, pe, B)
        thunks.extend(slots)
        text0 = pe["secs"][0]
        run = 0
        while d[text0["rptr"] + run] == 0xCC:
            run += 1
        ptrs = code_pointers(d, pe, sites, B, B + text0["va"] + run)
        ptr_seeds = sorted(ptrs - {B + rva for rva in ex.values()} - {B + pe["entry"]})
        layout.append(dict(name=m["name"], file=m["file"], base=B,
                           rawptr0=rawptr0, packed=packed,
                           image_size=pe["image_size"],
                           text=[B + text["va"], text["vsize"]],
                           entry=B + pe["entry"],
                           exports={k: B + v for k, v in sorted(ex.items())},
                           relocations=len(sites)))
        for name, rva in sorted(set(ex.items()) | {("entry", pe["entry"])}):
            seeds.append({"start": f"0x{B + rva:08X}", "observed": True,
                          "note": f"{m['name']} {name}"})
        for a in ptr_seeds:
            seeds.append({"start": f"0x{a:08X}", "observed": True,
                          "note": f"{m['name']} code pointer (relocated)"})
        c_mods.append((m["name"], B, rawptr0, packed, [s - rawptr0 for s in sites],
                       d, pe, sites, text))
        total_sites += len(sites)

    span = cursor - MODULE_REGION
    texts = []
    for k, (name, B, rawptr0, packed, offs, d, pe, sites, text) in enumerate(c_mods):
        img = bytearray(d[text["rptr"]:text["rptr"] + text["vsize"]])
        delta = (B - pe["image_base"]) & 0xFFFFFFFF
        for s in sites:
            if text["va"] <= s and s + 4 <= text["va"] + text["vsize"]:
                o = s - text["va"]
                v = struct.unpack_from("<I", img, o)[0]
                struct.pack_into("<I", img, o, (v + delta) & 0xFFFFFFFF)
        texts.append((f"M{k + 1:04d}", B + text["va"], bytes(img)))

    os.makedirs(os.path.join(root, "build-modules"), exist_ok=True)
    xbe_out = os.path.join(root, "build-modules", "default_modules.xbe")
    renamed = build_xbe(open(xbe_path, "rb").read(), texts, xbe_out)
    with open(os.path.join(root, "build-modules", "extra_sections.txt"), "w") as f:
        f.write(",".join(renamed + [n for n, _, _ in texts]))
    json.dump(layout, open(os.path.join(mdir, "layout.json"), "w"), indent=1)
    json.dump(seeds, open(os.path.join(mdir, "module_seeds.json"), "w"), indent=1)

    c = ["/* Generated by tools/build_modules.py -- do not edit.",
         " *",
         " * Fixed guest bases for Alter Echo's runtime-loaded modules, and the",
         " * relocation sites (offsets from the loaded image start, i.e. from",
         " * base + rawptr0) that src/module_loader.c re-applies when it moves a",
         " * freshly loaded module to its base. */",
         "#include \"module_layout.h\"", ""]
    for i, (name, B, rawptr0, packed, offs, *_rest) in enumerate(c_mods):
        c.append(f"static const uint32_t s_sites_{i}[] = {{")
        for j in range(0, len(offs), 8):
            c.append("    " + ", ".join(f"0x{o:05X}" for o in offs[j:j + 8]) + ",")
        c.append("};")
    c.append("")
    c.append("const AeModule g_ae_modules[] = {")
    for i, (name, B, rawptr0, packed, offs, *_rest) in enumerate(c_mods):
        c.append(f"    {{ \"{name}\", 0x{B:08X}u, 0x{rawptr0:X}u, 0x{packed:X}u, "
                 f"{len(offs)}u, s_sites_{i} }},")
    c.append("};")
    c.append(f"const unsigned g_ae_module_count = {len(c_mods)}u;")
    c.append(f"const uint32_t g_ae_module_region = 0x{MODULE_REGION:08X}u;")
    c.append(f"const uint32_t g_ae_module_span = 0x{span:08X}u;")
    c.append("")
    with open(os.path.join(root, "src", "module_layout.c"), "w") as f:
        f.write("\n".join(c))

    t = ["/* Generated by tools/build_modules.py -- do not edit.",
         " *",
         " * Import thunk slots in the modules' .text, patched at run time by",
         " * each module's DLLFixupNames with `jmp <engine function>`. Each is",
         " * hand-written (tools.recomp --manual-functions build-modules/",
         " * thunk_functions.json) so a call into one follows the jmp that is",
         " * actually there. See ae_dyn_thunk in module_loader.c. */",
         "#include <stdint.h>",
         "void ae_dyn_thunk(uint32_t slot);", ""]
    for a in thunks:
        t.append(f"void sub_{a:08X}(void) {{ ae_dyn_thunk(0x{a:08X}u); }}")
    t.append("")
    with open(os.path.join(root, "src", "module_thunks.c"), "w") as f:
        f.write("\n".join(t))
    json.dump({f"0x{a:08X}": f"sub_{a:08X}" for a in thunks},
              open(os.path.join(root, "build-modules", "thunk_functions.json"), "w"),
              indent=1)

    print(f"{len(c_mods)} modules at 0x{MODULE_REGION:08X}-0x{cursor:08X} "
          f"({span / 1e6:.1f} MB), {total_sites} relocation sites, "
          f"{len(seeds)} seeds, {len(thunks)} import thunk slots -> {xbe_out}")


if __name__ == "__main__":
    main()
