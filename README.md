# Deluxe Paint IV for Linux

A native C and SDL2 recreation of Deluxe Paint IV. The menu names match
the original. The UI font, the other built-in faces, the toolbox
bitplanes, and the default palette are compiled into the source, so the
program paints text with no Amiga disk present. The page is whatever
space is left after the menu bar, tool column, and palette, so a large
monitor gets a larger page than 320x200. Screen Format still offers the
four original sizes.

Original Deluxe Paint IV is copyright 1985-1991 Electronic Arts. This
tree does not ship the ADF, the program, kickstarts, or the unpacked
disk. Those stay in `amiga_original/` and `kickstart/` (gitignored) for
FS-UAE only.

## Build and run

Needs gcc and SDL2 (`pkg-config sdl2`).

```
make
./dpaint
make smoke
```

`make run` is the same launch. `make smoke` does not open a window. It
draws strokes, a line, a filled rectangle, and a fill; adds a frame and
changes it; writes an ILBM (PackBits and uncompressed) and a brush with
GRAB; writes a compressed ANIM and an expanded ANIM; writes a PRSP and
a 1-plane stencil; reads each back and compares pixels or bytes; and
writes `ui.ppm` of the chrome. The temp directory is printed at the end.

The window title is Deluxe Paint IV. A directory argument still loads
DiskFont files and a DPaint hunk from that path. With no argument the
built-in faces, icons, and palette are used. Picking a font file still
goes through the DiskFont loader.

## What this build does

- Menu bar, right tool column, bottom palette and status, integer scale
- Tools: dotted freehand, continuous draw, line, curve, fill, rectangle,
  filled rectangle, circle, ellipse, airbrush, text, brush pickup and
  stamp, undo, clear
- 32-color palette, foreground and background
- Modes: Matte, Color, Replc, Smear, plus simpler Shade, Blend, Cycle,
  Smooth, Mix, and HBrite
- Spare page: swap, copy, merge in front, merge in back
- Frames: add, copy, delete, next, previous, go to, play, play once,
  ping-pong, frame rate. AnimBrush pickup and stamp across frames
- Screen formats Lo-Res 320x200, Med-Res 640x200, Interlace 320x400,
  Hi-Res 640x400, and a larger default page
- About credits: Daniel Silva, Lee Taran, and Steve Shaw

## File formats that round-trip here

Tested by `make smoke` on files this program writes:

- ILBM pictures: FORM/ILBM, BMHD, CMAP, CAMG, CRNG, BODY. Compression 0
  and PackBits (1). HAM is rejected on load.
- ILBM brushes: the same, plus GRAB
- ANIM: first frame is an ILBM. Compressed frames are ANHD operation 5
  and DLTA (byte columns). Expanded frames are a full BODY each. DPAN
  carries the frame count and rate.
- PRSP: FORM/PRSP with a 110-byte DPPS chunk. The bytes round-trip. The
  field layout inside DPPS was not recovered, so this is not a working
  perspective file.
- Stencil: a 1-plane ILBM. No stencil chunk id was found in the program,
  so this is the container used here, not a verified original stencil.

## Not in this build

Perspective, print, light table, anti-alias, translucency, metamorph,
shear, any-angle rotate, bend, trim, the color mixer, range editing,
move templates, and HAM. Choosing those menu items reports that they
are not in this build. This is not a finished Deluxe Paint IV.

Notes on the Amiga binary are in `notes/RE.md`.
