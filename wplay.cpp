#include <signal.h>
#include <stdio.h>

#include "my_struct.h"
#include "slog/slog.h"
#include "tools/cmdutils.h"
#include "tools/log.h"

#include "src/packet.h"
#include "src/frame.h"
#include "src/decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "SDL.h"
#include "libavutil/avstring.h"
#include "libavutil/time.h"

#ifdef __cplusplus
}
#endif  //__cplusplus

const char program_name[]     = "WPlayer";
const int  program_birth_year = 2000;
const char cc_ident[]         = "gcc 9.1.1 (GCC) 20190807";

extern const OptionDef options[];

extern AVDictionary *sws_dict, *swr_opts, *format_opts, *codec_opts,
    *resample_opts;

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
    MainState* is = ( MainState* )ctx;
    return is->abort_request;
}

// �����ת״̬
static void stream_seek(MainState* is, int64_t pos, int64_t rel, int seek_by_bytes) {
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;

        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
        SDL_CondSignal(is->continue_read_thread);
    }
}

static int is_realtime(AVFormatContext* s) {
    if (!strcmp(s->iformat->name, "rtmp") || 
        !strcmp(s->iformat->name, "rtsp")
        || !strcmp(s->iformat->name, "sdp"))
        return 1;

    if (s->pb && (!strncmp(s->url, "rtp:", 4) || 
                  !strncmp(s->url, "udp:", 4)))
        return 1;

    return 0;
}

static int read_thread(void* arg) {
    // TODO(wangqing): ����δʵ��
    MainState* is = ( MainState* )arg;
    int        ret, err, i;
    int        st_index[AVMEDIA_TYPE_NB];


    SDL_mutex* wait_mutex = SDL_CreateMutex();
    if (!wait_mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex():%s\n", SDL_GetError());
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    memset(st_index, -1, sizeof(st_index));
    is->eof = 0;

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        flog("Could not allocate packet\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    AVFormatContext* ic = avformat_alloc_context();
    if (!ic) {
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
    if (err < 0) {
        print_error(is->filename, err);
        ret = -1;
        goto fail;
    }
    if (scan_all_pmts_set)
        av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

    AVDictionaryEntry* t;
    if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        elog("Option %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }
    is->ic = ic;

    if (genpts)  // TODO(wangqing): issue
        ic->flags |= AVFMT_FLAG_GENPTS;

    av_format_inject_global_side_data(ic);  // TODO(wangqing): issue

    if (find_stream_info) {
        AVDictionary** opts = setup_find_stream_info_opts(ic, codec_opts);
        int            orig_nb_streams = ic->nb_streams;

        err = avformat_find_stream_info(ic, opts);

        for (int i = 0; i < orig_nb_streams; ++i)
            av_dict_free(&opts[i]);
        av_freep(&opts);

        if (err < 0) {
            wlog("%s: could not find codec parameters\n", is->filename);
            ret = -1;
            goto fail;
        }
    }

    if (ic->pb)
        ic->pb->eof_reached = 0;

    if (seek_by_bytes < 0)
        seek_by_bytes = !!(ic->iformat->flags & AVFMT_TS_DISCONT)
                        && strcmp("ogg", ic->iformat->name);

    is->max_frame_duration =
        (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

    // TODO(wangqing): title�Ĵ洢λ�ã���ʽ����ʽ
    if (!window_title && (t = av_dict_get(ic->metadata, "title", NULL, 0)))
        window_title = av_asprintf("%s - %s", t->value, input_filename);

    if (start_time != AV_NOPTS_VALUE) {
        int64_t timestamp = start_time;
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        // TODO(wangqing): avformat_seek_fileʹ�÷���
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0)
            wlog("%s: could not seek to position %0.3f\n",
                 is->filename,
                 timestamp / AV_TIME_BASE);
    }

    // TODO(wangqing): ��ý�壿����
    is->realtime = is_realtime(ic);

    if (show_status)
        av_dump_format(ic, 0, is->filename, 0);

    // ��������Ƶ��
    for (i = 0; i < ic->nb_streams; ++i) {
        AVStream*   st   = ic->streams[i];
        AVMediaType type = st->codecpar->codec_type;
        st->discard = AVDISCARD_ALL;  // TODO(wangqing): discard��ʲô����
        if (type >= 0 && wanted_stream_spec[type] && st_index[type] == -1)
            if (avformat_match_stream_specifier(
                    ic, st, wanted_stream_spec[type])
                > 0)
                st_index[type] = i;
    }
    for (i = 0; i < AVMEDIA_TYPE_NB; ++i) {
        if (wanted_stream_spec[i] && st_index[i] == -1) {
            elog("Stream specifier %s does not match any %s stream\n",
                 wanted_stream_spec[i],
                 av_get_media_type_string(AVMediaType(i)));
            st_index[i] = INT_MAX;
        }
    }
    if (!video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(
            ic, AVMEDIA_TYPE_VIDEO, st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

    if (!video_disable)
        st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(
            ic, AVMEDIA_TYPE_AUDIO, st_index[AVMEDIA_TYPE_AUDIO], -1, NULL, 0);

    if (!video_disable && !subtitle_disable)
        st_index[AVMEDIA_TYPE_SUBTITLE] = av_find_best_stream(
            ic,
            AVMEDIA_TYPE_SUBTITLE,
            st_index[AVMEDIA_TYPE_SUBTITLE],
            st_index[AVMEDIA_TYPE_AUDIO] >= 0 ? st_index[AVMEDIA_TYPE_AUDIO]
                                              : st_index[AVMEDIA_TYPE_VIDEO],
            NULL,
            0);

    is->show_mode = show_mode;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        // У׼��ƵĬ�Ͽ��߱���
        AVStream*          st       = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        AVCodecParameters* codecpar = st->codecpar;
        AVRational         sar = av_guess_sample_aspect_ratio(ic, st, NULL);
        if (codecpar->width)
            set_default_window_set(codecpar->width, codecpar->height, sar);
    }

    // open stream
    if (st_index[AVMEDIA_TYPE_AUDIO])
        stream_component_open(is, st_index[AVMEDIA_TYPE_AUDIO]);

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO])
        stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO]);
    if (is->show_mode == SHOW_MODE_NONE)
        is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

    if (st_index[AVMEDIA_TYPE_SUBTITLE])
        stream_component_open(is, st_index[AVMEDIA_TYPE_SUBTITLE]);

    if (infinite_buffer < 0 && is->realtime)
        infinite_buffer = 1;

    // ���װ
    while (true) {
        if (is->abort_request)
            break;
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }
#if CONFIG_RTST_DEMUXER || CONFIG_MMSH_PROTOCOL
        if (is->paused
            && (!strcmp(ic->iformat->name, "rtsp")
                || (ic->pb && !strncmp(input_filename, "mmsh", 5)))) {
            SDL_Delay(10);
            continue;
        }
#endif
        if (is->seek_req) {
            int64_t seek_target = is->seek_pos;
            int64_t seek_min =
                is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
            int64_t seek_max =
                is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;

            ret = avformat_seek_file(
                is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                elog("%s: error while seeking\n", is->ic->url);
            } else {
                if (is->audio_stream >= 0)
                    packet_queue_flush(&is->audioq);
                if (is->subtitleq >= 0)
                    packet_queue_flush(&is->subtitleq);
                if (is->video_stream >= 0)
                    packet_queue_flush(&is->videoq);

                // TODO(wangqing): ���ִ�е���֡
                if (is->seek_flags & AVSEEK_FLAG_BYTE)
                    set_clock(&is->extclk, NAN, 0);
                else
                    set_clock(
                        &is->extclk, seek_target / ( double )AV_TIME_BASE, 0);
            }
            is->seek_req             = 0;
            is->queue_attachmets_req = 1;
            is->eof                  = 0;
            if (is->paused)
                step_to_next_frame(is);
        }

        // TODO(wangqing): issue
        if (is->queue_attachmets_req) {
            if (is->video_st
                && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                if ((ret = av_packet_ref(pkt, &is->video_st->attached_pic)) < 0)
                    goto fail;
                packet_queue_put(&is->videoq, pkt);
                packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
            }
        }

        // δ����İ�����
        if (infinite_buffer < 1
            && (is->audioq.size + is->videoq.size + is->subtitleq.size
                    > MAX_QUEUE_SIZE
                || (stream_has_enough_packets(
                        is->audio_st, is->audio_stream, &is->audioq)
                    && stream_has_enough_packets(
                           is->video_st, is->video_stream, &is->videoq)
                    && stream_has_enough_packets(is->subtitle_st,
                                                 is->subtitle_stream,
                                                 &is->subtitleq)))) {
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }

        // ���װ���
        if (!is->paused && 
            (!is->audio_st || (is->auddec.finished == is->audioq.serial && 
                               frame_queue_nb_remaining(&is->sampq) == 0))
            (!is->video_st || (is->viddec.finished == is->videoq.serial && 
                               frame_queue_nb_remaining(&is->pictq) == 0))) {
            if (loop != 1 && (!loop || --loop)) { // loop��ʾѭ��������0��ʾ����ѭ������
                stream_seek(is, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
            } else if (autoexit) {
                ret = AVERROR_EOF;
                goto fail;
            }
        }

        ret = av_read_frame(ic, pkt);
        if (ret < 0){
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                if (is->video_stream >= 0) 
                    packet_queue_put_nullpacket( &is->videoq, pkt, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket( &is->audioq, pkt, is->audio_stream);
                if (is->subtitle_stream >= 0)
                    packet_queue_put_nullpacket( &is->subtitleq, pkt, is->subtitle_stream);
                is->eof = 1;
            }
            if (ic->pb && ic->pb->error){
                if (autoexit)
                    goto fail;
                else
                    break;
            }
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;  // ʵ�ֹ��ܣ�������ɺ���ת��Ȼ����ʹ��
        }else{
            is->eof = 0;
        }

        int64_t stream_start_time = ic->streams[pkt->stream_index]->start_time;
        pkt_ts            = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        int pkt_in_play_range =
            duration == AV_NOPTS_VALUE
            || (pkt_ts
                - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0))
                           * av_q2d(ic->streams[pkt->stream_index]->time_base)
                       - ( double )(start_time != AV_NOPTS_VALUE ? start_time
                                                                 : 0)
                             / 1000000
                   < (( double )duration / 1000000);
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range){
            packet_queue_put(*is->audioq, pkt);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range && 
                   !(is->video_st->disposition
                        & AV_DISPOSITION_ATTACHED_PIC)) {
            packet_queue_put(*is->videoq, pkt);
        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range){
            packet_queue_put(*is->subtitle_stream, pkt);
        }else{
            av_packet_unref(pkt);
        }
    }

    ret = 0;
fail:
    if (ic && !is->ic)
        avformat_close_input(&ic);

    av_packet_free(&pkt);
    if (ret != 0) {
        SDL_Event event;
        event.type       = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    SDL_DestroyMutex(wait_mutex);
    return 0;
}

static void stream_component_open(MainState* is, int stream_index) {
    AVFormatContext* ic = is->ic;
    AVCodecContext*  avctx;
    AVCodec*         codec;
    const char*      forced_codec_name = NULL;
    AVDictionary*    opts              = NULL;
    AVDictionaryEntry* t                 = NULL;
    int                sample_rate, nb_channels;
    int64_t            channel_layout;
    int                ret = 0;
    int                stream_lowres = lowers;

    if (stream_index < 0 || stream_indx >= ic->nb_streams)
        return -1;

    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    ret = avcodec_parameters_to_contex(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0)
        goto fail;

    avctx->pkt_timebase = ic->streams[stream_index]->time_base;

    codec = avcodec_find_decoder(avctx->codec_id);

    switch(avctx->codec_type){
        case AVMEDIA_TYPE_AUDIO:
            is->last_audio_stream = stream_index;
            forced_codec_name     = audio_codec_name;
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            is->last_subtitle_stream = stream_index;
            forced_codec_name        = subtitle_codec_name;
            break;
        case AVMEDIA_TYPE_VIDEO:
            is->last_audio_stream = stream_index;
            forced_codec_name     = video_codec_name;
            break;
    }

    if (forced_codec_name)
        codec = avcodec_find_decoder_by_name(forced_codec_name);
    if (!codec){
        if (forced_codec_name)
            wlog("No codec could be found with name '%s'\n", forced_codec_name);
        else
            wlog("No docoder could be found for codec %s\n", avcodec_get_name(avctx->codec_id));

        ret = AVERROR(EINVAL);
        goto fail;
    }

    avctx->codec_id = codec->id;
    if (stream_lowres > codec->max_lowres){
        wlog("The maximum value for lowres supported by the decoer is %d\n", codec->max_lowres);
        stream_lowres = codec->max_lowres;
    }
    avctx->lowres = stream_lowres;

    // TODO(wangqing): fast什么用途
    if (fast)
        avctx->flags |= AV_CODEC_FLAG_FAST;

    opts = filter_codec_opts(
        codec_opts, avctx->codec_id, ic, ic->streams[stream_index], codec);

    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);

    if (stream_lowres)
        av_dict_set_init(&opts, "lowres", stream_lowres, 0);

    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        av_dict_set(&opts, "refcounted_frames", "1", 0));
        
    if  (ret = avcodec_open2(avctx, codec, *opts) < 0)
        goto fail;
    
    if ((t = av_dict_get(opts, ""， NULL, AV_DICT_IGNORE_SUFFIX))){
        elor("Options %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }

    is->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;

    switch(avctx->codec_type){
        case AVMEIDA_TYPE_AUDIO:
#if CONIFG_AVFILTER
            is->audio_filter_src.freq = avctx->sample_rate;
            is->audio_filter_src.channels = avctx->channels;
            is->audio_filter_src.channel_layout = get_valid_channel_layout(avctx->channel_layout, avctx->channels);
            is->audio_filter_src.fmt = avctx->sample_fmt;
            if ((ret = config_audio_filters(is, afilters, 0)) < 0)
                goto fail;
            AVFilterContext* sink = is->out_audio_filter;
            sample_rate = av_buffersink_get_sample_rate(sink);
            nb_channels = av_buffersink_get_channels(sink);
            channel_layout = av_buffersink_get_channel_layout(sink);
#else
            sample_rate = avctx->sample_rate;
            nb_channels = avctx->channel;
            channel_layout = avctx->channel_layout;
#endif
            break;
        case AVMEDIA_TYPE_VIDEO:
            is->video_stream = stream_index;
            is->video_st     = ic->stream[stream_index];

            decoder_init( &is->viddec, avctx, &is->videoq, is->continue_read_thread);
            if ((ret = decoder_start(&is->viddec, video_thread, "video_decoder", is))  < 0)
                goto out;
            is->queue_attachmets_req = 1;
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            is->subtitle_stream = stream_index;
            is->subtitle_st     = ic->streams[stream_index];

            decoder_init(&is->subdec, avctx, &is->subtitleq, is->continue_read_thread);
            if ((ret = decoder_start(&is->subdec, subtitle_thread, "subtitle_thread", is)) < 0)
                goto out;
            break;
    }
    goto out;

fail:
    avcodec_free_context(avctx);

out:
    av_dict_free(*opts);

    return ret;
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

    packet_queue_destroy(&is->videoq);
    packet_queue_destroy(&is->audioq);
    packet_queue_destroy(&is->subtitleq);

    frame_queue_destroy(&is->pictq);
    frame_queue_destroy(&is->sampq);
    frame_queue_destroy(&is->subpq);

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

    // ��ʼ������Stream��ʱ��
    init_clock(&is->vidclk, &is->videoq.serial);
    init_clock(&is->audclk, &is->audioq.serial);
    init_clock(&is->extclk, &is->extclk.serial);

    // TODO(wangqing): ��ʲô��;
    is->audio_clock_serial = -1;

    // ���ó�ʼ����
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
