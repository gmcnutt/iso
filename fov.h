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
#ifndef fov_h
#define fov_h

typedef struct {
        int w, h;
        char *opq;              /* opacity set by caller; 0 is transparent */
        char *vis;              /* visibility set by fov() */
} fov_map_t;

/**
 * Initialize/deinitialize an fov_map, allocating/deallocating the arrays.
 */
int fov_init(fov_map_t * fov, int w, int h);
void fov_deinit(fov_map_t * fov);


/**
 * Compute the field of view for a grid.
 */
void fov(fov_map_t * map, int origin_x, int origin_y, int max_radius);


#endif
