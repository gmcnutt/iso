/**
 * A map viewer context.
 *
 * Copyright (c) 2019 Gordon McNutt
 */

#include "view.h"

int view_init(view_t * view, area_t * maps, bool use_fov)
{
        int res = 0;
        memset(view, 0, sizeof (*view));

        view->n_fovs = maps->n_maps;

        for (int i = 0; i < view->n_fovs; i++) {
                fov_map_t *fov = &view->fovs[i];
                map_t *map = maps->maps[i];
                view->fov_w = map_w(map);
                view->fov_h = map_h(map);

                if ((res = fov_init(fov, map_w(map), map_h(map)))) {
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

void view_calc_fov(view_t * view)
{
        for (int i = 0; i < view->n_fovs; i++) {
                fov(&view->fovs[i], view->cursor[X], view->cursor[Y], VIEW_W);
        }
}

void view_deinit(view_t * view)
{
        for (int i = 0; i < view->n_fovs; i++) {
                fov_deinit(&view->fovs[i]);
        }
        memset(view, 0, sizeof (*view));
}

bool view_in_fov(view_t *view, point_t maploc)
{
        int index = maploc[Y] * view->fov_w + maploc[X];
        int lvl = Z2L(maploc[Z]);
        return view->fovs[lvl].vis[index];
}
