#ifndef fov_h
#define fov_h

typedef struct {
        int w, h;
        char *opq;  /* opacity set by caller; 0 is transparent */
        char *vis;  /* visibility set by fov() */
} fov_map_t;

/**
 * Compute the field of view for a grid.
 */
void fov(fov_map_t * map, int origin_x, int origin_y, int max_radius);


#endif
