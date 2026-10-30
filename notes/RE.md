# Deluxe Paint IV, Amiga binary

The disk and the program stay in `amiga_original/` and are not part of
this tree. Facts below were checked against
`amiga_original/unpacked/DPaint` (368060 bytes) and the ADF
`amiga_original/deluxe-paint4.adf` (901120 bytes, SHA-256
`78e383f5697901f3f9ee815273586d14cfb1ca5bc98316e4f5c296b1084ad9cc`).

Pseudocode is not original source. Drawing modes in `src/draw.c` are
reconstructions from the menu names. They were not copied from a
decompilation.

## Hunks

HUNK_HEADER `0x3F3`, no resident libraries. Three hunks. `tools/mkelf.py`
relocates them and writes `ghidra/DPaintIV.elf` (gitignored).

| Hunk | Kind | File bytes | Memory bytes | Relocs | Base |
| --- | --- | --- | --- | --- | --- |
| 0 | CODE | 323224 | 323224 | 1726 | `0x400000` |
| 1 | DATA, chip | 3944 | 3944 | 0 | `0x44EE98` |
| 2 | DATA | 29500 initialized | 38376 | 1089 | `0x44FE00` |

Hunk 1's type long is `0x400003EA` (DATA with the chip flag). The loader
masks with `0x3FFFFFFF`. Relocs are RELOC32: the stored long is an offset
in the target hunk, and the target base is added. After that pass the
string addresses are `Load..` at `0x450D87`, `Picture` at `0x45031E`,
`Matte` at `0x450F4A`, `FILETYPE=ILBM` at `0x4563DE`.

Libraries named in the binary: dos, graphics, intuition, icon, layers,
diskfont, and the devices timer, console, printer, input.

## Menus

The menus are not an Intuition `NewMenu` table. They are a linked list
of `Menu` (name pointer at +14, first item at +18), `MenuItem` (item
text at +18, command byte at +26), and `IntuiText` (string at +12).
Item flags `0x52` are text items, `0x56` adds a command key, `0x5B` is
a prefs toggle, and `0x153` is the checked default on Compressed.

Order recovered from that list:

- Picture: Load.. (L), Save.. (S), Delete.. (D), Print.. (P), Flip,
  Spare (Swap, Copy To Spare, Merge in front, Merge in back, Delete
  this Page), Page Size.., Show Page, Screen Format.., About.., Quit
- Brush: Load, Save, Restore, Spare, Size (Stretch, Halve, Double,
  Double Horiz, Double Vert), Flip, Edge (Outline, Trim), Rotate
  (90 Degrees, Any Angle, Shear), Bend, Handle (Center, Corner, Place)
- Mode: Matte, Color, Replc, Smear, Shade, Blend, Cycle, Smooth, Mix,
  HBrite
- Anim: AnimBrush, Load.., Save.., Move.., Frames, Control, Method
  (Compressed checked, Expanded)
- Color: Ranges.., Cycle, Palette, Color Set, BG -> FG, BG <-> FG,
  Remap, Recompute, Brush
- Effect: Stencil, LightTable, Background, AntiAlias, Process,
  Translucency, Perspective
- Prefs: Coords, Fast FB, MultiCycle, Be Square, Workbench, ExclBrush,
  AutoTransp, No Icons, AutoGrid, OriginUL, FastAdjust

The Linux menus use these names. Several leaves only set a status line
because the matching operation was not recovered.

## Icons, palette, font

Toolbox images are 16 by 11, one bitplane, stride 2. A table in hunk 2
points at them in the chip hunk. The unrelocated signature at the table
is the longs `0xD4`, `0xEA`, `0x100`. Twenty-nine pointers. The tool
column uses a subset that matches the usual DPaint tools (dotted,
continuous, line, curve, fill, boxes, circles, ellipses, airbrush,
text, brush). Undo and clear are the letters U and C, not original icons.

The default 32-color palette is 32 Amiga `0RGB` words in hunk 2, file
offset `0x51D2A` (hunk offset `0x3D2`). Color 0 is black. Colors 20
through 31 are the gray ramp `0333` .. `0FFF`. Color 31 is white. The
program stores each nibble times 17.

`fonts/DPaint/8` is a DiskFont (id `0x0F80`), y size 8, designed and
proportional, range 32..170. `fonts/DPaint/5` is y size 5. Topaz is
named in the program and is not on this disk; the UI uses DPaint.

## IFF sizes from immediate operands

Writers push a size with `PEA` of a small constant and then
`MOVE.L #id,-(A7)`. Sizes seen that way:

- BMHD 20, GRAB 4, CRNG 8, CAMG 4, ANHD 40, DPAN 8, MOVE 24, DPPS 110

FORM types written: ILBM, ANIM, PRSP. Also chunk ids BODY, CMAP, DLTA,
DRNG. DPAN's first word is stored as 3 (`MOVE.W #3` in that writer).
The 110 DPPS bytes are written, but the fields inside them were not
mapped. DRNG is not implemented. There is no STEN chunk; the stencil
file in this program is a 1-plane ILBM.

Screen mode bits used for CAMG: hi-res `0x8000`, lace `0x4`. HAM is
`0x800` and the loader refuses it.

## Ghidra

REA `open_binary` with `provider_id` `ghidra` rejected
`ghidra/DPaintIV.elf` as an unsupported ELF architecture. REA raw-binary
is only 6502, x86, x86_64, arm, and arm64, so that path was not used for
m68k. The DX-Ball GhidraMCP server was left alone.

Ghidra 12.1.4 headless (Java 25) imported the ELF with loader ELF and
language `68000:BE:32:default` into project `ghidra/DPaintIV`. Image base
`0x400000`. Blocks match the load map: `.text` 323224 bytes at
`0x400000`, `.chip` 3944 bytes at `0x44EE98`, `.data` 38376 bytes at
`0x44FE00`. Auto-analysis reported 3921 functions. That count is
Ghidra's split of the code, not a reviewed list of original routines.

Bytes at `0x450F4A` read back as `Matte`. The 32 palette words at
`0x4501D2` are `0000 0aaa 0e00 0a00 0d80 0fe0 08f0 0080 00b6 00dd 00af
007c 000f 070f 0c0e 0c08 0620 0e52 0a52 0fca` then the gray ramp through
`0fff`. The first three icon pointers at `0x453F9C` are `0x44EF6C`,
`0x44EF82`, `0x44EF98` (22 bytes apart, inside the chip hunk, which is
`0x44EE98 + 0xD4`). There were no code references to the `Matte` or
`Picture` string addresses. The decompiler output for the entry is an
unsettled startup that reads absolute address 4 (Amiga Exec base). It
is not a drawing routine, and it was not used as source.

So Ghidra confirmed the load map, the menu string, the palette, and the
relocated icon pointers. It did not identify the pixel operations behind
Matte, Color, Replc, or Smear. Those modes in this program are named
reconstructions.
