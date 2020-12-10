#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <getopt.h>
#include <stdio.h>

#include <gcu.h>

#include "fov.h"
#include "iso.h"
#include "map.h"
#include "model.h"
#include "point.h"
#include "view.h"

enum {
        MODEL_RENDER_FLAG_TRANSPARENT = 1,
        MODEL_RENDER_FLAG_SKIPLEFT = 2,
        MODEL_RENDER_FLAG_SKIPRIGHT = 4,
        MODEL_RENDER_FLAG_TOPJUSTIFY = 8
};

enum {
        TEXTURE_GRASS,
        TEXTURE_5x1x1LEFT,
        TEXTURE_5x1x1RIGHT,
        TEXTURE_TOP,
        TEXTURE_LEFTSHORT,
        TEXTURE_RIGHTSHORT,
        TEXTURE_INTERIOR,
        TEXTURE_1x1x1LEFT,
        TEXTURE_1x1x1RIGHT,
        TEXTURE_2x1x1LEFT,
        TEXTURE_2x1x1RIGHT,
        TEXTURE_3x1x1LEFT,
        TEXTURE_3x1x1RIGHT,
        TEXTURE_4x1x1LEFT,
        TEXTURE_4x1x1RIGHT,
        N_TEXTURES
};

enum {
        MODEL_SHORT,
        MODEL_1x1x1,
        MODEL_2x1x1,
        MODEL_3x1x1,
        MODEL_4x1x1,
        MODEL_5x1x1,
        MODEL_INTERIOR,
        N_MODELS
};

enum {
        DIR_XLEFT,
        DIR_XRIGHT,
        DIR_YUP,
        DIR_YDOWN,
        DIR_ZUP,
        DIR_ZDOWN,
        N_DIR
};

struct args {
        char *filenames[N_MAPS];
        char *cmd;
        bool fov;
        bool delay;
        bool transparency;
};

typedef struct {
        view_t view;
        area_t area;
        bool transparency;
} session_t;

#define FPS 60
#define MODEL_H2I(h) clamp((h), 0, N_MODELS - 1)
#define MODEL_I2H(i) (i)
#define TILE_HEIGHT 18
#define TILE_HEIGHT_HALF (TILE_HEIGHT / 2)
#define TILE_WIDTH 36
#define TILE_WIDTH_HALF (TILE_WIDTH / 2)
#define TICK_PER_FRAME (1000 / FPS)


#define between(x, l, r) (((l) < (x)) && (x) < (r))
#define between_inc(x, l, r) (((l) <= (x)) && (x) <= (r))
#define clamp(x, a, b) ((x) < (a) ? (x) : ((x) > (b) ? (b) : (x)))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

static const char *texture_files[] = {
        "grass.png",
        "5x1x1-left.png",
        "5x1x1-right.png",
        "1x1-top.png",
        "short_gray_left.png",
        "short_gray_right.png",
        "gray_interior.png",
        "1x1x1-left.png",
        "1x1x1-right.png",
        "2x1x1-left.png",
        "2x1x1-right.png",
        "3x1x1-left.png",
        "3x1x1-right.png",
        "4x1x1-left.png",
        "4x1x1-right.png",
};

static const size_t texture_indices[N_MODELS][N_MODEL_FACES] = {
        {TEXTURE_LEFTSHORT, TEXTURE_RIGHTSHORT, TEXTURE_TOP},
        {TEXTURE_1x1x1LEFT, TEXTURE_1x1x1RIGHT, TEXTURE_TOP},
        {TEXTURE_2x1x1LEFT, TEXTURE_2x1x1RIGHT, TEXTURE_TOP},
        {TEXTURE_3x1x1LEFT, TEXTURE_3x1x1RIGHT, TEXTURE_TOP},
        {TEXTURE_4x1x1LEFT, TEXTURE_4x1x1RIGHT, TEXTURE_TOP},
        {TEXTURE_5x1x1LEFT, TEXTURE_5x1x1RIGHT, TEXTURE_TOP},
        {TEXTURE_LEFTSHORT, TEXTURE_RIGHTSHORT, TEXTURE_INTERIOR}
};


static char rendered[VIEW_W * VIEW_H] = { 0 };
static model_t models[N_MODELS] = { 0 };

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
        printf("  -i: image filename (max %d)\n", N_MAPS);
        printf("  -t: enable transparency\n");
}

static void parse_filenames(struct args *args, char *filenames, int i)
{
        char *c = strchr(filenames, ',');
        if (!c) {
                return;
        }
        if (i >= N_MAPS) {
                printf("Only %d map files allowed!\n", N_MAPS);
                print_usage();
                exit(-1);
        }
        *c = '\0';
        c++;
        args->filenames[i] = c;
        parse_filenames(args, c, i + 1);
}

/**
 * Parse command-line args.
 */
static void parse_args(int argc, char **argv, struct args *args)
{
        int c = 0;

        /* Set defaults */
        memset(args, 0, sizeof (*args));
        args->delay = true;

        /* Get user args */
        while ((c = getopt(argc, argv, "i:hfdt")) != -1) {
                switch (c) {
                case 'd':
                        args->delay = false;
                        break;
                case 'f':
                        args->fov = !args->fov;
                        break;
                case 'i':
                        args->filenames[0] = optarg;
                        parse_filenames(args, optarg, 1);
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

static inline void map_to_camera(point_t left, point_t right, point_t cursor)
{
        left[X] = (right[X] - (right[Z] - cursor[Z])) - cursor[X];
        left[Y] = (right[Y] - (right[Z] - cursor[Z])) - cursor[Y];
}

static inline void map_to_view(point_t l, point_t r, point_t cursor)
{
        map_to_camera(l, r, cursor);
        l[X] += VIEW_W / 2;
        l[Y] += VIEW_H / 2;
}

static inline size_t map_xy_to_index(map_t * map, size_t map_x, size_t map_y)
{
        return map_y * map_w(map) + map_x;
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

static bool cutaway_at(point_t vloc)
{
        int margin = 1;
        int cam_x = view_to_camera_x(vloc[X]/* - vloc[Z]*/);
        int cam_y = view_to_camera_y(vloc[Y]/* - vloc[Z]*/);
        return (cam_x > -1 && cam_y > -1);
        return (between(cam_x, -margin, Z_PER_LEVEL + margin) &&
                between(cam_y, -margin, Z_PER_LEVEL + margin));
}

/**
 * The view coordinates overlay the map and follow the cursor around with an
 * origin at a fixed (x, y) distance from the cursor. The view z coordinate is
 * offset from the cursor z.
 */
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
                if (flags & MODEL_RENDER_FLAG_TOPJUSTIFY) {
                        dst.y = view_to_screen_y(view_x, view_y, view_z);
                        int max_offset = (Z_PER_LEVEL * TILE_HEIGHT);
                        dst.y -= max_offset;
                        int max_offset_y = (model->tile_h - 1) * TILE_HEIGHT;
                        int off_z = max(0, max_offset_y - offset->y);
                        dst.y += off_z;
                } else {
                        dst.y = view_to_screen_y(view_x, view_y, view_z) - offset->y;
                }
                Uint8 alpha =
                        (flags & MODEL_RENDER_FLAG_TRANSPARENT) ? 128 : 255;
                SDL_SetTextureAlphaMod(texture, alpha);
                SDL_SetTextureColorMod(texture, red, grn, blu);
                SDL_RenderCopy(renderer, texture, NULL, &dst);
        }
}


static bool render_level(SDL_Renderer * renderer, SDL_Texture ** textures,
                         session_t * session, area_t *area, int map_level)
{
        SDL_Rect src, dst;
        view_t *view = &session->view;
        int cursor_level = Z2L(view->cursor[Z]);
        int map_z = map_level * Z_PER_LEVEL;
        //int view_z = (map_level - cursor_level) * Z_PER_LEVEL;
        int view_z = (map_z - view->cursor[Z]);
        static const int cursor_h = Z_PER_LEVEL;
        int cursor_top_z = view->cursor[Z] + cursor_h;
        bool clipped_pillar = false;
        bool top_of_stairs = false;
        bool enable_cutaway = cursor_level == map_level || (cursor_level < map_level && cursor_top_z > map_z);
        map_t *map = area_get_map_at_level(area, map_level);

        src.x = 0;
        src.y = 0;
        src.w = TILE_WIDTH;
        src.h = TILE_HEIGHT;

        /* Render the map as a tiled view */
        for (int view_y = 0; view_y < VIEW_H; view_y++) {
                for (int view_x = 0; view_x < VIEW_W; view_x++) {
                        point_t vloc = { view_x, view_y, view_z };
                        point_t mloc = { 0, 0, 0 };
                        view_to_map(view, vloc, mloc);
                        mloc[Z] = map_z;
                        int map_y = mloc[Y];
                        int map_x = mloc[X];
                        Uint8 model_index = 0;

                        if (!(map_contains(map, map_x, map_y))) {
                                continue;
                        }

                        if (!view_in_fov(view, mloc)) {

                                dst.x = view_to_screen_x(view_x, view_y);
                                dst.y = view_to_screen_y(view_x, view_y, view_z);
                                dst.w = TILE_WIDTH;
                                dst.h = TILE_HEIGHT;
                                SDL_SetTextureColorMod(textures[TEXTURE_TOP], 0, 0, 16);
                                SDL_RenderCopy(renderer,
                                               textures[TEXTURE_TOP],
                                               &src, &dst);
                                continue;
                        }

                        /* Only render tiles on lower levels if they do not
                         * have a ceiling. This is what prevents us from
                         * rendering underground levels when walking on the
                         * surface. */
                        if (map_level < cursor_level) {
                                int lvl = map_level;
                                int skip = false;
                                while (lvl < cursor_level) {
                                        lvl++;
                                        map_t *map = area_get_map_at_level(area, lvl);
                                        if (map && map_get_pixel (map, map_x, map_y)) {
                                                skip = true;
                                                break;
                                        }
                                }
                                if (skip) {
                                        continue;
                                }
                        }

                        /* Draw the terrain */
                        pixel_t pixel = map_get_pixel(map, map_x, map_y);
                        if (!pixel) {
                                /* transparent, nothing there */
                                goto draw_cursor;
                        }
                        model_t *model = NULL;
                        int flags = 0;

                        switch (pixel) {
                        case PIXEL_VALUE_GRASS:

                                dst.x = view_to_screen_x(view_x, view_y);
                                dst.y = view_to_screen_y(view_x, view_y, view_z);
                                dst.w = TILE_WIDTH;
                                dst.h = TILE_HEIGHT;

                                if (session->transparency &&
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
                        default:
                                model_index = PIXEL_MODEL(pixel);
                                if (model_index >= N_MODELS) {
                                        printf
                                                ("Unknown pixel value: 0x%08x at (%d, %d, %d) model %d\n",
                                                 pixel, map_x, map_y,
                                                 view_z, model_index);
                                } else {
                                        if (PIXEL_IS_FLUID(pixel)) {
                                                flags |= MODEL_RENDER_FLAG_TOPJUSTIFY;
                                                model_index = (MODEL_5x1x1 + 1) - model_index;
                                        }

                                        model = &models[model_index];
                                        if (session->transparency &&
                                            blocks_fov(view_x, view_y,
                                                       model->tile_h)) {
                                                flags |=
                                                        MODEL_RENDER_FLAG_TRANSPARENT;
                                        }


                                        if (PIXEL_IS_OPAQUE(pixel) &&
                                            !PIXEL_IS_STAIRS(pixel)) {

                                                /* Cut away walls if they are on the same level as the cursor. */
                                                bool cutaway = enable_cutaway && cutaway_at(vloc);

                                                if (cutaway) {

                                                        /* Use truncated model. */
                                                        model = &models [MODEL_INTERIOR];

                                                        /* Check for a ceiling on a clipped wall. */
                                                        if (!clipped_pillar && map_level == cursor_level) {
                                                                map_t *map = area_get_map_at_level(area, map_level + 1);
                                                                if (map && map_get_pixel (map, map_x, map_y)) {
                                                                        clipped_pillar = true;
                                                                }
                                                        }
                                                }
                                        }

                                        model_render(renderer, model,
                                                     view_x, view_y,
                                                     view_z,
                                                     PIXEL_RED(pixel),
                                                     PIXEL_GREEN(pixel),
                                                     PIXEL_BLUE(pixel),
                                                     flags);
                                }
                                break;
                        }
                        view_set_rendered(view_x, view_y,
                                          model ? model->tile_h : 1);

                draw_cursor:
                        /* Draw the cursor if this is where it is. */
                        if (cursor_top_z > map_z &&
                            map_level >= cursor_level &&
                            map_x == view->cursor[X] &&
                            map_y == view->cursor[Y]) {
                                int off_z = 0;

                                /* Model is sticking up from map below, shorten it. */
                                if (view->cursor[Z] < map_z) {
                                        model = &models[MODEL_H2I(cursor_top_z - map_z)];
                                } else {
                                        model = &models[MODEL_5x1x1];
                                        off_z = view->cursor[Z] - map_z;
                                }

                                /* If cursor is on the stairs offset it up. Note
                                 * if it is on top of the stairs, we'll show
                                 * the next level in that case. */
                                if (PIXEL_IS_STAIRS(pixel)) {
                                        if (PIXEL_HEIGHT(pixel) > 1) {
                                                top_of_stairs = true;
                                        }
                                }
                                model_render(renderer,
                                             model,
                                             view_x, view_y,
                                             view_z + off_z, 255, 128,
                                             64, 0);
                        }
                }
        }

        return !clipped_pillar || top_of_stairs;
}

static void render(SDL_Renderer * renderer, SDL_Texture ** textures,
                   session_t * session)
{
        clear_screen(renderer);

        view_t *view = &session->view;
        int cursor_level = Z2L(view->cursor[Z]);

        /* Clear the rendered buffer */
        view_clear_rendered();

        /* Recompute fov based on player's position */
        view_calc_fov(view);

        /* Render the maps in z order */
        for (int level = 0; level < session->area.n_maps; level++) {
                map_t *map = session->area.maps[level];

                /* But if the cursor is now directly underneath a tile on a
                 * higher level, stop rendering higher levels. This implements
                 * roof clipping. What if the cursor is standing under a hole?
                 * The roof won't get clipped, I think, when it probably
                 * should. */
                if ((level > cursor_level) &&
                    map_get_pixel(map, view->cursor[X], view->cursor[Y])) {
                        break;
                }

                /* Or if the rendering says to stop, then clip the higher
                 * levels. */
                if (!render_level(renderer, textures, session, &session->area, level)) {
                        break;
                }

        }

        /* Paint the grid */
        SDL_SetRenderDrawColor(renderer, 0, 64, 64, 128);
        iso_grid(renderer, VIEW_W, VIEW_H);

        /* Paint a red square for a cursor position */
        point_t vloc;
        map_to_view(vloc, view->cursor, view->cursor);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
        iso_square(renderer, VIEW_H, vloc[X], vloc[Y]);

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
void on_mouse_button(SDL_MouseButtonEvent * event, session_t * session)
{
        view_t *view = &session->view;
        point_t vloc = { 0, 0, 0 };
        vloc[X] = screen_to_view_x(event->x, event->y);
        vloc[Y] = screen_to_view_y(event->x, event->y);
        int cam_x = view_to_camera_x(vloc[X]);
        int cam_y = view_to_camera_y(vloc[Y]);
        point_t mloc = { 0, 0, 0 };
        view_to_map(view, vloc, mloc);
        bool cutaway = cutaway_at(vloc);

        printf("s(%d, %d)->v(%d, %d)->c(%d, %d)->m(%d, %d)->%c %c\n",
               event->x, event->y,
               vloc[X], vloc[Y],
               cam_x, cam_y,
               mloc[X], mloc[Y],
               view_rendered_at(vloc[X], vloc[Y]) ? 't' : 'f',
               cutaway ? 't' : 'f');
}

bool move_cursor(area_t *area, view_t * view, const point_t dir)
{
        point_t newcur, rdir;
        map_t *map;

        /* Figure out where the new point is, at least horizontally. */
        point_copy(newcur, view->cursor);
        point_copy(rdir, dir);
        rdir[Z] = L2Z(dir[Z]);  /* dir z is # levels */
        point_rotate(rdir, view->rotation);
        point_translate(newcur, rdir);

        /* While there is a map at this level that contains the (x, y)
         * coordinates...  */
        while ((map = area_get_map_at_level(area, Z2L(newcur[Z]))) &&
               map_contains(map, newcur[X], newcur[Y])) {

                /* Find the terrain there. */
                pixel_t pix = map_get_pixel(map, newcur[X], newcur[Y]);
                if (! pix) {
                        /* If there's a hole there, try the next level down... */
                        newcur[Z] -= Z_PER_LEVEL;
                } else {
                        /* Else if it's impassable... */
                        if (PIXEL_IS_IMPASSABLE(pix) || PIXEL_IS_FLUID(pix)) {

                                /* Else we'll take it. */
                                int lvl_z = L2Z(Z2L(newcur[Z]));
                                int new_z =  (PIXEL_HEIGHT(pix) + lvl_z);
                                if ((new_z - view->cursor[Z]) > 1) {
                                        return false; /* Can't climb more than 1 step at a time */
                                }
                                if (!(map = area_get_map_at_level(area, Z2L(new_z))) ||
                                    !map_contains(map, newcur[X], newcur[Y])) {
                                        return false;
                                }
                                point_copy(view->cursor, newcur);
                                view->cursor[Z] = new_z;
                                return true;
                        }
                        int new_z = L2Z(Z2L(newcur[Z]));
                        point_copy(view->cursor, newcur);
                        view->cursor[Z] = new_z;
                        return true;
                }
        }
        return false;
}


/**
 * Handle key presses.
 */
void on_keydown(SDL_KeyboardEvent * event, int *quit, session_t * session)
{
        area_t *area = &session->area;
        view_t *view = &session->view;
        static const point_t directions[N_DIR] = {
                {-1, 0, 0},     /* left */
                {1, 0, 0},      /* right */
                {0, -1, 0},     /* up */
                {0, 1, 0},      /* down */
                {0, 0, 1},      /* vert up */
                {0, 0, -1}      /* vert down */
        };

        switch (event->keysym.sym) {
        case SDLK_LEFT:
                move_cursor(area, view, directions[DIR_XLEFT]);
                break;
        case SDLK_RIGHT:
                move_cursor(area, view, directions[DIR_XRIGHT]);
                break;
        case SDLK_UP:
                move_cursor(area, view, directions[DIR_YUP]);
                break;
        case SDLK_DOWN:
                move_cursor(area, view, directions[DIR_YDOWN]);
                break;
        case SDLK_PAGEUP:
                move_cursor(area, view, directions[DIR_ZUP]);
                break;
        case SDLK_PAGEDOWN:
                move_cursor(area, view, directions[DIR_ZDOWN]);
                break;
        case SDLK_q:
                *quit = 1;
                break;
        case SDLK_t:
                session->transparency = !(session->transparency);
                break;
        case SDLK_PERIOD:
                view->rotation = (view->rotation + 1) % N_ROTATIONS;
                break;
        case SDLK_COMMA:
                view->rotation = (view->rotation - 1) % N_ROTATIONS;
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
        session_t session;

        int done = 0;
        Uint32 start_ticks, end_ticks, frames = 0, pre_tick;
        double total_delay = 0, total_used = 0;
        struct args args;

        memset(&session, 0, sizeof (session));

        parse_args(argc, argv, &args);

        session.transparency = args.transparency;

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
                model_init(&models[i], textures, texture_indices[i],
                           TILE_HEIGHT);
        }

        map_t *map;
        if (!
            (map =
             map_from_image(args.filenames[0] ? args.
                            filenames[0] : "map.png"))) {
                goto destroy_textures;
        }

        area_add(&session.area, map);

        for (int i = 1; args.filenames[i]; i++) {

                if (!(map = map_from_image(args.filenames[i]))) {
                        goto destroy_maps;
                }
                if ((map_w(map) != area_w(&session.area)) ||
                    (map_h(map) != area_h(&session.area))) {
                        printf("Maps must be same size!\n");
                        goto destroy_maps;
                }
                area_add(&session.area, map);
        }

        view_init(&session.view, &session.area, args.fov);
        session.view.cursor[Z] = Z_PER_LEVEL * MAP_FLOOR1;

        start_ticks = SDL_GetTicks();
        pre_tick = SDL_GetTicks();

        while (!done) {
                while (SDL_PollEvent(&event)) {
                        switch (event.type) {
                        case SDL_QUIT:
                                done = 1;
                                break;
                        case SDL_KEYDOWN:
                                on_keydown(&event.key, &done, &session);
                                break;
                        case SDL_WINDOWEVENT:
                                frames++;
                                render(renderer, textures, &session);
                                break;
                        case SDL_MOUSEBUTTONDOWN:
                                on_mouse_button(&event.button, &session);
                        default:
                                break;
                        }
                }

                render(renderer, textures, &session);

                frames++;
                Uint32 post_tick = SDL_GetTicks();
                Uint32 used = post_tick - pre_tick;
                pre_tick = post_tick;
                total_used += used;

                if (args.delay) {
                        int delay = TICK_PER_FRAME - used;
                        if (delay > 0) {
                                SDL_Delay(delay);
                                total_delay += delay;
                        }
                }
        }

        end_ticks = SDL_GetTicks();
        printf("Frames: %d\n", frames);
        if (end_ticks > start_ticks) {
                printf("%2.2f FPS\n",
                       ((double)frames * 1000) / (end_ticks - start_ticks)
                        );
                printf("%f msecs avg loop times\n", (total_used / frames));
                if (args.delay) {
                        printf("%f msecs avg delay\n", (total_delay / frames));
                }
        }
destroy_maps:
        for (int i = 0; i < N_MAPS; i++) {
                if (session.area.maps[i]) {
                        map_free(session.area.maps[i]);
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
