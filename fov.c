/*
 * Compute FOV map using recursive shadow casting:
 *
 * http://roguebasin.roguelikedevelopment.org/index.php?title=FOV_using_recursive_shadowcasting
 *
 * This is derived from libtcod's version, which is under the license shown
 * below. libtcod may currently be found at:
 *
 * https://github.com/libtcod/libtcod
 */
/*

BSD 3-Clause License

Copyright © 2008-2019, Jice and the libtcod contributors.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "fov.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int mult[4][8] = {
        {1, 0, 0, -1, -1, 0, 0, 1},
        {0, 1, -1, 0, 0, -1, 1, 0},
        {0, 1, 1, 0, 0, -1, -1, 0},
        {1, 0, 0, 1, -1, 0, 0, -1},
};

static void fov_octant(fov_map_t * map, int cx, int cy, int row,
                       float start, float end, int radius, int r2, int xx,
                       int xy, int yx, int yy, int id)
{
        int j;
        float new_start = 0.0f;
        if (start < end) {
                return;
        }
        for (j = row; j < radius + 1; j++) {
                int dx = -j - 1;
                int dy = -j;
                int blocked = 0;
                while (dx <= 0) {
                        int X, Y;
                        dx++;
                        X = cx + dx * xx + dy * xy;
                        Y = cy + dx * yx + dy * yy;
                        if ((unsigned)X < (unsigned)map->w &&
                            (unsigned)Y < (unsigned)map->h) {
                                float l_slope, r_slope;
                                int offset;
                                offset = X + Y * map->w;
                                l_slope = (dx - 0.5f) / (dy + 0.5f);
                                r_slope = (dx + 0.5f) / (dy - 0.5f);
                                if (start < r_slope)
                                        continue;
                                else if (end > l_slope)
                                        break;
                                if (dx * dx + dy * dy <= r2) {
                                        map->vis[offset] = 1;
                                }
                                if (blocked) {
                                        if (map->opq[offset]) {
                                                new_start = r_slope;
                                                continue;
                                        } else {
                                                blocked = 0;
                                                start = new_start;
                                        }
                                } else {
                                        if (map->opq[offset] && j < radius) {
                                                blocked = 1;
                                                fov_octant(map, cx, cy, j + 1,
                                                           start, l_slope,
                                                           radius, r2, xx, xy,
                                                           yx, yy, id + 1);
                                                new_start = r_slope;
                                        }
                                }
                        }
                }
                if (blocked)
                        break;
        }
}

int fov_init(fov_map_t * fov, int w, int h)
{
        fov->w = w;
        fov->h = h;
        if (! (fov->opq = calloc(1, (w * h)))) {
                return ERROR_ALLOC;
        }
        if (! (fov->vis = calloc(1, (w * h)))) {
                fov_deinit(fov);
                return ERROR_ALLOC;
        }
        return 0;
}

void fov_deinit(fov_map_t * fov)
{
        if (fov->opq) {
                free(fov->opq);
                fov->opq = NULL;
        }
        if (fov->vis) {
                free(fov->vis);
                fov->vis = NULL;
        }
        fov->w = fov->h = 0;
}

void fov(fov_map_t * map, int origin_x, int origin_y, int max_radius)
{
        int oct, r2;

        /* clean the map */
        memset(map->vis, 0, map->w * map->h);

        if (max_radius == 0) {
                int max_radius_x = map->w - origin_x;
                int max_radius_y = map->h - origin_y;
                max_radius_x = MAX(max_radius_x, origin_x);
                max_radius_y = MAX(max_radius_y, origin_y);
                max_radius =
                    (int)(sqrt
                          (max_radius_x * max_radius_x +
                           max_radius_y * max_radius_y)) + 1;
        }
        r2 = max_radius * max_radius;

        /* recursive shadow casting */
        for (oct = 0; oct < 8; oct++)
                fov_octant(map, origin_x, origin_y, 1, 1.0, 0.0, max_radius, r2,
                           mult[0][oct], mult[1][oct], mult[2][oct],
                           mult[3][oct], 0);

        /* origin is always visible */
        map->vis[origin_x + origin_y * map->w] = 1;
}
