#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <getopt.h>
#include <stdio.h>

#include <gcu.h>

#include "fov.h"
#include "iso.h"

typedef uint32_t pixel_t;

enum texture_indices {
        GRASS_TEXTURE = 0,
        WALL_LEFT_TEXTURE,
        WALL_RIGHT_TEXTURE,
        WALL_TOP_TEXTURE,
        N_TEXTURES
};

enum wall_offsets {
        MODEL_FACE_LEFT,
        MODEL_FACE_RIGHT,
        MODEL_FACE_TOP,
        N_MODEL_FACES
};

enum pixel_values {
        PIXEL_VALUE_GRASS = 0x00ff00ff,
        PIXEL_VALUE_WALL = 0xffffffff
};

struct args {
        char *filename;
        char *cmd;
};

typedef struct {
        SDL_Texture *textures[N_MODEL_FACES];
        SDL_Rect offsets[N_MODEL_FACES];        /* Shift textures from the default
                                                 * position where the tile would
                                                 * normally be placed */
        size_t height;          /* total height in pixels */
} model_t;


#define FIRST_WALL_TEXTURE WALL_LEFT_TEXTURE
#define MAP_H (map_surface->h)
#define MAP_W (map_surface->w)
#define TILE_HEIGHT 18
#define TILE_HEIGHT_HALF (TILE_HEIGHT / 2)
#define TILE_WIDTH 36
#define TILE_WIDTH_HALF (TILE_WIDTH / 2)

#define wall_at(x, y) (get_pixel((x), (y)) == PIXEL_VALUE_WALL)


static const char *texture_files[] = {
        "grass.png",
        "wall_left.png",
        "wall_right.png",
        "wall_top.png",
        "map.png"
};

static SDL_Surface *map_surface = NULL; /* the map image */
static fov_map_t map;
static const int FOV_RAD = 32;
static int cursor_x = 0;
static int cursor_y = 0;
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

        model->offsets[MODEL_FACE_RIGHT].x = model->offsets[MODEL_FACE_LEFT].w;
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


static inline pixel_t get_pixel(size_t x, size_t y)
{
        /* The pitch is the length of a row of pixels in bytes. Find the first
         * byte of the desired pixel then cast it to an RGBA 32 bit value to
         * check it. */
        size_t offset = (y * map_surface->pitch + x * sizeof (pixel_t));
        uint8_t *byteptr = (uint8_t *) map_surface->pixels;
        byteptr += offset;
        uint32_t *pixelptr = (uint32_t *) byteptr;
        return *pixelptr;
}


static SDL_Surface *get_map_surface(const char *filename)
{
        SDL_Surface *surface;

        /* Load the image file that has the map */
        if (!(surface = IMG_Load(filename))) {
                printf("%s:IMG_Load:%s\n", __FUNCTION__, SDL_GetError());
                return NULL;
        }

        /* Make sure the pixels are in RGBA format, 8 bits per pixel */
        if (surface->format->format != SDL_PIXELFORMAT_RGBA8888) {
                SDL_Surface *tmp;
                tmp =
                    SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888,
                                             0);
                SDL_FreeSurface(surface);
                surface = tmp;
                if (!surface) {
                        printf("%s:SDL_ConvertSurfaceFormat:%s\n", __FUNCTION__,
                               SDL_GetError());
                        return NULL;
                }
        }

        return surface;
}

static void setup_fov()
{
        for (size_t y = 0, index = 0; y < MAP_H; y++) {
                for (size_t x = 0; x < MAP_W; x++, index++) {
                        if (wall_at(x, y)) {
                                map.opq[index] = 1;
                        }
                }
        }

}


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

static inline int screen_x(int map_x, int map_y)
{
        return (map_x - map_y) * TILE_WIDTH_HALF;
}

static inline int screen_y(int map_x, int map_y)
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
                if ((in_fov(map_x - 1, map_y - 1) &&
                     !wall_at(map_x - 1, map_y - 1))) {
                        return 1;
                }
                img_h -= TILE_HEIGHT;
        }
        return 0;
}

static void render_iso_test(SDL_Renderer * renderer, SDL_Texture ** textures,
                            int off_x, int off_y, int map_w, int map_h)
{
        size_t row, col, idx, map_x;
        SDL_Rect src, dst;

        src.x = 0;
        src.y = 0;
        src.w = TILE_WIDTH;
        src.h = TILE_HEIGHT;

        dst.w = TILE_WIDTH;
        dst.h = TILE_HEIGHT;

        map_x = screen_x(map_h - 1, 0);

        /* Recompute fov based on player's position */
        fov(&map, cursor_x, cursor_y, FOV_RAD);

        /* Clear the screen to gray */
        clear_screen(renderer);

        /* Render the map as a tiled view */
        for (row = 0, idx = 0; row < map_h; row++) {
                for (col = 0; col < map_w; col++, idx++) {
                        if (map.vis[idx]) {
                                pixel_t pixel = get_pixel(col, row);
                                switch (pixel) {
                                case PIXEL_VALUE_GRASS:
                                        dst.x = screen_x(col, row) + map_x;
                                        dst.y = screen_y(col, row);
                                        SDL_RenderCopy(renderer,
                                                       textures[GRASS_TEXTURE],
                                                       &src, &dst);
                                        break;
                                case PIXEL_VALUE_WALL:
                                        for (int j = 0; j < N_MODEL_FACES; j++) {
                                                if ((j == MODEL_FACE_LEFT &&
                                                     (wall_at(col, row + 1) &&
                                                      in_fov(col, row + 1))) ||
                                                    (j == MODEL_FACE_RIGHT &&
                                                     (wall_at(col + 1, row) &&
                                                      in_fov(col + 1, row)))) {
                                                        continue;
                                                }

                                                SDL_Texture *texture =
                                                    textures[j +
                                                             FIRST_WALL_TEXTURE];
                                                SDL_Rect *offset =
                                                    &wall_model.offsets[j];
                                                dst.w = wall_model.offsets[j].w;
                                                dst.h = wall_model.offsets[j].h;
                                                dst.x =
                                                    screen_x(col,
                                                             row) + map_x +
                                                    offset->x;
                                                dst.y =
                                                    screen_y(col,
                                                             row) - offset->y;

                                                int transparent =
                                                    blocks_fov(col, row,
                                                               wall_model.
                                                               height);
                                                if (transparent) {
                                                        SDL_SetTextureAlphaMod
                                                            (texture, 128);
                                                } else {
                                                        SDL_SetTextureAlphaMod
                                                            (texture, 255);
                                                };

                                                SDL_RenderCopy(renderer,
                                                               texture, NULL,
                                                               &dst);
                                        }
                                        break;
                                default:
                                        printf
                                            ("Unknown pixel value: 0x%08x at (%ld, %ld)\n",
                                             pixel, col, row);
                                        break;
                                }
                        }
                }
        }

        /* Paint the grid */
        SDL_SetRenderDrawColor(renderer, 0, 64, 64, 128);
        iso_grid(renderer, map_w, map_h);

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

        if (!(map_surface = get_map_surface(args.filename ? args.filename : "map.png"))) {
                goto destroy_textures;
        }

        /* Setup the fov map. */
        map.w = MAP_W;
        map.h = MAP_H;
        map.opq = calloc(1, map.w * map.h);
        map.vis = calloc(1, map.w * map.h);
        setup_fov();

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
        SDL_FreeSurface(map_surface);

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
