/**
 * A map viewer context.
 *
 * Copyright (c) 2019 Gordon McNutt
 */

#ifndef view_header
#define view_header

#include <stdbool.h>

#include "fov.h"
#include "point.h"
#include "map.h"

#define VIEW_H 35
#define VIEW_W 35
#define VIEW_Z_MULT 5           /* Distance in tiles to offset maps vertically. */

/* The isometric view is rotated 45 degrees clockwise. This means the tile at
 * the lower left corner of the view (0, VIEW_H) should be at the left of the
 * screen. */
#define VIEW_OFFSET ((VIEW_H - 1) * TILE_WIDTH_HALF)

typedef struct {
        point_t cursor;
        rotation_t rotation;
        mapstack_t *maps;       /* all maps */
        map_t *map;             /* current map */
        fov_map_t fovs[N_MAPS]; /* one per map */
} view_t;

/**
 * Initialize the already-allocated view.
 */
int view_init(view_t *view, mapstack_t *maps, bool use_fov);
void view_deinit(view_t *view);

/**
 * Recalculate the fov map based on the cursor.
 */
void view_calc_fov(view_t *view);

/**
 * Check if the map coordinates are visible from the cursor location.
 */
bool view_in_fov(view_t *view, point_t mloc);

/**
 * Attempt to move the cursor.
 *
 * The z-coord is assumed to mean number of map levels (not tiles).
 */
bool view_move_cursor(view_t * view, const point_t direction);

static inline void view_to_camera(point_t view, point_t cam)
{
        cam[X] = view[X] - VIEW_W / 2;
        cam[Y] = view[Y] - VIEW_H / 2;
}

/**
 * Convert a view location to a map location.
 *
 * Given a tile in view coordinates, convert it to coordinates of the map the
 * cursor is on. This only applies to (x, y), z is ignored.
 */
static inline void view_to_map(view_t * view, point_t vloc, point_t mloc)
{
        view_to_camera(vloc, mloc);
        point_rotate(mloc, view->rotation);
        mloc[X] += view->cursor[X];
        mloc[Y] += view->cursor[Y];
}


#endif
