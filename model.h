/**
 * Representation of a simple 3-faced isometric "model".
 * Copyright (c) 2019 Gordon McNutt
 */
#ifndef model_h
#define model_h

#include <SDL2/SDL.h>

enum {
        MODEL_FACE_LEFT,
        MODEL_FACE_RIGHT,
        MODEL_FACE_TOP,
        N_MODEL_FACES
};

typedef struct {
        /* Textures for each face. */
        SDL_Texture *textures[N_MODEL_FACES];

        /* Pixel offsets for each face wrt the base tile origin. */
        SDL_Rect offsets[N_MODEL_FACES];

        /* Total height in tiles when rendered. */
        size_t tile_h;
} model_t;


/**
 * Initialize a model.
 *
 * Set up the textures, offsets and tile_h fields.
 */
void model_init(model_t * model, SDL_Texture ** textures, const size_t * texture_indices, int tile_h);

#endif
