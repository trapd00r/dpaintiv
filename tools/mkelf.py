#!/usr/bin/env python3
"""Relocate the unpacked DPaint hunk file into a big-endian m68k ELF.

Reads the user's disk copy. The ELF is a load image for Ghidra, not a
file that belongs in git. Output defaults to ghidra/DPaintIV.elf.
"""

import struct
import sys

HUNK_HEADER = 0x3F3
HUNK_CODE = 0x3E9
HUNK_DATA = 0x3EA
HUNK_RELOC32 = 0x3EC
HUNK_END = 0x3F2
SIZE_MASK = 0x3FFFFFFF
BASES = (0x400000, 0x44EE98, 0x44FE00)


def u32(buf, off):
    return struct.unpack_from(">I", buf, off)[0]


def parse(data):
    if u32(data, 0) != HUNK_HEADER:
        raise SystemExit("not an Amiga hunk file")
    off = 4
    nlibs = u32(data, off)
    off += 4
    for _ in range(nlibs):
        n = u32(data, off)
        off += 4 + n * 4
    off += 4
    first = u32(data, off)
    off += 4
    last = u32(data, off)
    off += 4
    sizes = []
    for _ in range(last - first + 1):
        sizes.append(u32(data, off) & SIZE_MASK)
        off += 4
    hunks = []
    relocs = []
    for i, mem_longs in enumerate(sizes):
        typ = u32(data, off) & SIZE_MASK
        off += 4
        nlong = u32(data, off) & SIZE_MASK
        off += 4
        if typ not in (HUNK_CODE, HUNK_DATA):
            raise SystemExit("hunk %d type %#x" % (i, typ))
        raw = data[off:off + nlong * 4]
        off += nlong * 4
        mem = bytearray(mem_longs * 4)
        mem[:len(raw)] = raw
        hrs = []
        while True:
            t = u32(data, off)
            off += 4
            if t == HUNK_END:
                break
            if (t & SIZE_MASK) != HUNK_RELOC32:
                raise SystemExit("unexpected hunk %#x" % t)
            while True:
                count = u32(data, off)
                off += 4
                if count == 0:
                    break
                hn = u32(data, off)
                off += 4
                hrs.append((hn, [u32(data, off + k * 4) for k in range(count)]))
                off += count * 4
        hunks.append(mem)
        relocs.append(hrs)
    return hunks, relocs


def relocate(hunks, relocs):
    counts = [0, 0, 0]
    for hi, hrs in enumerate(relocs):
        for hn, offs in hrs:
            base = BASES[hn]
            counts[hi] += len(offs)
            for o in offs:
                val = u32(hunks[hi], o)
                struct.pack_into(">I", hunks[hi], o, (val + base) & 0xFFFFFFFF)
    return counts


def write_elf(path, hunks):
    phoff = 52
    phnum = 3
    off = phoff + phnum * 32
    segs = []
    for h in hunks:
        segs.append(off)
        off += len(h)
    shstr = b"\x00.text\x00.chip\x00.data\x00.shstrtab\x00"
    shstr_off = off
    shoff = shstr_off + len(shstr)
    if shoff % 4:
        shoff += 4 - (shoff % 4)
    names = (0, 1, 7, 13, 19)
    shnum = 5

    out = bytearray()
    eh = bytearray(52)
    eh[0:4] = b"\x7fELF"
    eh[4] = 1
    eh[5] = 2
    eh[6] = 1
    struct.pack_into(">HHI", eh, 16, 2, 4, 1)
    struct.pack_into(">III", eh, 24, BASES[0], phoff, shoff)
    struct.pack_into(">IHHHHHH", eh, 36, 0, 52, 32, phnum, 40, shnum, 4)
    out += eh
    pad = shoff - (shstr_off + len(shstr))
    flags = (5, 6, 6)
    for i, h in enumerate(hunks):
        ph = bytearray(32)
        struct.pack_into(">IIIIIII", ph, 0, 1, segs[i], BASES[i], BASES[i],
                         len(h), len(h), flags[i])
        struct.pack_into(">I", ph, 28, 4)
        out += ph
    for h in hunks:
        out += h
    out += shstr
    out += b"\x00" * pad

    def shdr(name, typ, flags, addr, foff, size):
        sh = bytearray(40)
        struct.pack_into(">IIIIIIIIII", sh, 0, name, typ, flags, addr, foff,
                         size, 0, 0, 4, 0)
        return sh

    # null, text, chip data, fast data, shstrtab
    # hunk order is code, chip data, fast data
    sec_flags = (0, 2 | 4, 2 | 1, 2 | 1, 0)
    sec_type = (0, 1, 1, 1, 3)
    sec_addr = (0, BASES[0], BASES[1], BASES[2], 0)
    sec_off = (0, segs[0], segs[1], segs[2], shstr_off)
    sec_size = (0, len(hunks[0]), len(hunks[1]), len(hunks[2]), len(shstr))
    for i in range(5):
        out += shdr(names[i], sec_type[i], sec_flags[i], sec_addr[i],
                    sec_off[i], sec_size[i])
    with open(path, "wb") as f:
        f.write(out)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "amiga_original/unpacked/DPaint"
    dst = sys.argv[2] if len(sys.argv) > 2 else "ghidra/DPaintIV.elf"
    data = open(src, "rb").read()
    hunks, relocs = parse(data)
    counts = relocate(hunks, relocs)
    write_elf(dst, hunks)
    print("hunks", [len(h) for h in hunks], "relocs", counts, "elf", dst)
    blob = b"".join(hunks)
    for s in (b"Load..", b"Picture", b"Matte", b"FILETYPE=ILBM", b"About Deluxe Paint IV"):
        at = blob.find(s)
        if at < 0:
            print("missing", s)
            continue
        base = BASES[0]
        if at >= len(hunks[0]):
            at -= len(hunks[0])
            base = BASES[1]
            if at >= len(hunks[1]):
                at -= len(hunks[1])
                base = BASES[2]
        print("%s at %#x" % (s.decode(), base + at))


if __name__ == "__main__":
    main()
