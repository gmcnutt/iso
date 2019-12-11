# ISO Viewer

For experimenting with 2.5d isometric maps.

## Dependencies

Depends on `libcu`, a C utility lib available at
git@github.com:gmcnutt/cutils.git.

## Build

    make

## Usage

Example:

    ./demo -i moongate_clearing.png,moongate_clearing_l2.png

Key bindings:

<arrow> - move cursor
',','.' - rotate camera
't'     - toggle transparency

Use the arrow keys to move the red cursor square around. 't' toggles
transparency. Clicking a tile prints some info on stdout.

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

Opaque terrain blocks field-of-view (FOV). Impassable terrain blocks
cursor movement.

