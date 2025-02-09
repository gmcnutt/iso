# ISO Viewer

For experimenting with 2.5d isometric maps.

## Dependencies

Depends on `libcu`, a C utility lib available at
git@github.com:gmcnutt/cutils.git.

## Build

    make

## Usage

Example:

    ./demo -i mc0.png,mc1.png,mc2.png,mc3.png

That stacks 4 maps on top of each other. Maps must be the same size
for this to work.

Key bindings:

    <arrow> ..........move cursor
    ., ...............rotate camera
    t  ...............toggle transparency
    <page up/down> ...jump between maps (when passable)

Clicking a tile prints some info on stdout.

## Maps

Maps are just image files. The color of the pixel determines the terrain type:

        PIXEL_VALUE_TREE = 0x004001ff,
        PIXEL_VALUE_SHRUB = 0x008000ff,
        PIXEL_VALUE_GRASS = 0x00ff00ff,
        PIXEL_VALUE_FLOOR = 0x0000f0ff,
        PIXEL_VALUE_WALL = 0xffffffff

A transparent pixel means "nothing there". Unrecognized colors print
an error on stdout but otherwise also mean "nothing there".

Some of the low bits of the blue color control opacity and
passability:

        PIXEL_MASK_OPAQUE = 0x00000100, /* blue bit 0 */
        PIXEL_MASK_IMPASSABLE = 0x00000200      /* blue bit 1 */

Opaque terrain blocks field-of-view (FOV). Impassable terrain and no terrain
blocks cursor movement.
