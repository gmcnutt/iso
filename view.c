/**
 * A map viewer context.
 *
 * Copyright (c) 2019 Gordon McNutt
 */

#include "view.h"

void view_init(view_t *view, mapstack_t *maps, bool use_fov)
{
        memset(view, 0, sizeof(*view));
        
        view->maps = maps;
        view->map = maps->maps[MAP_FLOOR1];

        fov_init(&view->fov, map_w(view->map), map_h(view->map));

        if (use_fov) {
                map_t *map = view->map;
                fov_map_t *fov = &view->fov;
                for (size_t y = 0, index = 0; y < map_h(map); y++) {
                        for (size_t x = 0; x < map_w(map); x++, index++) {
                                if (map_opaque_at(map, x, y)) {
                                        fov->opq[index] = 1;
                                }
                        }
                }
        }
}

void view_deinit(view_t *view)
{
        fov_deinit(&view->fov);
        memset(view, 0, sizeof(*view));
        
}

bool view_move_cursor(view_t * view, const point_t dir)
{
        point_t newcur, rdir;
        map_t *map;

        point_copy(newcur, view->cursor);
        point_copy(rdir, dir);
        rdir[Z] *= VIEW_Z_MULT;
        point_rotate(rdir, view->rotation);
        point_translate(newcur, rdir);
        if ((map = mapstack_get(view->maps, newcur[Z] / VIEW_Z_MULT)) &&
            map_contains(map, newcur[X], newcur[Y]) &&
            map_passable_at_xy(map, newcur[X], newcur[Y])) {
                point_copy(view->cursor, newcur);
                return true;
        }
        return false;
}
