#include <signal.h>
#include <stdio.h>

#include "my_struct.h"
#include "slog/slog.h"
#include "tools/cmdutils.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "SDL.h"

#ifdef __cplusplus
}
#endif  //__cplusplus

const char program_name[]     = "WPlayer";
const int  program_birth_year = 2000;
const char cc_ident[]         = "gcc 9.1.1 (GCC) 20190807";

extern const OptionDef options[];

static void sigterm_handler(int sig) {
    // todo-start/////////////////////////////////////
    // author: wangqing deadline: 2021/01/01
    // todo-end//////////////////////////////////////////////
    exit(123);
}

static void opt_input_file(void* optctx, const char* filename) {
    if (input_filename) {
        av_log(NULL,
               AV_LOG_FATAL,
               "Argument '%s' provided as input filename, but '%s' was already "
               "specified.\n",
               filename,
               input_filename);
        exit(1);
    }

    if (!strcmp(filename, "-"))
        filename = "pipe:";
    input_filename = filename;
}

static void show_usage() {
    av_log(NULL, AV_LOG_INFO, "Simple media player\n");
    av_log(NULL, AV_LOG_INFO, "usage: %s [options] input_file\n", program_name);
    av_log(NULL, AV_LOG_INFO, "\n");
}

static void do_exit(MainState* is) {
    // todo-start/////////////////////////////////////
    // author: :wwangqing deadline: 2021/01/01
    // todo-end//////////////////////////////////////////////
}

static MainState* stream_open(const char*          filename,
                              const AVInputFormat* iformat) {
    // todo-start/////////////////////////////////////
    // author: wangqing deadline: 2021/01/01
    // todo-end//////////////////////////////////////////////
}

static void event_loop(MainState* cur_stream) {
    // todo-start/////////////////////////////////////
    // author: wangqing deadline: 2021/01/01
    // todo-end//////////////////////////////////////////////
}

static SDL_Window*   window;
static SDL_Renderer* renderer;
static SDL_RendererInfo renderer_info = {0};
static SDL_AudioDeviceID audio_dev;

int main(int argc, char const *argv[])
{
    init_logger("log", _slog_level::S_DEBUG);

    init_dynload();

    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    parse_loglevel(argc, argv, options);

#if CONFIG_AVDEVICE
    avdevice_register_all();
#else
    avformat_network_init();
#endif

    init_opts();

    signal(SIGINT, sigterm_handler);
    signal(SIGTERM, sigterm_handler);

    show_banner(argc, argv, options);

    parse_options(NULL, argc, argv, options, opt_input_file);

    if (!input_filename) {
        show_usage();
        av_log(NULL, AV_LOG_FATAL, "An input file must be specified\n");
        av_log(NULL,
               AV_LOG_FATAL,
               "Use -h to get full help or, even better, run 'man %s'\n",
               program_name);
        exit(1);
    }

    if (display_disable)
        video_disable = 1;

    int flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (audio_disable) {
        flags &= ~SDL_INIT_AUDIO;
    } else {
        if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE"))
            SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE", "1", 1);
    }

    if (display_disable)
        flags &= ~SDL_INIT_VIDEO;
    if (SDL_Init(flags)) {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not initialize SDL - %s\n",
               SDL_GetError());
        av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
        exit(1);
    }

    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    if (!display_disable) {
        int flags = SDL_WINDOW_HIDDEN;
        if (alwaysontop) {
#if SDL_VERSION_ATLEAST(2, 0, 5)
            flags |= SDL_WINDOW_ALWAYS_ON_TOP;
#else
            av_log(NULL,
                   AV_LOG_WARNING,
                   "Your SDL version doesn't support SDL_WINDOW_ALWAYS_ON_TOP. "
                   "Feature will be inactive.\n");
#endif
        }

        if (borderless) {
            flags |= SDL_WINDOW_BORDERLESS;
        } else {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        window = SDL_CreateWindow(program_name,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  default_width,
                                  default_height,
                                  flags); 
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        if (window) {
            renderer = SDL_CreateRenderer(window,
                                          -1,
                                          SDL_RENDERER_ACCELERATED
                                              | SDL_RENDERER_PRESENTVSYNC);
            if (!renderer) {
                av_log(NULL, AV_LOG_WARNING, "Failed to initilize a hardware accelerated renderer:%s\n", SDL_GetError());
                renderer = SDL_CreateRenderer(window, -1, 0);
            }
            if (renderer) {
                if (!SDL_GetRendererInfo(renderer, &renderer_info))
                    av_log(NULL, AV_LOG_VERBOSE, "Initialized %s renderer.", renderer_info.name);
            }
        }

        if (!window || !renderer || !renderer_info.num_texture_formats) {
            av_log(NULL, AV_LOG_FATAL, "Failed to create window or renderer:%s", SDL_GetError());
            do_exit(NULL);
        }
    }
    MainState* is = stream_open(input_filename, file_iformat);
    if (!is) {
        av_log(NULL, AV_LOG_FATAL, "Failed to initialize MainState!\n");
        do_exit(NULL);
    }

    event_loop(is);

    return 0;
}


