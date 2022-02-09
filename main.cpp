#include <signal.h>
#include <stdio.h>

#include "my_struct.h"
#include "slog/slog.h"
#include "tools/cmdutils.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "SDL.h"
#include "libavutil/time.h"

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
    if (is)
        stream_close(is);

    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    uninit_opts();
#if CONFIG_AVFILTER
    av_freep(&vfilters_list);
#endif
    avformat_network_deinit();

    if (show_status)
        printf("\n");
    SDL_Quit();
    av_log(NULL, AV_LOG_QUIET, "%s", "");
    exit(0);
}

static int frame_queue_init(FrameQueue*  f,
                            PacketQueue* pktq,
                            int          max_size,
                            int          keep_last) {
    memset(f, 0, sizeof(FrameQueue));
    if (!(f->mutex = SDL_CreateMutex())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex():%s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    if (!(f->cond = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond():%s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    f->pktq     = pktq;
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    // TODO(wangqing): 为什么使用两次取反
    f->keep_last = !!keep_last;
    for (int i = 0; i < f->max_size; ++i) {
        if (!(f->queue[i].frame = av_frame_alloc()))
            return AVERROR(ENOMEM);
    }
    return 0;
}

static int packet_queue_init(PacketQueue* q) {
    memset(q, 0, sizeof(PacketQueue));
    q->pkt_list = av_fifo_alloc(sizeof(MyAVPacket));
    if (!q->pkt_list)
        return AVERROR(ENOMEM);
    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex():%s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->cond = SDL_CreateCond();
    if (!q->cond) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond():%s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->abort_request = 1;
    return 0;
}

static double get_clock(Clock* c) {
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->pts;
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time
               - (time - c->last_updates) * (1.0 - c->speed);
    }
}

static void set_clock_at(Clock* c, double pts, int serial, double time) {
    c->pts          = pts;
    c->last_updates = time;
    c->pts_drift    = c->pts - time;
    c->serial       = serial;
}

static void set_clock(Clock* c, double pts, int serial) {
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

static void set_clock_speed(Clock* c, double speed)
{
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

static void init_clock(Clock* c, int* queue_serial) {
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

static void stream_close(MainState* is) {
    // todo-start/////////////////////////////////////
    // author: wangqing deadline: 2021/01/01
    // todo-end//////////////////////////////////////////////
}

static MainState* stream_open(const char*          filename,
                              const AVInputFormat* iformat) {
    // todo-start/////////////////////////////////////
    // author: wangqing deadline: 2021/01/01
    // todo-end//////////////////////////////////////////////
    MainState* is;
    is = ( MainState* )av_mallocz(sizeof(MainState));
    if (!is)
        return NULL;

    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;
    is->last_subtitle_stream = is->subtitle_stream = -1;
    is->filename                                   = av_strdup(filename);
    if (!is->filename)
        goto fail;
    is->iformat = iformat;
    is->ytop    = 0;
    is->xleft   = 0;

    // start video display
    if (frame_queue_init(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1)
        < 0)
        goto fail;
    if (frame_queue_init(&is->subpq, &is->subtitleq, SUBPICTURE_QUEUE_SIZE, 0)
        < 0)
        goto fail;
    if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
        goto fail;

    if (packet_queue_init(&is->videoq) < 0 || 
        packet_queue_init(&is->audioq) < 0 || 
        packet_queue_init(&is->subtitleq) < 0)
        goto fail;

    if (!(is->continue_read_thread = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond():%s\n", SDL_GetError());
        goto fail;
    }

    init_clock(&is->vidclk, &is->videoq.serial);
    init_clock(&is->audclk, &is->audioq.serial);
    init_clock(&is->extclk, &is->extclk.serial);


fail:
    stream_close(is);
    return NULL;
}

static void event_loop(MainState* cur_stream) {
    // todo-start/////////////////////////////////////
    // author: wangqing deadline: 2021/01/01
    // todo-end//////////////////////////////////////////////
}

static SDL_Window*       window;
static SDL_Renderer*     renderer;
static SDL_RendererInfo  renderer_info = { 0 };
static SDL_AudioDeviceID audio_dev;

int main(int argc, char const* argv[]) {
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
                av_log(
                    NULL,
                    AV_LOG_WARNING,
                    "Failed to initilize a hardware accelerated renderer:%s\n",
                    SDL_GetError());
                renderer = SDL_CreateRenderer(window, -1, 0);
            }
            if (renderer) {
                if (!SDL_GetRendererInfo(renderer, &renderer_info))
                    av_log(NULL,
                           AV_LOG_VERBOSE,
                           "Initialized %s renderer.",
                           renderer_info.name);
            }
        }

        if (!window || !renderer || !renderer_info.num_texture_formats) {
            av_log(NULL,
                   AV_LOG_FATAL,
                   "Failed to create window or renderer:%s",
                   SDL_GetError());
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
