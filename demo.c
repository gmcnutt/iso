#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <getopt.h>
#include <stdio.h>

#include <gcu.h>

#include "fov.h"
#include "iso.h"

#define TILE_WIDTH 36
#define TILE_HEIGHT 18
#define TILE_WIDTH_HALF (TILE_WIDTH / 2)
#define TILE_HEIGHT_HALF (TILE_HEIGHT / 2)

struct args {
        char *filename;
        char *cmd;
};

enum texture_indices {
        GRASS = 0,
        WALL_LEFT,
        WALL_RIGHT,
        WALL_TOP,
        N_TEXTURES
};

#define FIRST_WALL_TEXTURE WALL_LEFT

static const char *texture_files[] = {
        "grass.png",
        "wall_left.png",
        "wall_right.png",
        "wall_top.png"
};

/* A 3-faced isometric model. Textures are ordered left, right and top. */
enum wall_offsets {
        MODEL_FACE_LEFT,
        MODEL_FACE_RIGHT,
        MODEL_FACE_TOP,
        N_MODEL_FACES
};
typedef struct {
        SDL_Texture *textures[N_MODEL_FACES];
        SDL_Rect offsets[N_MODEL_FACES];        /* Shift textures from the default
                                                 * position where the tile would
                                                 * normally be placed */
        size_t height;          /* total height in pixels */
} model_t;

static model_t wall_model = { 0 };

/**
 * XXX: this could be done as a preprocessing step that generates a header
 * file with static declarations of all the model data.
 */
static void setup_model(model_t * model, SDL_Texture ** textures)
{
        /* Store the textures and their sizes. */
        for (size_t i = 0; i < N_MODEL_FACES; i++) {
                model->textures[i] = textures[i];
                SDL_QueryTexture(textures[i], NULL, NULL, &model->offsets[i].w,
                                 &model->offsets[i].h);
        }

        model->offsets[MODEL_FACE_RIGHT].x =
            model->offsets[MODEL_FACE_LEFT].w;
        model->offsets[MODEL_FACE_RIGHT].y =
            model->offsets[MODEL_FACE_RIGHT].h - TILE_HEIGHT;
        model->offsets[MODEL_FACE_LEFT].y =
            model->offsets[MODEL_FACE_LEFT].h - TILE_HEIGHT;
        model->offsets[MODEL_FACE_TOP].y =
            (model->offsets[MODEL_FACE_LEFT].y +
             model->offsets[MODEL_FACE_TOP].h / 2);
        model->height =
            model->offsets[MODEL_FACE_LEFT].h +
            model->offsets[MODEL_FACE_TOP].h / 2;

}

static const int MAP_W = 13;
static const int MAP_H = 11;
static const int FOV_RAD = 32;
static int cursor_x = 0;
static int cursor_y = 0;
static fov_map_t map;
//static int wall_height;
static grid_t *wallmap = NULL;

typedef struct {
        size_t x, y;
} wall_t;

#define N_WALLS 12
static const wall_t walls[N_WALLS] = {
        {5, 5}, {6, 5}, {7, 5}, {8, 5},
        {5, 6}, {8, 6},
        {5, 7}, {8, 7},
        {5, 8}, {6, 8}, {7, 8}, {8, 8}
};

static void setup_wallmap()
{
        char *marker = mem_alloc(1, NULL);      /* marker that a wall is there */
        wallmap = grid_alloc(MAP_W, MAP_H);
        for (size_t i = 0; i < N_WALLS; i++) {
                grid_put(wallmap, walls[i].x, walls[i].y, marker);
        }
        mem_deref(marker);      /* grid will keep refs */
}

#define wall_at(x, y) (grid_has(wallmap, (x), (y)))


/**
 * Handle key presses.
 */
void on_keydown(SDL_KeyboardEvent * event, int *quit)
{
        switch (event->keysym.sym) {
        case SDLK_LEFT:
                cursor_x--;
                break;
        case SDLK_RIGHT:
                cursor_x++;
                break;
        case SDLK_UP:
                cursor_y--;
                break;
        case SDLK_DOWN:
                cursor_y++;
                break;
        case SDLK_q:
                *quit = 1;
                break;
        default:
                break;
        }
}


/**
 * Print a command-line usage message.
 */
static void print_usage(void)
{
        printf("Usage:  demo [options] [command]\n"
               "Options: \n" "    -h:	help\n" "    -i: image filename\n");
}


/**
 * Parse command-line args.
 */
static void parse_args(int argc, char **argv, struct args *args)
{
        int c = 0;

        memset(args, 0, sizeof (*args));

        while ((c = getopt(argc, argv, "i:h")) != -1) {
                switch (c) {
                case 'i':
                        args->filename = optarg;
                        break;
                case 'h':
                        print_usage();
                        exit(0);
                case '?':
                default:
                        print_usage();
                        exit(-1);
                        break;
                }
        }

}



static void clear_screen(SDL_Renderer * renderer)
{
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
}

static int screen_x(int map_x, int map_y)
{
        return (map_x - map_y) * TILE_WIDTH_HALF;
}

static int screen_y(int map_x, int map_y)
{
        return (map_x + map_y) * TILE_HEIGHT_HALF;
}

static inline int in_fov(int map_x, int map_y)
{
        return map.vis[map_x + map_y * MAP_W];
}

static int blocks_fov(int map_x, int map_y, int img_h)
{
        while (img_h > TILE_HEIGHT) {
                if (
                           //(in_fov(map_x - 1, map_y) && !wall_at(map_x - 1, map_y)) ||
                           //(in_fov(map_x, map_y - 1) && !wall_at(map_x, map_y - 1))||
                           (in_fov(map_x - 1, map_y - 1) &&
                            !wall_at(map_x - 1, map_y - 1))
                    ) {
                        return 1;
                }
                img_h -= TILE_HEIGHT;
        }
        return 0;
}

static void render_iso_test(SDL_Renderer * renderer, SDL_Texture ** textures,
                            int off_x, int off_y, int map_w, int map_h)
{
        int row, col, idx, map_x;
        SDL_Rect src, dst;

        src.x = 0;
        src.y = 0;
        src.w = TILE_WIDTH;
        src.h = TILE_HEIGHT;

        dst.w = TILE_WIDTH;
        dst.h = TILE_HEIGHT;

        map_x = screen_x(map_h - 1, 0);

        /* Clear the screen to gray */
        clear_screen(renderer);

        /* Compute fov */
        for (int i = 0; i < N_WALLS; i++) {
                map.opq[walls[i].x + walls[i].y * MAP_W] = 1;
        }

        fov(&map, cursor_x, cursor_y, FOV_RAD);

        /* Paint the ground in fov */
        for (row = 0, idx = 0; row < map_h; row++) {
                for (col = 0; col < map_w; col++, idx++) {
                        if (map.vis[idx]) {
                                dst.x = screen_x(col, row) + map_x;
                                dst.y = screen_y(col, row);
                                SDL_RenderCopy(renderer,
                                               textures[GRASS], &src, &dst);
                        }
                }
        }

        /* Paint the grid */
        SDL_SetRenderDrawColor(renderer, 0, 64, 64, 128);
        iso_grid(renderer, map_w, map_h);

        /* Paint a column, semi-transparent if the cursor should be able to see
         * beyond it. */
        for (int i = 0; i < N_WALLS; i++) {
                int col_x = walls[i].x, col_y = walls[i].y;
                if (map.vis[col_x + col_y * MAP_W]) {
                        for (int j = 0; j < N_MODEL_FACES; j++) {
                                int transparent =
                                    blocks_fov(col_x, col_y, wall_model.height);
                                if ((j == MODEL_FACE_LEFT &&
                                     (wall_at(col_x, col_y + 1) &&
                                      in_fov(col_x, col_y + 1))) ||
                                    (j == MODEL_FACE_RIGHT &&
                                     (wall_at(col_x + 1, col_y) &&
                                      in_fov(col_x + 1, col_y)))) {
                                        continue;
                                }

                                SDL_Texture *texture =
                                    textures[j + FIRST_WALL_TEXTURE];
                                SDL_Rect *offset = &wall_model.offsets[j];
                                dst.w = wall_model.offsets[j].w;
                                dst.h = wall_model.offsets[j].h;
                                dst.x =
                                    screen_x(col_x, col_y) + map_x + offset->x;
                                dst.y = screen_y(col_x, col_y) - offset->y;

                                if (transparent) {
                                        SDL_SetTextureAlphaMod(texture, 128);
                                } else {
                                        SDL_SetTextureAlphaMod(texture, 255);

                                };
                                SDL_RenderCopy(renderer, texture, NULL, &dst);
                        }
                }
        }

        /* Paint a red square for a cursor position */
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
        iso_square(renderer, map_w, map_h, cursor_x, cursor_y);


}

static void render(SDL_Renderer * renderer, SDL_Texture ** textures)
{
        clear_screen(renderer);
        render_iso_test(renderer, textures, 0, 0, MAP_W, MAP_H);
        SDL_RenderPresent(renderer);
}

static SDL_Texture *load_texture(SDL_Renderer * renderer, const char *filename)
{
        SDL_Surface *surface = NULL;
        SDL_Texture *texture = NULL;

        if (!(surface = IMG_Load(filename))) {
                printf("%s:IMG_Load:%s\n", __FUNCTION__, SDL_GetError());
                return NULL;
        }

        if (!(texture = SDL_CreateTextureFromSurface(renderer, surface))) {
                printf("%s:SDL_CreateTextureFromSurface:%s\n",
                       __FUNCTION__, SDL_GetError());
        }

        SDL_FreeSurface(surface);

        return texture;
}

int main(int argc, char **argv)
{
        SDL_Event event;
        SDL_Window *window = NULL;
        SDL_Renderer *renderer = NULL;
        SDL_Texture *textures[N_TEXTURES] = { 0 };

        int done = 0;
        Uint32 start_ticks, end_ticks, frames = 0;
        struct args args;


        parse_args(argc, argv, &args);

        /* Init SDL */
        if (SDL_Init(SDL_INIT_VIDEO)) {
                printf("SDL_Init: %s\n", SDL_GetError());
                return -1;
        }

        /* Cleanup SDL on exit. */
        atexit(SDL_Quit);

        /* Create the main window */
        if (!(window = SDL_CreateWindow("Demo", SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED, 640, 480,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN)))
        {
                printf("SDL_CreateWindow: %s\n", SDL_GetError());
                return -1;
        }

        /* Create the renderer. */
        if (!(renderer = SDL_CreateRenderer(window, -1, 0))) {
                printf("SDL_CreateRenderer: %s\n", SDL_GetError());
                goto destroy_window;
        }

        /* Load the textures */
        for (int i = 0; i < N_TEXTURES; i++) {
                if (!(textures[i] = load_texture(renderer, texture_files[i]))) {
                        goto destroy_textures;
                }
        }

        setup_model(&wall_model, &textures[FIRST_WALL_TEXTURE]);
        setup_wallmap();

        /* Setup the fov map. */
        map.w = MAP_W;
        map.h = MAP_H;
        map.opq = calloc(1, map.w * map.h);
        map.vis = calloc(1, map.w * map.h);

        start_ticks = SDL_GetTicks();

        while (!done) {
                frames++;
                while (SDL_PollEvent(&event)) {
                        switch (event.type) {
                        case SDL_QUIT:
                                done = 1;
                                break;
                        case SDL_KEYDOWN:
                                on_keydown(&event.key, &done);
                                break;
                        case SDL_WINDOWEVENT:
                                frames++;
                                render(renderer, textures);
                                break;
                        default:
                                break;
                        }
                }

                render(renderer, textures);
        }

        end_ticks = SDL_GetTicks();
        printf("Frames: %d\n", frames);
        if (end_ticks > start_ticks) {
                printf("%2.2f FPS\n",
                       ((double)frames * 1000) / (end_ticks - start_ticks)
                    );
        }

destroy_textures:
        for (int i = 0; i < N_TEXTURES; i++) {
                if (textures[i]) {
                        SDL_DestroyTexture(textures[i]);
                } else {
                        break;
                }
        }
//destroy_renderer:
        SDL_DestroyRenderer(renderer);
destroy_window:
        SDL_DestroyWindow(window);

        return 0;
}
