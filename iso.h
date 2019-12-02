/**
 * Functions for rendering to an isometric grid.
 */
#ifndef iso_h
#define iso_h

#include <SDL2/SDL.h>

/**
 * Blit a texture to an iso grid.
 */
void iso_blit(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *srcrect,
              int map_x, int map_y);

/**
 * Fill an iso grid with a texture.
 */
void iso_fill(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *srcrect,
              int map_w, int map_h);

/**
 * Render isometric grid lines.
 */
void iso_grid(SDL_Renderer *renderer, int map_w, int map_h);

/**
 * Convert screen coordinates back to map coordinates. Returns -1 if
 * coordinates are off-map.
 */
int iso_screen_to_map(int screen_x, int screen_y, int *map_x, int *map_y);

#endif
