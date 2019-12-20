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

/* The isometric view is rotated 45 degrees clockwise. This means the tile at
 * the lower left corner of the view (0, VIEW_H) should be at the left of the
 * screen. */
#define VIEW_OFFSET ((VIEW_H - 1) * TILE_WIDTH_HALF)

typedef struct {
        point_t cursor;
        rotation_t rotation;
        fov_map_t fovs[N_MAPS]; /* one per map */
        int n_fovs;
        int fov_w;
        int fov_h;
} view_t;

/**
 * Initialize the already-allocated view.
 */
int view_init(view_t * view, area_t * maps, bool use_fov);
void view_deinit(view_t * view);

/**
 * Recalculate the fov map based on the cursor.
 */
void view_calc_fov(view_t * view);

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

bool view_in_fov(view_t *view, point_t maploc);

#endif
