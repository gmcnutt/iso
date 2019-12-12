#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <getopt.h>
#include <stdio.h>

#include <gcu.h>

#include "fov.h"
#include "iso.h"

typedef uint32_t pixel_t;
typedef int point_t[3];
typedef int matrix_t[2][2];

enum {
        MODEL_RENDER_FLAG_TRANSPARENT = 1,
        MODEL_RENDER_FLAG_SKIPLEFT = 2,
        MODEL_RENDER_FLAG_SKIPRIGHT = 4
};

enum {
        TEXTURE_GRASS,
        TEXTURE_LEFTTALL,
        TEXTURE_RIGHTTALL,
        TEXTURE_TOP,
        TEXTURE_LEFTSHORT,
        TEXTURE_RIGHTSHORT,
        TEXTURE_INTERIOR,
        N_TEXTURES
};

enum {
        MODEL_FACE_LEFT,
        MODEL_FACE_RIGHT,
        MODEL_FACE_TOP,
        N_MODEL_FACES
};

enum {
        MODEL_TALL,
        MODEL_SHORT,
        MODEL_INTERIOR,
        N_MODELS
};

enum {
        PIXEL_VALUE_TREE = 0x004001ff,
        PIXEL_VALUE_SHRUB = 0x008000ff,
        PIXEL_VALUE_GRASS = 0x00ff00ff,
        PIXEL_VALUE_FLOOR = 0x0000f0ff,
        PIXEL_VALUE_WALL = 0xffffffff
};

enum {
        PIXEL_MASK_OPAQUE = 0x00000100, /* blue bit 0 */
        PIXEL_MASK_IMPASSABLE = 0x00000200      /* blue bit 1 */
};

typedef enum {
        ROTATE_0,
        ROTATE_90,
        ROTATE_180,
        ROTATE_270,
        N_ROTATIONS
} rotation_t;

enum {
        X,
        Y,
        Z,
        N_DIMENSIONS
};

enum {
        MAP_FLOOR1,
        MAP_FLOOR2,
        N_MAPS
};

struct args {
        char *filename;
        char *filename_l2;
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

        size_t tile_h;          /* total height in tiles */
} model_t;

#define point_init(p) ((p) = {0, 0, 0})
#define point_copy(f, t) do {\
                (t)[X] = (f)[X];\
                (t)[Y] = (f)[Y];\
                (t)[Z] = (f)[Z];\
        } while(0);

#define FPS 60
#define TILE_HEIGHT 18
#define TILE_HEIGHT_HALF (TILE_HEIGHT / 2)
#define TILE_WIDTH 36
#define TILE_WIDTH_HALF (TILE_WIDTH / 2)
#define TICK_PER_FRAME (1000 / FPS)
#define VIEW_H 35
#define VIEW_W 35
/* The isometric view is rotated 45 degrees clockwise. This means the tile at
 * the lower left corner of the view (0, VIEW_H) should be at the left of the
 * screen. */
#define VIEW_OFFSET ((VIEW_H - 1) * TILE_WIDTH_HALF)

#define between(x, l, r) (((l) < (x)) && (x) < (r))
#define between_inc(x, l, r) (((l) <= (x)) && (x) <= (r))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define map_opaque_at(m, x, y) (map_get_pixel((m), (x), (y)) & PIXEL_MASK_OPAQUE)
#define map_passable_at(m, x, y) (!(map_get_pixel((m), (x), (y)) & PIXEL_MASK_IMPASSABLE))
#define map_contains(m, x, y) \
        (between_inc((x), map_left(m), map_right(m)) && \
         between_inc((y), map_top(m), map_bottom(m)))
#define map_h(m) ((m)->h)
#define map_w(m) ((m)->w)
#define map_left(m) 0
#define map_right(m) ((m)->w - 1)
#define map_top(m) 0
#define map_bottom(m) ((m)->h - 1)

static const char *texture_files[] = {
        "grass.png",
        "gray_left.png",
        "gray_right.png",
        "gray_top.png",
        "short_gray_left.png",
        "short_gray_right.png",
        "gray_interior.png"
};

static const size_t texture_indices[N_MODELS][N_MODEL_FACES] = {
        {TEXTURE_LEFTTALL, TEXTURE_RIGHTTALL, TEXTURE_TOP},     /* tall */
        {TEXTURE_LEFTSHORT, TEXTURE_RIGHTSHORT, TEXTURE_TOP},   /* short */
        {TEXTURE_LEFTSHORT, TEXTURE_RIGHTSHORT, TEXTURE_INTERIOR}       /* interior */
};


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

static fov_map_t fov_map;
static const int FOV_RAD = 32;
static point_t cursor = { 0 };

static rotation_t camera_rotation = ROTATE_0;
static char rendered[VIEW_W * VIEW_H] = { 0 };
static model_t models[N_MODELS] = { 0 };
static SDL_Surface *maps[N_MAPS];

/**
 * XXX: this could be done as a preprocessing step that generates a header
 * file with static declarations of all the model data.
 */
static void model_setup(model_t * model, SDL_Texture ** textures,
                        const size_t * texture_indices)
{
        /* Store the textures and their sizes. */
        for (size_t i = 0; i < N_MODEL_FACES; i++) {
                int texture_index = texture_indices[i];
                model->textures[i] = textures[texture_index];
                SDL_QueryTexture(model->textures[i], NULL, NULL,
                                 &model->offsets[i].w, &model->offsets[i].h);
        }

        model->offsets[MODEL_FACE_RIGHT].x = model->offsets[MODEL_FACE_LEFT].w;
        model->offsets[MODEL_FACE_RIGHT].y =
            model->offsets[MODEL_FACE_RIGHT].h - TILE_HEIGHT;
        model->offsets[MODEL_FACE_LEFT].y =
            model->offsets[MODEL_FACE_LEFT].h - TILE_HEIGHT;
        model->offsets[MODEL_FACE_TOP].y =
            (model->offsets[MODEL_FACE_LEFT].y +
             model->offsets[MODEL_FACE_TOP].h / 2);
        int screen_h =
            model->offsets[MODEL_FACE_LEFT].h +
            model->offsets[MODEL_FACE_TOP].h / 2;
        model->tile_h = screen_h / TILE_HEIGHT;
}


static inline pixel_t map_get_pixel(SDL_Surface * map, size_t x, size_t y)
{
        /* The pitch is the length of a row of pixels in bytes. Find the first
         * byte of the desired pixel then cast it to an RGBA 32 bit value to
         * check it. */
        size_t offset = (y * map->pitch + x * sizeof (pixel_t));
        uint8_t *byteptr = (uint8_t *) map->pixels;
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

static void setup_fov(SDL_Surface *map)
{
        for (size_t y = 0, index = 0; y < map_h(map); y++) {
                for (size_t x = 0; x < map_w(map); x++, index++) {
                        if (map_opaque_at(map, x, y)) {
                                fov_map.opq[index] = 1;
                        }
                }
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
                        /* A second level map can follow a comma */
                        if ((args->filename_l2 = strchr(optarg, ','))) {
                                *args->filename_l2 = '\0';
                                args->filename_l2++;
                        }
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

static inline int screen_to_view_x(int screen_x, int screen_y)
{
        screen_x -= (VIEW_OFFSET + TILE_WIDTH_HALF);
        return (int)((float)screen_x / (float)TILE_WIDTH +
                     (float)screen_y / (float)TILE_HEIGHT);
}

static inline int screen_to_view_y(int screen_x, int screen_y)
{
        screen_x -= (VIEW_OFFSET + TILE_WIDTH_HALF);
        return (int)((float)screen_y / (float)TILE_HEIGHT -
                     (float)screen_x / (float)TILE_WIDTH);
}

static inline int view_to_screen_x(int view_x, int view_y)
{
        return (view_x - view_y) * TILE_WIDTH_HALF + VIEW_OFFSET;
}

static inline int view_to_screen_y(int view_x, int view_y, int view_z)
{
        return (view_x + view_y) * TILE_HEIGHT_HALF - view_z * TILE_HEIGHT;
}

static inline int view_to_camera_x(int view_x)
{
        return view_x - VIEW_W / 2;
}

static inline int view_to_camera_y(int view_y)
{
        return view_y - VIEW_H / 2;
}

static inline void view_to_camera(point_t view, point_t cam)
{
        cam[X] = view[X] - VIEW_W / 2;
        cam[Y] = view[Y] - VIEW_H / 2;
}

static inline void rotate(point_t p, rotation_t r)
{
        const matrix_t *m = &rotations[r];
        int x = p[X], y = p[Y];
        p[X] = x * (*m)[0][0] + y * (*m)[0][1];
        p[Y] = x * (*m)[1][0] + y * (*m)[1][1];
}

static inline void translate(point_t p, point_t o)
{
        p[X] += o[X];
        p[Y] += o[Y];
}

static inline void view_to_map(point_t view, point_t map)
{
        view_to_camera(view, map);
        rotate(map, camera_rotation);
        map[X] += cursor[X];
        map[Y] += cursor[Y];
}

static inline void map_to_camera(point_t left, point_t right)
{
        left[X] = (right[X] - (right[Z] - cursor[Z])) - cursor[X];
        left[Y] = (right[Y] - (right[Z] - cursor[Z])) - cursor[Y];
}

static inline void map_to_view(point_t l, point_t r)
{
        map_to_camera(l, r);
        l[X] += VIEW_W / 2;
        l[Y] += VIEW_H / 2;
}

static inline size_t map_xy_to_index(SDL_Surface *map, size_t map_x, size_t map_y)
{
        return map_y * map_w(map) + map_x;
}

static inline int in_fov(SDL_Surface *map, int map_x, int map_y)
{
        return fov_map.vis[map_x + map_y * map_w(map)];
}

static inline int view_rendered_at(size_t view_x, size_t view_y)
{
        return rendered[view_y * VIEW_W + view_x];
}

static inline void view_set_rendered(size_t view_x, size_t view_y,
                                     size_t tile_h)
{
        int index = view_y * VIEW_W + view_x;
        while (tile_h && index > 0) {
                rendered[index] = 1;
                tile_h--;
                index -= (VIEW_W + 1);  /* up one row and left one column */
        }
}

static inline void view_clear_rendered()
{
        memset(rendered, 0, sizeof (rendered));
}

static int blocks_fov(int view_x, int view_y, int tile_h)
{
        while (tile_h && view_x > 0 && view_y > 0) {
                if (view_rendered_at(view_x, view_y)) {
                        return 1;
                }
                view_x--;
                view_y--;
                tile_h--;
        }

        return 0;
}

static bool cutaway_at(int view_x, int view_y, int view_z)
{
        int margin = view_z > cursor[Z] ? 4 : 3;
        int cam_x = view_to_camera_x(view_x);
        int cam_y = view_to_camera_y(view_y);
        return (between(cam_x, -margin, 6 + view_z) &&
                between(cam_y, -margin, 6 + view_z));
}

static void model_render(SDL_Renderer * renderer, model_t * model, int view_x,
                         int view_y, int view_z, Uint8 red, Uint8 grn,
                         Uint8 blu, int flags)
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
                dst.y = view_to_screen_y(view_x, view_y, view_z) - offset->y;
                Uint8 alpha =
                    (flags & MODEL_RENDER_FLAG_TRANSPARENT) ? 128 : 255;
                SDL_SetTextureAlphaMod(texture, alpha);
                SDL_SetTextureColorMod(texture, red, grn, blu);
                SDL_RenderCopy(renderer, texture, NULL, &dst);
        }
}

static bool skipface(SDL_Surface *map, point_t nview, bool cutaway)
{
        point_t nmap;
        view_to_map(nview, nmap);
        return (map_opaque_at(map, nmap[X], nmap[Y])
                && in_fov(map, nmap[X], nmap[Y]) &&
                (cutaway ||
                 !(cutaway_at
                   (nview[X], nview[Y], nview[Z]))));
}

static void map_render(SDL_Surface * map, SDL_Renderer * renderer,
                       SDL_Texture ** textures, bool transparency, int view_z)
{
        SDL_Rect src, dst;
        point_t nview;

        src.x = 0;
        src.y = 0;
        src.w = TILE_WIDTH;
        src.h = TILE_HEIGHT;

        /* Render the map as a tiled view */
        for (int view_y = 0; view_y < VIEW_H; view_y++) {
                for (int view_x = 0; view_x < VIEW_W; view_x++) {
                        point_t vloc = { view_x, view_y, view_z };
                        point_t mloc;
                        view_to_map(vloc, mloc);
                        int map_y = mloc[Y];
                        int map_x = mloc[X];

                        if (!(map_contains(map, map_x, map_y))) {
                                continue;
                        }
                        size_t map_index = map_xy_to_index(map, map_x, map_y);
                        if (view_z == cursor[Z] && map_x == cursor[X] &&
                            map_y == cursor[Y]) {
                                model_render(renderer, &models[MODEL_TALL],
                                             view_x, view_y, view_z, 255, 128,
                                             64, 0);
                                continue;
                        }
                        if (fov_map.vis[map_index]) {
                                pixel_t pixel =
                                    map_get_pixel(map, map_x, map_y);
                                if (!pixel) {
                                        /* transparent, nothing there */
                                        continue;
                                }
                                model_t *model = NULL;
                                int flags = 0;

                                /* Cut away anything that obscures the cursor's
                                 * immediate area. */
                                bool cutaway = cutaway_at(view_x, view_y, view_z);
                                if (cutaway && view_z > cursor[Z]) {
                                        continue;
                                }

                                switch (pixel) {
                                case PIXEL_VALUE_FLOOR:
                                        model = &models[MODEL_SHORT];
                                        if (transparency &&
                                            blocks_fov(view_x, view_y, model->tile_h)) {
                                                flags |=
                                                    MODEL_RENDER_FLAG_TRANSPARENT;
                                        }
                                        model_render(renderer, model,
                                                     view_x, view_y, view_z,
                                                     128, 128, 255, flags);
                                        break;
                                case PIXEL_VALUE_GRASS:
                                        dst.x =
                                                view_to_screen_x(view_x, view_y);
                                        dst.y =
                                            view_to_screen_y(view_x, view_y,
                                                             view_z);
                                        dst.w = TILE_WIDTH;
                                        dst.h = TILE_HEIGHT;


                                        if (transparency &&
                                            blocks_fov(view_x, view_y, 1)) {
                                                SDL_SetTextureAlphaMod(textures
                                                                       [TEXTURE_GRASS],
                                                                       128);
                                        } else {

                                                SDL_SetTextureAlphaMod(textures
                                                                       [TEXTURE_GRASS],
                                                                       255);
                                        }

                                        SDL_RenderCopy(renderer,
                                                       textures[TEXTURE_GRASS],
                                                       &src, &dst);
                                        break;
                                case PIXEL_VALUE_WALL:
                                case PIXEL_VALUE_TREE:

                                        /* Truncate cutaway walls. */
                                        if (cutaway) {
                                                model = &models[MODEL_INTERIOR];
                                        } else {
                                                model = &models[MODEL_TALL];
                                        }

                                        if (transparency &&
                                            blocks_fov(view_x, view_y, model->tile_h)) {
                                                flags |=
                                                    MODEL_RENDER_FLAG_TRANSPARENT;
                                        }

                                        /* Don't blit faces that overlap faces
                                         * behind them. */
                                        nview[X] = view_x;
                                        nview[Y] = view_y + 1;
                                        if (skipface(map, nview, cutaway)) {
                                                flags |= MODEL_RENDER_FLAG_SKIPLEFT;
                                        }

                                        nview[X] = view_x + 1;
                                        nview[Y] = view_y;
                                        if (skipface(map, nview, cutaway)) {
                                                flags |= MODEL_RENDER_FLAG_SKIPRIGHT;
                                        }

                                        if (pixel == PIXEL_VALUE_WALL) {
                                                model_render(renderer, model,
                                                             view_x, view_y,
                                                             view_z, 200, 200,
                                                             255, flags);
                                        } else {
                                                model_render(renderer, model,
                                                             view_x, view_y,
                                                             view_z, 64, 128,
                                                             64, flags);
                                        }
                                        break;
                                case PIXEL_VALUE_SHRUB:
                                        model_render(renderer,
                                                     &models[MODEL_SHORT],
                                                     view_x, view_y, view_z,
                                                     128, 255, 128, flags);
                                        break;
                                default:
                                        printf
                                            ("Unknown pixel value: 0x%08x at (%d, %d, %d)\n",
                                             pixel, map_x, map_y, view_z);
                                        break;
                                }

                                view_set_rendered(view_x, view_y,
                                                  model ? model->tile_h : 1);
                        }
                }
        }

}

static void render_iso_test(SDL_Renderer * renderer, SDL_Texture ** textures,
                            bool transparency)
{
        /* Clear the rendered buffer */
        view_clear_rendered();

        /* Recompute fov based on player's position */
        fov(&fov_map, cursor[X], cursor[Y], FOV_RAD);

        /* Render the main surface map */
        map_render(maps[MAP_FLOOR1], renderer, textures, transparency, 0);
        if (maps[MAP_FLOOR2]) {
                map_render(maps[MAP_FLOOR2], renderer, textures, transparency, 5);
        }

        /* Paint the grid */
        SDL_SetRenderDrawColor(renderer, 0, 64, 64, 128);
        iso_grid(renderer, VIEW_W, VIEW_H);

        /* Paint a red square for a cursor position */
        point_t view;
        map_to_view(view, cursor);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
        iso_square(renderer, VIEW_H, view[X], view[Y]);
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

/**
 * Handle button clicks.
 */
void on_mouse_button(SDL_MouseButtonEvent * event)
{
        point_t view = { 0, 0, 0 };
        view[X] = screen_to_view_x(event->x, event->y);
        view[Y] = screen_to_view_y(event->x, event->y);
        point_t map = { 0, 0, 0 };
        view_to_map(view, map);

        printf("s(%d, %d) -> v(%d, %d) -> m(%d, %d) -> %c\n",
               event->x, event->y,
               view[X], view[Y],
               map[X], map[Y], view_rendered_at(view[X], view[Y]) ? 't' : 'f');
}

static void move_cursor(SDL_Surface *map, int dx, int dy)
{
        point_t dir = {dx, dy, 0}, newcursor;
        rotate(dir, camera_rotation);
        point_copy(cursor, newcursor);
        translate(newcursor, dir);
        if (map_contains(map, newcursor[X], newcursor[Y]) &&
            map_passable_at(map, newcursor[X], newcursor[Y])) {
                point_copy(newcursor, cursor);
        }
}

/**
 * Handle key presses.
 */
void on_keydown(SDL_KeyboardEvent * event, int *quit, bool * transparency, SDL_Surface *map)
{
        switch (event->keysym.sym) {
        case SDLK_LEFT:
                move_cursor(map, -1, 0);
                break;
        case SDLK_RIGHT:
                move_cursor(map, 1, 0);
                break;
        case SDLK_UP:
                move_cursor(map, 0, -1);
                break;
        case SDLK_DOWN:
                move_cursor(map, 0, 1);
                break;
        case SDLK_q:
                *quit = 1;
                break;
        case SDLK_t:
                *transparency = !(*transparency);
                break;
        case SDLK_PERIOD:
                camera_rotation = (camera_rotation + 1) % N_ROTATIONS;
                break;
        case SDLK_COMMA:
                camera_rotation = (camera_rotation - 1) % N_ROTATIONS;
                break;
        default:
                break;
        }
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

        /* Setup the models */
        for (size_t i = 0; i < N_MODELS; i++) {
                model_setup(&models[i], textures, texture_indices[i]);
        }

        if (!
            (maps[MAP_FLOOR1] =
             get_map_surface(args.filename ? args.filename : "map.png"))) {
                goto destroy_textures;
        }

        if (args.filename_l2) {
                if (!(maps[MAP_FLOOR2] = get_map_surface(args.filename_l2))) {
                        goto destroy_maps;
                }
                if ((map_w(maps[MAP_FLOOR1]) != map_w(maps[MAP_FLOOR2])) ||
                    (map_h(maps[MAP_FLOOR1]) != map_h(maps[MAP_FLOOR2]))) {
                        printf("Maps must be same size!\n");
                        goto destroy_maps;
                }
        }

        /* Setup the fov map. */
        fov_map.w = map_w(maps[MAP_FLOOR1]);
        fov_map.h = map_h(maps[MAP_FLOOR2]);
        fov_map.opq = calloc(1, fov_map.w * fov_map.h);
        fov_map.vis = calloc(1, fov_map.w * fov_map.h);
        if (args.fov) {
                setup_fov(maps[MAP_FLOOR1]);
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
                                           &args.transparency,
                                           maps[MAP_FLOOR1]);
                                break;
                        case SDL_WINDOWEVENT:
                                frames++;
                                render(renderer, textures, args.transparency);
                                break;
                        case SDL_MOUSEBUTTONDOWN:
                                on_mouse_button(&event.button);
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
destroy_maps:
        for (int i = 0; i < N_MAPS; i++) {
                if (maps[i]) {
                        SDL_FreeSurface(maps[i]);
                }
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
