#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <getopt.h>
#include <stdio.h>


struct args {
        char *filename;
        char *cmd;
};


/**
 * Handle key presses.
 */
void on_keydown(SDL_KeyboardEvent *event, int *quit)
{
        switch (event->keysym.sym) {
        case SDLK_LEFT:
                break;
        case SDLK_RIGHT:
                break;
        case SDLK_UP:
                break;
        case SDLK_DOWN:
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
               "Options: \n"
               "    -h:	help\n"
               "    -i: image filename\n"
               "Commands:\n"
               "    info    Show graphics info\n"
                );
}


/**
 * Parse command-line args.
 */
static void parse_args(int argc, char **argv, struct args *args)
{
        int c = 0;

        memset(args, 0, sizeof(*args));

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

        /* Any remaining option is assumed to be the save-file to load the game
         * from. If there is none then abort. */
        if (optind < argc) {
                args->cmd = argv[optind];
        }
}

/**
 * Print renderer info to stdout.
 */
static void show_renderer_info(SDL_RendererInfo *info)
{
        Uint32 tfi;

        printf("name: %s\n", info->name);
        printf("\tflags: 0x%x - ", info->flags);
        if (info->flags & SDL_RENDERER_SOFTWARE) {
                printf("software ");
        }
        if (info->flags & SDL_RENDERER_ACCELERATED) {
                printf("accelerated ");
        }
        if (info->flags & SDL_RENDERER_PRESENTVSYNC) {
                printf("presentvsync ");
        }
        if (info->flags & SDL_RENDERER_TARGETTEXTURE) {
                printf("targettexture ");
        }
        printf("\n");
        printf("\tnum_texture_formats: %d\n", info->num_texture_formats);
        for (tfi = 0; tfi < info->num_texture_formats; tfi++) {
                Uint32 tf = info->texture_formats[tfi];
                printf("\tTexture format %d:\n", tfi);
                printf("\t\tname: %s\n", SDL_GetPixelFormatName(tf));
                printf("\t\ttype: %d\n", SDL_PIXELTYPE(tf));
                printf("\t\torder: %d\n", SDL_PIXELORDER(tf));
                printf("\t\tlayout: %d\n", SDL_PIXELLAYOUT(tf));
                printf("\t\tbitsperpixel: %d\n", SDL_BITSPERPIXEL(tf));
                printf("\t\tbytesperpixel: %d\n",
                       SDL_BYTESPERPIXEL(tf));
                printf("\t\tindexed: %c\n",
                       SDL_ISPIXELFORMAT_INDEXED(tf) ? 'y': 'n');
                printf("\t\talpha: %c\n",
                       SDL_ISPIXELFORMAT_ALPHA(tf) ? 'y': 'n');
                printf("\t\tfourcc: %c\n",
                       SDL_ISPIXELFORMAT_FOURCC(tf) ? 'y': 'n');
        }
        printf("\tmax_texture_width: %d\n", info->max_texture_width);
        printf("\tmax_texture_height: %d\n", info->max_texture_height);

}

/**
 * Print available driver info to stdout.
 */
static void show_driver_info(void)
{
        int i, n;

        n = SDL_GetNumRenderDrivers();
        printf("%d renderer drivers\n", n);
        for (i = 0; i < n; i++) {
                SDL_RendererInfo info;

                if (SDL_GetRenderDriverInfo(i, &info)) {
                        printf("SDL_GetRenderDriverInfo(%d): %s\n",
                               i, SDL_GetError());
                        exit(-1);
                }

                show_renderer_info(&info);
        }
}

#define TILE_WIDTH 64
#define TILE_HEIGHT 32
#define TILE_WIDTH_HALF (TILE_WIDTH / 2)
#define TILE_HEIGHT_HALF (TILE_HEIGHT / 2)

static void clear_screen(SDL_Renderer *renderer)
{
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
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

static void render_iso_test(SDL_Renderer *renderer, SDL_Texture *texture,
                            int off_x, int off_y, int map_w, int map_h)
{
        int row, col, map_x;
        SDL_Rect src, dst;

        src.x = 0;
        src.y = 32;
        src.w = TILE_WIDTH;
        src.h = TILE_HEIGHT;

        dst.w = TILE_WIDTH;
        dst.h = TILE_HEIGHT;

        map_x = screen_x(map_h - 1, 0);

        for (row = 0; row < map_h; row++) {
                for (col = 0; col < map_w; col++) {
                        dst.x = screen_x(col, row) + map_x;
                        dst.y = screen_y(col, row);
                        SDL_RenderCopy(renderer, texture, &src, &dst);
                }
        }
}

static void render(SDL_Renderer *renderer, SDL_Texture *texture)
{
        clear_screen(renderer);
        render_iso_test(renderer, texture, 0, 0, 10, 10);
        SDL_RenderPresent(renderer);
}

static SDL_Texture *load_texture(SDL_Renderer *renderer, const char *filename)
{
        SDL_Surface *surface = NULL;
        SDL_Texture *texture = NULL;

        if (! (surface = IMG_Load(filename))) {
                printf("%s:IMG_Load:%s\n", __FUNCTION__, SDL_GetError());
                return NULL;
        }

        if (! (texture = SDL_CreateTextureFromSurface(renderer, surface))) {
                printf("%s:SDL_CreateTextureFromSurface:%s\n",
                       __FUNCTION__, SDL_GetError());
        }

        SDL_FreeSurface(surface);

        return texture;
}

int main(int argc, char **argv)
{
        SDL_Event event;
        SDL_Window *window=NULL;
        SDL_Renderer *renderer=NULL;
        SDL_Texture *texture=NULL;
        int done=0;
        Uint32 start_ticks, end_ticks, frames=0;
        struct args args;


        parse_args(argc, argv, &args);

        if (args.cmd) {
                if (! strcmp(args.cmd, "info")) {
                        show_driver_info();
                }
        }

        /* Init SDL */
        if (SDL_Init(SDL_INIT_VIDEO)) {
                printf("SDL_Init: %s\n", SDL_GetError());
                return -1;
        }

        /* Cleanup SDL on exit. */
        atexit(SDL_Quit);

        /* Create the main window */
        if (! (window = SDL_CreateWindow(
                       "Demo", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 640, 480,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN))) {
                printf("SDL_CreateWindow: %s\n", SDL_GetError());
                return -1;
        }

        /* Create the renderer. */
        if (! (renderer = SDL_CreateRenderer(window, -1, 0))) {
                printf("SDL_CreateRenderer: %s\n", SDL_GetError());
                goto destroy_window;
        }

        SDL_RendererInfo info;
        SDL_GetRendererInfo(renderer, &info);
        show_renderer_info(&info);

        /* Load the texture image */
        if (! (texture = load_texture(
                       renderer,
                       "/home/gmcnutt/Dropbox/projects/art/iso-64x64-outside.png"))) {
                goto destroy_renderer;
        }

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
                                render(renderer, texture);
                                break;
                        default:
                                break;
                        }
                }

                render(renderer, texture);
        }

        end_ticks = SDL_GetTicks();
        printf("Frames: %d\n", frames);
        if (end_ticks > start_ticks) {
                printf("%2.2f FPS\n",
                       ((double)frames * 1000) / (end_ticks - start_ticks)
                        );
        }

        SDL_DestroyTexture(texture);
destroy_renderer:
        SDL_DestroyRenderer(renderer);
destroy_window:
        SDL_DestroyWindow(window);

        return 0;
}
