#include <signal.h>
#include <stdio.h>

#include "my_struct.h"
#include "slog/slog.h"
#include "tools/log.h"
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
    f->keep_last = !!keep_last;
    for (int i = 0; i < f->max_size; ++i) {
        if (!(f->queue[i].frame = av_frame_alloc()))
            return AVERROR(ENOMEM);
    }
    return 0;
}

static void frame_queue_destory(FrameQueue* f) {
    // TODO(wangqing): issue
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

static void packet_queue_destory(PacketQueue* q) {
    // TODO(wangqing): issue
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

static void set_clock_speed(Clock* c, double speed) {
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

static void init_clock(Clock* c, int* queue_serial) {
    c->speed        = 1.0;
    c->paused       = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

static int decode_interrupt_cb(void* ctx) {
    MainState* is = (MainState*)ctx;
    return is->abort_request;
}

extern AVDictionary* format_opts;
static int read_thread(void* arg) {
    // TODO(wangqing): 功能未实现
    MainState*       is = ( MainState* )arg;
    int              ret, err, i;
    int              st_index[AVMEDIA_TYPE_NB];

    int64_t    stream_start_time;  // TODO(wangqing): issue
    int        pkt_in_play_range = 0;  // TODO(wangqing): :wissue

    SDL_mutex* wait_mutex = SDL_CreateMutex();
    if (!wait_mutex){
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex():%s\n", SDL_GetError());
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    memset(st_index, -1, sizeof(st_index));
    is->eof = 0;

    AVPacket* pkt = av_packet_alloc();
    if (!pkt){
        flog("Could not allocate packet\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    AVFormatContext* ic = avformat_alloc_context();
    if (!ic){
        flog("Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque   = is;
    if (!av_dict_get(format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
        av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        scan_all_pmts_set = 1;
    }

    err = avformat_open_input(&ic, is->filename, is->iformat, &format_opts);
    if (err < 0){
        print_error(is->filename, err);
        ret = -1;
        goto fail;
    }
    if (scan_all_pmts_set)
        av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

    AVDictionaryEntry* t;
    if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))){
        elog("Option %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }
    is->ic = ic;

    if (genpts)  // TODO(wangqing): issue
        ic->flags |= AVFMT_FLAG_GENPTS;

     // TODO(wangqing): 代码有省略

    if (find_stream_info){

    }

fail:
    if (ic && !is->ic)
        avformat_close_input(&ic);

    av_packet_free(&pkt);
    if (ret != 0) {
        SDL_Event event;
        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    SDL_DestroyMutex(wait_mutex);
    return 0;
}

static void stream_component_close(MainState* is, int stream_index) {
    // TODO(wangqing): issue
}

static void stream_close(MainState* is) {
    is->abort_request = 1;
    SDL_WaitThread(is->read_tid, NULL);

    // close each stream
    if (is->audio_stream >= 0)
        stream_component_close(is, is->audio_stream);
    if (is->video_stream >= 0)
        stream_component_close(is, is->video_stream);
    if (is->subtitle_stream >= 0)
        stream_component_close(is, is->subtitle_stream);

    avformat_close_input(&is->ic);

    packet_queue_destory(&is->videoq);
    packet_queue_destory(&is->audioq);
    packet_queue_destory(&is->subtitleq);

    frame_queue_destory(&is->pictq);
    frame_queue_destory(&is->sampq);
    frame_queue_destory(&is->subpq);

    SDL_DestroyCond(is->continue_read_thread);
    sws_freeContext(is->img_convert_ctx);
    sws_freeContext(is->sub_convert_ctx);
    av_free(is->filename);

    if (is->vis_texture)
        SDL_DestroyTexture(is->vis_texture);
    if (is->vid_texture)
        SDL_DestroyTexture(is->vid_texture);
    if (is->sub_texture)
        SDL_DestroyTexture(is->sub_texture);
    av_free(is);
}

static MainState* stream_open(const char*          filename,
                              const AVInputFormat* iformat) {
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

    if (packet_queue_init(&is->videoq) < 0 || packet_queue_init(&is->audioq) < 0
        || packet_queue_init(&is->subtitleq) < 0)
        goto fail;

    if (!(is->continue_read_thread = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond():%s\n", SDL_GetError());
        goto fail;
    }

    // 初始化各个Stream的时钟
    init_clock(&is->vidclk, &is->videoq.serial);
    init_clock(&is->audclk, &is->audioq.serial);
    init_clock(&is->extclk, &is->extclk.serial);

    // TODO(wangqing): 有什么用途
    is->audio_clock_serial = -1;

    // 设置初始音量
    if (startup_volume < 0)
        av_log(NULL,
               AV_LOG_WARNING,
               "-volume=%s < 0, setting to 0\n",
               startup_volume);
    if (startup_volume > 100)
        av_log(NULL,
               AV_LOG_WARNING,
               "-volume=%s > 100, setting to 100\n",
               startup_volume);
    startup_volume = av_clip(startup_volume, 0, 100);
    startup_volume =
        av_clip(SDL_MIX_MAXVOLUME * startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
    is->audio_volume = startup_volume;

    is->muted        = 0;
    is->av_sync_type = av_sync_type;

    is->read_tid = SDL_CreateThread(read_thread, "read_thread", is);
    if (!is->read_tid) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateThread():%s\n", SDL_GetError());
    fail:
        stream_close(is);
        return NULL;
    }
    return is;
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
