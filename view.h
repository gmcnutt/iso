/**
 * A map viewer context.
 *
 * Copyright (c) 2019 Gordon McNutt
 */

#ifndef view_header
#define view_header

#include <stdbool.h>

#include "point.h"
#include "map.h"

#define VIEW_H 35
#define VIEW_W 35
#define VIEW_Z_MULT 5  /* Distance in tiles to offset maps vertically. */

/* The isometric view is rotated 45 degrees clockwise. This means the tile at
 * the lower left corner of the view (0, VIEW_H) should be at the left of the
 * screen. */
#define VIEW_OFFSET ((VIEW_H - 1) * TILE_WIDTH_HALF)

typedef struct {
        point_t cursor;
        rotation_t rotation;
        mapstack_t *maps; /* all maps */
        map_t *map; /* current map */
} view_t;

/**
 * Attempt to move the cursor.
 */
bool view_move_cursor(view_t *view, const point_t direction);


#endif
