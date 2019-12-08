#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <getopt.h>
#include <stdio.h>

#include <gcu.h>

#include "fov.h"
#include "iso.h"

typedef uint32_t pixel_t;

enum {
        MODEL_RENDER_FLAG_TRANSPARENT = 1,
        MODEL_RENDER_FLAG_SKIPLEFT = 2,
        MODEL_RENDER_FLAG_SKIPRIGHT = 4
};

enum {
        GRASS_TEXTURE = 0,
        GRAY_LEFT_TEXTURE,
        GRAY_RIGHT_TEXTURE,
        GRAY_TOP_TEXTURE,
        SHORT_GRAY_LEFT_TEXTURE,
        SHORT_GRAY_RIGHT_TEXTURE,
        SHORT_GRAY_TOP_TEXTURE,
        N_TEXTURES
};

enum {
        MODEL_FACE_LEFT,
        MODEL_FACE_RIGHT,
        MODEL_FACE_TOP,
        N_MODEL_FACES
};

enum {
        PIXEL_VALUE_TREE = 0x004001ff,
        PIXEL_VALUE_SHRUB = 0x008000ff,
        PIXEL_VALUE_GRASS = 0x00ff00ff,
        PIXEL_VALUE_WALL = 0xffffffff
};

enum {
        PIXEL_MASK_OPAQUE = 0x00000100  /* low bit of blue => opaque */
};

struct args {
        char *filename;
        char *cmd;
        bool fov;
        bool delay;
        bool transparency;
};

typedef struct {
        SDL_Texture *textures[N_MODEL_FACES];

        /* The offsets shift textures from the default position where the tile
         * would normally be placed */
        SDL_Rect offsets[N_MODEL_FACES];

        size_t height;          /* total height in pixels */
} model_t;


#define FIRST_SHORT_GRAY_TEXTURE SHORT_GRAY_LEFT_TEXTURE
#define FIRST_GRAY_TEXTURE GRAY_LEFT_TEXTURE
#define FPS 60
#define MAP_H (map_surface->h)
#define MAP_W (map_surface->w)
#define TILE_HEIGHT 18
#define TILE_HEIGHT_HALF (TILE_HEIGHT / 2)
#define TILE_WIDTH 36
#define TILE_WIDTH_HALF (TILE_WIDTH / 2)
#define TICK_PER_FRAME (1000 / FPS)
#define VIEW_H 41
#define VIEW_W 41
/* The isometric view is rotated 45 degrees clockwise. This means the tile at
 * the lower left corner of the view (0, VIEW_H) should be at the left of the
 * screen. */
#define VIEW_OFFSET ((VIEW_H - 1) * TILE_WIDTH_HALF)

#define between(x, l, r) (((l) < (x)) && (x) < (r))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define opaque_at(x, y) (get_pixel((x), (y)) & PIXEL_MASK_OPAQUE)
#define truncate_wall_at(x, y) (between(x, cursor_x - 1, cursor_x + 6) && between(y, cursor_y - 1, cursor_y + 6))
#define tall_model_at(x, y) (get_pixel((x), (y)) == PIXEL_VALUE_WALL || get_pixel((x), (y)) == PIXEL_VALUE_TREE)

static const char *texture_files[] = {
        "grass.png",
        "gray_left.png",
        "gray_right.png",
        "gray_top.png",
        "short_gray_left.png",
        "short_gray_right.png",
        "gray_top.png"
};

static SDL_Surface *map_surface = NULL; /* the map image */
static fov_map_t map;
static const int FOV_RAD = 32;
static int cursor_x = 0;
static int cursor_y = 0;
static model_t gray_model = { 0 };
static model_t short_gray_model = { 0 };

/**
 * XXX: this could be done as a preprocessing step that generates a header
 * file with static declarations of all the model data.
 */
static void model_setup(model_t * model, SDL_Texture ** textures)
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
                        if (opaque_at(x, y)) {
                                map.opq[index] = 1;
                        }
                }
        }

}


/**
 * Handle key presses.
 */
void on_keydown(SDL_KeyboardEvent * event, int *quit, bool * transparency)
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
        case SDLK_t:
                *transparency = !(*transparency);
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
        printf("Usage:  demo [options] [command]\n");
        printf("Options: \n");
        printf("  -d: disable delay (show true framerate)\n");
        printf("  -f: disable fov\n");
        printf("  -h: help\n");
        printf("  -i: image filename\n");
        printf("  -t: enable transparency\n");
}


/**
 * Parse command-line args.
 */
static void parse_args(int argc, char **argv, struct args *args)
{
        int c = 0;

        /* Set defaults */
        memset(args, 0, sizeof (*args));
        args->fov = true;
        args->delay = true;

        /* Get user args */
        while ((c = getopt(argc, argv, "i:hfdt")) != -1) {
                switch (c) {
                case 'd':
                        args->delay = false;
                        break;
                case 'f':
                        args->fov = false;
                        break;
                case 'i':
                        args->filename = optarg;
                        break;
                case 'h':
                        print_usage();
                        exit(0);
                case 't':
                        args->transparency = true;
                        break;
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
                     !tall_model_at(map_x - 1, map_y - 1))) {
                        return 1;
                }
                img_h -= TILE_HEIGHT;
                map_x -= 1;
                map_y -= 1;
        }
        return 0;
}

static void model_render(SDL_Renderer * renderer, model_t * model, int view_x,
                         int view_y, Uint8 red, Uint8 grn, Uint8 blu, int flags)
{
        for (int j = 0; j < N_MODEL_FACES; j++) {

                if (j == MODEL_FACE_LEFT &&
                    (flags & MODEL_RENDER_FLAG_SKIPLEFT)) {
                        continue;
                }
                if (j == MODEL_FACE_RIGHT &&
                    (flags & MODEL_RENDER_FLAG_SKIPRIGHT)) {
                        continue;
                }
                SDL_Texture *texture = model->textures[j];
                SDL_Rect *offset = &model->offsets[j];
                SDL_Rect dst;
                dst.w = model->offsets[j].w;
                dst.h = model->offsets[j].h;
                dst.x = view_to_screen_x(view_x, view_y) + offset->x;
                dst.y = view_to_screen_y(view_x, view_y) - offset->y;
                Uint8 alpha =
                    (flags & MODEL_RENDER_FLAG_TRANSPARENT) ? 128 : 255;
                SDL_SetTextureAlphaMod(texture, alpha);
                SDL_SetTextureColorMod(texture, red, grn, blu);
                SDL_RenderCopy(renderer, texture, NULL, &dst);
        }
}

static void render_iso_test(SDL_Renderer * renderer, SDL_Texture ** textures,
                            bool transparency)
{
        SDL_Rect src, dst;

        src.x = 0;
        src.y = 0;
        src.w = TILE_WIDTH;
        src.h = TILE_HEIGHT;

        /* A view looks at a part of the map (or the whole map, if the map is
         * small). It is centered on the cursor. Each view tile corresponds to
         * a map tile. I have to convert between view, tile and screen
         * coordinates to blit the right map tile at the right screen
         * position. */

        /* Recompute fov based on player's position */
        fov(&map, cursor_x, cursor_y, FOV_RAD);

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
                        if (map_x == cursor_x && map_y == cursor_y) {
                                model_render(renderer, &gray_model, view_x,
                                             view_y, 255, 128, 64, 0);
                                continue;
                        }
                        if (map.vis[map_index]) {
                                pixel_t pixel = get_pixel(map_x, map_y);
                                model_t *model = NULL;
                                int flags = 0;
                                switch (pixel) {
                                case PIXEL_VALUE_GRASS:
                                        dst.x =
                                            view_to_screen_x(view_x, view_y);
                                        dst.y =
                                            view_to_screen_y(view_x, view_y);
                                        dst.w = TILE_WIDTH;
                                        dst.h = TILE_HEIGHT;

                                        SDL_RenderCopy(renderer,
                                                       textures[GRASS_TEXTURE],
                                                       &src, &dst);
                                        break;
                                case PIXEL_VALUE_WALL:
                                case PIXEL_VALUE_TREE:

                                        /* Truncate walls between the focus and the camera. */
                                        if (truncate_wall_at(map_x, map_y)) {
                                                model = &short_gray_model;
                                        } else {
                                                model = &gray_model;
                                        }

                                        /* Make walls that block fov beyond the focus see-through. */
                                        if (transparency) {
                                                if (blocks_fov
                                                    (map_x, map_y,
                                                     model->height)) {
                                                        flags |=
                                                            MODEL_RENDER_FLAG_TRANSPARENT;
                                                }
                                        };

                                        /* Don't blit faces that overlap faces
                                         * behind them. It's inefficient and
                                         * when transparency is applied it
                                         * looks chaotic.  */
                                        if (tall_model_at(map_x, map_y + 1)
                                            && in_fov(map_x,
                                                      map_y + 1) &&
                                            ((model ==
                                              &short_gray_model) ||
                                             !(truncate_wall_at
                                               (map_x, map_y + 1)))) {
                                                flags |=
                                                    MODEL_RENDER_FLAG_SKIPLEFT;
                                        }
                                        if (tall_model_at
                                            (map_x + 1, map_y) &&
                                            in_fov(map_x + 1,
                                                   map_y) &&
                                            ((model ==
                                              &short_gray_model) ||
                                             !(truncate_wall_at
                                               (map_x + 1, map_y)))) {
                                                flags |=
                                                    MODEL_RENDER_FLAG_SKIPRIGHT;
                                        }

                                        if (pixel == PIXEL_VALUE_WALL) {
                                                model_render(renderer, model,
                                                             view_x, view_y, 64, 96,
                                                             128, flags);
                                        } else {
                                                model_render(renderer, model,
                                                             view_x, view_y, 64, 128,
                                                             64, flags);
                                        }
                                        break;
                                case PIXEL_VALUE_SHRUB:
                                        model_render(renderer,
                                                     &short_gray_model, view_x,
                                                     view_y, 128, 255, 128,
                                                     flags);
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
        SDL_SetRenderDrawColor(renderer, 0, 64, 64, 128);
        iso_grid(renderer, VIEW_W, VIEW_H);

        /* Paint a red square for a cursor position */
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
        iso_square(renderer, VIEW_H, map_to_view_x(cursor_x),
                   map_to_view_y(cursor_y));
}

static void render(SDL_Renderer * renderer, SDL_Texture ** textures,
                   bool transparency)
{
        clear_screen(renderer);
        render_iso_test(renderer, textures, transparency);
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

        printf("%s %dx%d\n", filename, surface->w, surface->h);

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
        Uint32 start_ticks, end_ticks, frames = 0, pre_tick;
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

        model_setup(&gray_model, &textures[FIRST_GRAY_TEXTURE]);
        model_setup(&short_gray_model, &textures[FIRST_SHORT_GRAY_TEXTURE]);

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
        if (args.fov) {
                setup_fov();
        }

        start_ticks = SDL_GetTicks();
        pre_tick = SDL_GetTicks();

        while (!done) {
                while (SDL_PollEvent(&event)) {
                        switch (event.type) {
                        case SDL_QUIT:
                                done = 1;
                                break;
                        case SDL_KEYDOWN:
                                on_keydown(&event.key, &done,
                                           &args.transparency);
                                break;
                        case SDL_WINDOWEVENT:
                                frames++;
                                render(renderer, textures, args.transparency);
                                break;
                        default:
                                break;
                        }
                }

                frames++;
                render(renderer, textures, args.transparency);

                if (args.delay) {
                        Uint32 post_tick = SDL_GetTicks();
                        Uint32 used = post_tick - pre_tick;
                        int delay = TICK_PER_FRAME - used;
                        if (delay > 0) {
                                SDL_Delay(delay);
                        }
                        pre_tick = post_tick;
                }
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
