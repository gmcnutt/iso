/**
 * A 2d map representation.
 *
 * This just uses an SDL_Surface and its pixels to represent a map.
 *
 * Copyright (c) 2019 Gordon McNutt
 */

#include "map.h"
#include <SDL2/SDL_image.h>

map_t *map_from_image(const char *filename)
{
        SDL_Surface *surface;

        /* Load the image file that has the map */
        if (!(surface = IMG_Load(filename))) {
                printf("%s:IMG_Load:%s\n", __FUNCTION__, SDL_GetError());
                return NULL;
        }

        /* Make sure the pixels are in RGBA format, 8 bits per pixel */
        if (surface->format->format != SDL_PIXELFORMAT_RGBA8888) {
                SDL_Surface *tmp;
                tmp =
                    SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888,
                                             0);
                SDL_FreeSurface(surface);
                surface = tmp;
                if (!surface) {
                        printf("%s:SDL_ConvertSurfaceFormat:%s\n", __FUNCTION__,
                               SDL_GetError());
                        return NULL;
                }
        }

        return surface;
}
