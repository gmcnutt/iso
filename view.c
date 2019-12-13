/**
 * A map viewer context.
 *
 * Copyright (c) 2019 Gordon McNutt
 */

#include "view.h"

bool view_move_cursor(view_t *view, const point_t dir)
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
