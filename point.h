/**
 * Simple (x, y, z) point geometry.
 * Copyright (c) 2019 Gordon McNutt
 */
#ifndef point_h
#define point_h

typedef int point_t[3];
typedef int matrix_t[2][2];

/* Indices into the fixed rotations array. */
typedef enum {
        ROTATE_0,
        ROTATE_90,
        ROTATE_180,
        ROTATE_270,
        N_ROTATIONS
} rotation_t;

/* Indices into points. */
enum {
        X,
        Y,
        Z,
        N_DIMENSIONS
};

#define point_init(p) ((p) = {0, 0, 0})
#define point_copy(f, t) do {\
                (t)[X] = (f)[X];\
                (t)[Y] = (f)[Y];\
                (t)[Z] = (f)[Z];\
        } while(0);


/* Fixed rotations. */
static const matrix_t rotations[N_ROTATIONS] = {
        {{1, 0},                /* 0 degrees */
         {0, 1}},
        {{0, 1},                /* 90 degrees */
         {-1, 0}},
        {{-1, 0},               /* 180 degrees */
         {0, -1}},
        {{0, -1},               /* 270 degrees */
         {1, 0}}
};

/**
 * Rotate a point in place by a fixed rotation.
 */
static inline void point_rotate(point_t p, rotation_t r)
{
        const matrix_t *m = &rotations[r];
        int x = p[X], y = p[Y];
        p[X] = x * (*m)[0][0] + y * (*m)[0][1];
        p[Y] = x * (*m)[1][0] + y * (*m)[1][1];
}

/**
 * Translate a point in place by a vector.
 */
static inline void point_translate(point_t p, point_t vec)
{
        p[X] += vec[X];
        p[Y] += vec[Y];
        p[Z] += vec[Z];
}


#endif
