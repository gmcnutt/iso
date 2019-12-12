/**
 * A 2d map representation.
 *
 * This just uses an SDL_Surface and its pixels to represent a map.
 *
 * Copyright (c) 2019 Gordon McNutt
 */

#ifndef __map_h__
#define __map_h__

#include <SDL2/SDL.h>
#include <stddef.h>

typedef uint32_t pixel_t;
typedef SDL_Surface map_t;

enum {
        PIXEL_VALUE_TREE = 0x004001ff,
        PIXEL_VALUE_SHRUB = 0x008000ff,
        PIXEL_VALUE_GRASS = 0x00ff00ff,
        PIXEL_VALUE_FLOOR = 0x0000f0ff,
        PIXEL_VALUE_WALL = 0xffffffff
};

enum {
        PIXEL_MASK_OPAQUE = 0x00000100, /* blue bit 0 */
        PIXEL_MASK_IMPASSABLE = 0x00000200      /* blue bit 1 */
};


#define map_opaque_at(m, x, y) (map_get_pixel((m), (x), (y)) & PIXEL_MASK_OPAQUE)
#define map_passable_at(m, x, y) (!(map_get_pixel((m), (x), (y)) & PIXEL_MASK_IMPASSABLE))
#define map_contains(m, x, y) \
        (between_inc((x), map_left(m), map_right(m)) && \
         between_inc((y), map_top(m), map_bottom(m)))
#define map_h(m) ((m)->h)
#define map_w(m) ((m)->w)
#define map_left(m) 0
#define map_right(m) ((m)->w - 1)
#define map_top(m) 0
#define map_bottom(m) ((m)->h - 1)
#define map_free(m) (SDL_FreeSurface(m))

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

/**
 * Create a map from an image file.
 *
 * Use `map_free` when done with it.
 */
map_t *map_from_image(const char *filename);

#endif
