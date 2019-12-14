/**
 * A map viewer context.
 *
 * Copyright (c) 2019 Gordon McNutt
 */

#include "view.h"

#define Z2L(z) ((z) / VIEW_Z_MULT)
#define L2Z(l) ((l) * VIEW_Z_MULT);

int view_init(view_t *view, mapstack_t *maps, bool use_fov)
{
        int res = 0;
        memset(view, 0, sizeof(*view));
        
        view->maps = maps;
        view->map = maps->maps[MAP_FLOOR1];

        for (int i = 0; i < view->maps->n_maps; i++) {
                fov_map_t *fov = &view->fovs[i];
                map_t *map = maps->maps[i];

                if ((res = fov_init(fov, map_w(view->map), map_h(view->map)))) {
                        view_deinit(view);
                        return res;
                }

                if (use_fov) {
                        for (size_t y = 0, index = 0; y < map_h(map); y++) {
                                for (size_t x = 0; x < map_w(map); x++, index++) {
                                        if (map_opaque_at(map, x, y)) {
                                                fov->opq[index] = 1;
                                        }
                                }
                        }
                }
        }

        return 0;
}

void view_calc_fov(view_t *view)
{
        for (int i = 0; i < view->maps->n_maps; i++) {
                fov(&view->fovs[i], view->cursor[X], view->cursor[Y], VIEW_W);
        }
}

void view_deinit(view_t *view)
{
        if (view->maps) {
                for (int i = 0; i < view->maps->n_maps; i++) {
                        fov_deinit(&view->fovs[i]);
                }
        }
        memset(view, 0, sizeof(*view));        
}

bool view_in_fov(view_t *view, point_t mloc)
{
        int lvl = Z2L(mloc[Z]);
        return view->fovs[lvl].vis[mloc[X] + mloc[Y] * view->fovs[lvl].w];
}

bool view_move_cursor(view_t * view, const point_t dir)
{
        point_t newcur, rdir;
        map_t *map;

        point_copy(newcur, view->cursor);
        point_copy(rdir, dir);
        rdir[Z] = L2Z(dir[Z]);  /* dir z is # levels */
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
