/**
 * Functions for rendering to an isometric grid.
 */

#include "iso.h"
#include "log.h"

/* XXX: replace hard-coded values with something else, like an iso context. */
#define VIEW_HEIGHT 10
#define VIEW_WIDTH 10
#define TILE_WIDTH 36
#define TILE_HEIGHT 18
#define TILE_WIDTH_HALF (TILE_WIDTH / 2)
#define TILE_HEIGHT_HALF (TILE_HEIGHT / 2)


static inline int view_to_screen_x(int view_x, int view_y)
{
        return (view_x - view_y) * TILE_WIDTH_HALF;
}


static inline int view_to_screen_y(int view_x, int view_y)
{
        return (view_x + view_y) * TILE_HEIGHT_HALF;
}

static inline int screen_to_view_x(int screen_x, int screen_y)
{
        /* XXX: consider using integer-only.  XXX: these factors are common
         * with screen_to_view_y, so they only need to be computed once. With
         * optimizations enabled the compiler might be smart enough to figure
         * that out since these are declared inline.
         */
        return (int)((float)screen_x / (float)TILE_WIDTH +
                     (float)screen_y / (float)TILE_HEIGHT);
}

static inline int screen_to_view_y(int screen_x, int screen_y)
{
        return (int)((float)screen_y / (float)TILE_HEIGHT -
                     (float)screen_x / (float)TILE_WIDTH);
}

void iso_blit(SDL_Renderer * renderer, SDL_Texture * texture,
              SDL_Rect * srcrect, int view_x, int view_y)
{
        SDL_Rect screen_dst;

        screen_dst.x = (view_to_screen_x(view_x, view_y) +
                        view_to_screen_x(VIEW_HEIGHT - 1, 0));
        screen_dst.y = (view_to_screen_y(view_x, view_y) -
                        (srcrect->h - TILE_HEIGHT));

        screen_dst.w = srcrect->w;
        screen_dst.h = srcrect->h;
        SDL_RenderCopy(renderer, texture, srcrect, &screen_dst);
}


void iso_fill(SDL_Renderer * renderer, SDL_Texture * texture,
              SDL_Rect * srcrect, int view_w, int view_h)
{
        SDL_Rect screen_dst;
        int row, col, off_x;

        screen_dst.w = TILE_WIDTH;
        screen_dst.h = TILE_HEIGHT;

        off_x = view_to_screen_x(view_h - 1, 0);

        for (row = 0; row < view_h; row++) {
                for (col = 0; col < view_w; col++) {
                        screen_dst.x = view_to_screen_x(col, row) + off_x;
                        screen_dst.y = view_to_screen_y(col, row);
                        SDL_RenderCopy(renderer, texture, srcrect, &screen_dst);
                }
        }
}


void iso_grid(SDL_Renderer * renderer, int view_w, int view_h)
{
        int row, col, offx;

        offx = view_to_screen_x(view_h, 0);
        for (row = 0; row <= view_h; row++) {
                SDL_RenderDrawLine(renderer,
                                   offx + view_to_screen_x(0, row),
                                   view_to_screen_y(0, row),
                                   offx + view_to_screen_x(view_w, row),
                                   view_to_screen_y(view_w, row));
        }
        for (col = 0; col <= view_w; col++) {
                SDL_RenderDrawLine(renderer,
                                   offx + view_to_screen_x(col, 0),
                                   view_to_screen_y(col, 0),
                                   offx + view_to_screen_x(col, view_h),
                                   view_to_screen_y(col, view_h));
        }
}

void iso_square(SDL_Renderer * renderer, int view_h, int view_x, int view_y)
{
        int off_x = view_to_screen_x(view_h, 0);
        SDL_Point points[5] = {
                {off_x + view_to_screen_x(view_x, view_y),
                 view_to_screen_y(view_x, view_y)},
                {off_x + view_to_screen_x(view_x + 1, view_y),
                 view_to_screen_y(view_x + 1, view_y)},
                {off_x + view_to_screen_x(view_x + 1, view_y + 1),
                 view_to_screen_y(view_x + 1, view_y + 1)},
                {off_x + view_to_screen_x(view_x, view_y + 1),
                 view_to_screen_y(view_x, view_y + 1)},
                {off_x + view_to_screen_x(view_x, view_y),
                 view_to_screen_y(view_x, view_y)},
        };
        SDL_RenderDrawLines(renderer, points, 5);
}

int iso_screen_to_map(int screen_x, int screen_y, int *view_x, int *view_y)
{
        int off_x;

        off_x = view_to_screen_x(VIEW_HEIGHT, 0);
        screen_x -= off_x;
        *view_x = screen_to_view_x(screen_x, screen_y);
        *view_y = screen_to_view_y(screen_x, screen_y);

        if (*view_x < 0 || *view_x > VIEW_WIDTH) {
                log_debug("%s:view_x=%d off-camera\n", __FUNCTION__, *view_x);
                return -1;
        }

        if (*view_y < 0 || *view_y > VIEW_HEIGHT) {
                log_debug("%s:*view_y=%d off-camera\n", __FUNCTION__, *view_y);
                return -1;
        }

        return 0;
}
