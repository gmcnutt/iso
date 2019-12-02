/**
 * Functions for rendering to an isometric grid.
 */

#include "iso.h"
#include "log.h"

/* XXX: replace hard-coded values with something else, like an iso context. */
#define MAP_HEIGHT 10
#define MAP_WIDTH 10
#define TILE_WIDTH 64
#define TILE_HEIGHT 32
#define TILE_WIDTH_HALF (TILE_WIDTH / 2)
#define TILE_HEIGHT_HALF (TILE_HEIGHT / 2)


static inline int map_to_screen_x(int map_x, int map_y)
{
        return (map_x - map_y) * TILE_WIDTH_HALF;
}


static inline int map_to_screen_y(int map_x, int map_y)
{
        return (map_x + map_y) * TILE_HEIGHT_HALF;
}

static inline int screen_to_map_x(int screen_x, int screen_y)
{
        /* XXX: consider using integer-only.  XXX: these factors are common
           with screen_to_map_y, so they only need to be computed once. With
           optimizations enabled the compiler might be smart enough to figure
           that out since these are declared inline.
        */
        return (int)((float)screen_x / (float)TILE_WIDTH +
                     (float)screen_y / (float)TILE_HEIGHT);
}

static inline int screen_to_map_y(int screen_x, int screen_y)
{
        return (int)((float)screen_y / (float)TILE_HEIGHT -
                     (float)screen_x / (float)TILE_WIDTH);
}

void iso_blit(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *srcrect,
              int map_x, int map_y)
{
        SDL_Rect screen_dst;

        screen_dst.x = (map_to_screen_x(map_x, map_y) +
                        map_to_screen_x(MAP_HEIGHT - 1, 0));
        screen_dst.y = (map_to_screen_y(map_x, map_y) -
                        (srcrect->h - TILE_HEIGHT));

        screen_dst.w = srcrect->w;
        screen_dst.h = srcrect->h;
        SDL_RenderCopy(renderer, texture, srcrect, &screen_dst);
}


void iso_fill(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *srcrect,
              int map_w, int map_h)
{
        SDL_Rect screen_dst;
        int row, col, off_x;

        screen_dst.w = TILE_WIDTH;
        screen_dst.h = TILE_HEIGHT;

        off_x = map_to_screen_x(map_h - 1, 0);

        for (row = 0; row < map_h; row++) {
                for (col = 0; col < map_w; col++) {
                        screen_dst.x = map_to_screen_x(col, row) + off_x;
                        screen_dst.y = map_to_screen_y(col, row);
                        SDL_RenderCopy(renderer, texture, srcrect
                                       , &screen_dst);
                }
        }
}


void iso_grid(SDL_Renderer *renderer, int map_w, int map_h)
{
        int row, col, offx;

        offx = map_to_screen_x(map_h, 0);
        for (row = 0; row <= map_h; row++) {
                SDL_RenderDrawLine(
                        renderer,
                        offx + map_to_screen_x(0, row),
                        map_to_screen_y(0, row),
                        offx + map_to_screen_x(map_w, row),
                        map_to_screen_y(map_w, row));
        }
        for (col = 0; col <= map_w; col++) {
                SDL_RenderDrawLine(
                        renderer,
                        offx + map_to_screen_x(col, 0),
                        map_to_screen_y(col, 0),
                        offx + map_to_screen_x(col, map_h),
                        map_to_screen_y(col, map_h));
        }
}


int iso_screen_to_map(int screen_x, int screen_y, int *map_x, int *map_y)
{
        int off_x;

        off_x = map_to_screen_x(MAP_HEIGHT, 0);
        screen_x -= off_x;
        *map_x = screen_to_map_x(screen_x, screen_y);
        *map_y = screen_to_map_y(screen_x, screen_y);

        if (*map_x < 0 || *map_x > MAP_WIDTH) {
                log_debug("%s:map_x=%d off-map\n", __FUNCTION__, *map_x);
                return -1;
        }

        if (*map_y < 0 || *map_y > MAP_HEIGHT) {
                log_debug("%s:*map_y=%d off-map\n", __FUNCTION__, *map_y);
                return -1;
        }

        return 0;
}
