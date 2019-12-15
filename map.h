/**
 * A 2d map representation.
 *
 * This just uses an SDL_Surface and its pixels to represent a map.
 *
 * Copyright (c) 2019 Gordon McNutt
 */

#ifndef map_header
#define map_header

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef uint32_t pixel_t;
typedef SDL_Surface map_t;

/* 
   Pixel bits

   Red       Green     Blue      Alpha     |Color channels
   7654 3210 7654 3210 7654 3210 7653 3210 |Bit numbers in channel
   .... .... .... .... .... .... XXXX XXXX |Alpha
   .... .... .... .... .... ...X .... .... |1=opaque
   .... .... .... .... .... ..X. .... .... |1=impassable
   .... .XXX .... .... .... ..X. .... .... |Height (0=ground, 5=full)
   XXXX .... XXXX .... XXXX .... .... .... |Terrain type
   .... X... .... XXXX .... XX.. .... .... |Reserved
 */

#define PIXEL(r,g,b,o,i,h)

enum {
        PIXEL_VALUE_TREE = 0x004001ff,
        PIXEL_VALUE_SHRUB = 0x008000ff,
        PIXEL_VALUE_GRASS = 0x00ff00ff,
        PIXEL_VALUE_FLOOR = 0x0000f0ff,
        PIXEL_VALUE_WALL = 0xffffffff,
        PIXEL_VALUE_1x1x1 = 0xff00fdff,
};

enum {
        PIXEL_MASK_OPAQUE = 0x00000100, /* blue bit 0 */
        PIXEL_MASK_IMPASSABLE = 0x00000200      /* blue bit 1 */
};

enum {
        MAP_FLOOR1,
        MAP_FLOOR2,
        MAP_FLOOR3,
        N_MAPS
};

typedef struct {
        map_t *maps[N_MAPS];
        int n_maps;
        int w, h;
} mapstack_t;

#define mapstack_w(ms) ((ms)->w)
#define mapstack_h(ms) ((ms)->h)

#define map_opaque_at(m, x, y) (map_get_pixel((m), (x), (y)) & PIXEL_MASK_OPAQUE)
#define map_contains(m, x, y) (((x) >= 0 && (x) < map_w(m)) && ((y) >= 0 && (y) < map_h(m)))
#define map_h(m) ((m)->h)
#define map_w(m) ((m)->w)
#define map_left(m) 0
#define map_right(m) ((m)->w - 1)
#define map_top(m) 0
#define map_bottom(m) ((m)->h - 1)
#define map_free(m) (SDL_FreeSurface(m))

/**
 * Get the map at index i, or NULL if none or out-of-bounds.
 */
map_t *mapstack_get(mapstack_t * ms, int i);

/**
 * Add a map to the stack.
 */
bool mapstack_add(mapstack_t * ms, map_t * map);

/**
 * Get the pixel at the given map location.
 */
static inline pixel_t map_get_pixel(map_t * map, size_t x, size_t y)
{
        /* The pitch is the length of a row of pixels in bytes. Find the first
         * byte of the desired pixel then cast it to an RGBA 32 bit value to
         * check it. */
        size_t offset = (y * map->pitch + x * sizeof (pixel_t));
        uint8_t *byteptr = (uint8_t *) map->pixels;
        byteptr += offset;
        uint32_t *pixelptr = (uint32_t *) byteptr;
        return *pixelptr;
}

static inline bool map_passable_at_xy(map_t * map, int x, int y)
{
        pixel_t pix = map_get_pixel(map, x, y);
        return pix && !(pix & PIXEL_MASK_IMPASSABLE);
}

/**
 * Create a map from an image file.
 *
 * Use `map_free` when done with it.
 */
map_t *map_from_image(const char *filename);

#endif
