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
        SHORT_WALL_LEFT_TEXTURE,
        SHORT_WALL_RIGHT_TEXTURE,
        SHORT_WALL_TOP_TEXTURE,
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

        /* The offsets shift textures from the default position where the tile
         * would normally be placed */
        SDL_Rect offsets[N_MODEL_FACES];

        size_t height;          /* total height in pixels */
} model_t;


#define FIRST_WALL_TEXTURE WALL_LEFT_TEXTURE
#define FIRST_SHORT_WALL_TEXTURE SHORT_WALL_LEFT_TEXTURE
#define FPS 60
#define MAP_H (map_surface->h)
#define MAP_W (map_surface->w)
#define TILE_HEIGHT 18
#define TILE_HEIGHT_HALF (TILE_HEIGHT / 2)
#define TILE_WIDTH 36
#define TILE_WIDTH_HALF (TILE_WIDTH / 2)
#define TICK_PER_FRAME (1000 / FPS)
#define VIEW_H 31
#define VIEW_W 31
/* The isometric view is rotated 45 degrees clockwise. This means the tile at
 * the lower left corner of the view (0, VIEW_H) should be at the left of the
 * screen. */
#define VIEW_OFFSET ((VIEW_H - 1) * TILE_WIDTH_HALF)


#define wall_at(x, y) (get_pixel((x), (y)) == PIXEL_VALUE_WALL)
#define truncate_wall_at(x, y) ((x) > cursor_x && (y) > cursor_y)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

static const char *texture_files[] = {
        "grass.png",
        "wall_left.png",
        "wall_right.png",
        "wall_top.png",
        "short_wall_left.png",
        "short_wall_right.png",
        "wall_top.png",         /* reuse top texture */
        "map.png"
};

static SDL_Surface *map_surface = NULL; /* the map image */
static fov_map_t map;
static const int FOV_RAD = 32;
static int cursor_x = 0;
static int cursor_y = 0;
static model_t wall_model = { 0 };
static model_t short_wall_model = { 0 };

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

static inline int view_to_screen_x(int view_x, int view_y)
{
        return (view_x - view_y) * TILE_WIDTH_HALF + VIEW_OFFSET;
}

static inline int view_to_screen_y(int view_x, int view_y)
{
        return (view_x + view_y) * TILE_HEIGHT_HALF;
}

static inline int view_to_map_x(size_t view_x)
{
        return cursor_x - VIEW_W / 2 + view_x;
}

static inline int view_to_map_y(size_t view_y)
{
        return cursor_y - VIEW_H / 2 + view_y;
}

static inline int map_to_view_x(size_t map_x)
{
        return map_x + VIEW_W / 2 - cursor_x;
}

static inline int map_to_view_y(size_t map_y)
{
        return map_y + VIEW_H / 2 - cursor_y;
}

static inline size_t map_xy_to_index(size_t map_x, size_t map_y)
{
        return map_y * MAP_W + map_x;
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
                map_x -= 1;
                map_y -= 1;
        }
        return 0;
}

static void render_iso_test(SDL_Renderer * renderer, SDL_Texture ** textures)
{
        SDL_Rect src, dst;

        src.x = 0;
        src.y = 0;
        src.w = TILE_WIDTH;
        src.h = TILE_HEIGHT;

        dst.w = TILE_WIDTH;
        dst.h = TILE_HEIGHT;

        /* A view looks at a part of the map (or the whole map, if the map is
         * small). It is centered on the cursor. Each view tile corresponds to
         * a map tile. I have to convert between view, tile and screen
         * coordinates to blit the right map tile at the right screen
         * position. */

        /* Recompute fov based on player's position */
        fov(&map, cursor_x, cursor_y, FOV_RAD);

        /* Clear the screen to gray */
        clear_screen(renderer);

        /* Clamp the view to the map boundaries. */
        /* size_t view_left, view_right, view_top, view_bottom; */
        /* view_left = max(0, map_to_view_x(0)); */
        /* view_right = min(VIEW_W, map_to_view_x(VIEW_W)); */
        /* view_top = min(0, map_to_view_y(0)); */
        /* view_bottom = max(VIEW_W, map_to_view_y(VIEW_H)); */

        /* Render the map as a tiled view */
        for (int view_y = 0; view_y < VIEW_H; view_y++) {
                int map_y = view_to_map_y(view_y);
                if (map_y < 0 || map_y >= MAP_H) {
                        continue;
                }
                for (int view_x = 0; view_x < VIEW_W; view_x++) {
                        int map_x = view_to_map_x(view_x);
                        if (map_x < 0 || map_x >= MAP_W) {
                                continue;
                        }
                        size_t map_index = map_xy_to_index(map_x, map_y);
                        if (map.vis[map_index]) {
                                pixel_t pixel = get_pixel(map_x, map_y);
                                model_t *model = NULL;
                                switch (pixel) {
                                case PIXEL_VALUE_GRASS:
                                        dst.x =
                                            view_to_screen_x(view_x, view_y);
                                        dst.y =
                                            view_to_screen_y(view_x, view_y);
                                        SDL_RenderCopy(renderer,
                                                       textures[GRASS_TEXTURE],
                                                       &src, &dst);
                                        break;
                                case PIXEL_VALUE_WALL:
                                        model = &wall_model;
                                        /* Don't render walls between the
                                         * player and the camera but show they
                                         * are there. */
                                        if (truncate_wall_at(map_x, map_y)) {
                                                model = &short_wall_model;
                                        }

                                        for (int j = 0; j < N_MODEL_FACES; j++) {
                                                /* Don't render overlapping
                                                 * vertical faces, it looks
                                                 * weird when transparency
                                                 * applies and is wasted cpu
                                                 * when it doesn't. */
                                                if ((j == MODEL_FACE_LEFT &&
                                                     (wall_at(map_x, map_y + 1)
                                                      && in_fov(map_x,
                                                                map_y + 1) &&
                                                      ((model ==
                                                        &short_wall_model) ||
                                                       !(truncate_wall_at
                                                         (map_x, map_y + 1)))))
                                                    || (j == MODEL_FACE_RIGHT &&
                                                        (wall_at
                                                         (map_x + 1, map_y) &&
                                                         in_fov(map_x + 1,
                                                                map_y) &&
                                                         ((model ==
                                                           &short_wall_model) ||
                                                          !(truncate_wall_at
                                                            (map_x + 1,
                                                             map_y)))))) {
                                                        continue;
                                                }

                                                SDL_Texture *texture =
                                                    model->textures[j];
                                                SDL_Rect *offset =
                                                    &model->offsets[j];
                                                dst.w = model->offsets[j].w;
                                                dst.h = model->offsets[j].h;
                                                dst.x =
                                                    view_to_screen_x(view_x,
                                                                     view_y) +
                                                    offset->x;
                                                dst.y =
                                                    view_to_screen_y(view_x,
                                                                     view_y) -
                                                    offset->y;

                                                int transparent =
                                                    blocks_fov(map_x, map_y,
                                                               model->height);
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
                                            ("Unknown pixel value: 0x%08x at (%d, %d)\n",
                                             pixel, map_x, map_y);
                                        break;
                                }
                        }
                }
        }

        /* Paint the grid */
        /* SDL_SetRenderDrawColor(renderer, 0, 64, 64, 128); */
        /* iso_grid(renderer, VIEW_W, VIEW_H); */

        /* Paint a red square for a cursor position */
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
        iso_square(renderer, VIEW_H, map_to_view_x(cursor_x),
                   map_to_view_y(cursor_y));
}

static void render(SDL_Renderer * renderer, SDL_Texture ** textures)
{
        clear_screen(renderer);
        render_iso_test(renderer, textures);
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
        Uint32 start_ticks, end_ticks, frames = 0, pre_tick, post_tick = 0;
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
                                        SDL_WINDOWPOS_UNDEFINED, 640 * 2,
                                        480 * 2,
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
        setup_model(&short_wall_model, &textures[FIRST_SHORT_WALL_TEXTURE]);

        if (!
            (map_surface =
             get_map_surface(args.filename ? args.filename : "map.png"))) {
                goto destroy_textures;
        }

        /* Setup the fov map. */
        map.w = MAP_W;
        map.h = MAP_H;
        map.opq = calloc(1, map.w * map.h);
        map.vis = calloc(1, map.w * map.h);
        setup_fov();

        start_ticks = SDL_GetTicks();
        pre_tick = SDL_GetTicks();

        while (!done) {
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

                frames++;
                render(renderer, textures);
                post_tick = SDL_GetTicks();
                Uint32 used = post_tick - pre_tick;
                int delay = TICK_PER_FRAME - used;
                if (delay > 0) {
                        SDL_Delay(delay);
                }
                pre_tick = post_tick;
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
