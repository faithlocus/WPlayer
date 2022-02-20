#include "src/decoder.h"
#include "frame.h"
#include "packet.h"
#include "tools/log.h"

extern "C" {

#include "libavformat/avformat.h"
#include "libavutil/frame.h"
}

int decoder_init(Decoder *       d,
                 AVCodecContext *avctx,
                 PacketQueue *   queue,
                 SDL_cond *      empty_queue_cond) {
    memset(d, 0, sizeof(Decoder));
    d->pkt = av_packet_alloc();
    if (!d->pkt)
        return AVERROR(ENOMEM);
    d->avctx            = avctx;
    d->queue            = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts        = AV_NOPTS_VALUE;
    d->pkt_serial       = -1;
    return 0;
}

int decoder_start(Decoder *d, int (*fn)(void *), const char *thread_name, void *arg) {
    packet_queue_start(d->queue);
    d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (!d->decoder_tid) {
        elog("SDL_CreateThread():%s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    return 0;
}

int decoder_decoder_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub) {
    int ret = AVERROR(EAGAIN);

    while (true) {
        if (d->queue->serial == d->pkt_serial) {
            do {
                if (d->queue->abort_request)
                    return -1;

                switch (d->avctx->codec_type) {
                case AVMEDIA_TYPE_VIDEO:
                    ret = avcodec_receive_frame(d->avctx, frame);
                    if (ret >= 0) {
                        if (decoder_reorder_pts == -1)
                            frame->pts = frame->best_effort_timestamp;
                        else if (!decoder_reorder_pts)
                            frame->pts = frame->pkt_pts;  // brief: ??????????
                    }
                    break;
                case AVMEDIA_TYPE_AUDIO:
                    ret = avcodec_receive_frame(d->avctx, frame);
                    if (ret >= 0) {
                        // TODO(wangqing): ??��???????pts
                        AVRational tb{ 1, frame->sample_rate };
                        if (frame->pts != AV_NOPTS_VALUE)
                            frame->pts = av_rescale_q(frame->pts, d->avctx->pkt_timebase, tb);
                        else if (d->next_pts != AV_NOPTS_VALUE)
                            frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                        if (frame->pts != AV_NOPTS_VALUE) {
                            d->next_pts    = frame->pts + frame->nb_samples;
                            d->next_pts_tb = tb;
                        }
                    }
                    break;
                default:
                    break;
                }
                if (ret == AVERROR_EOF) {
                    d->finished = d->pkt_serial;
                    avcodec_flush_buffers(d->avctx);
                    return 0;
                }
                if (ret >= 0)
                    return 1;
            } while (ret != AVERROR(EAGAIN));
        }

        do {
            if (d->queue->nb_packets == 0)
                SDL_CondSignal(d->empty_queue_cond);

            if (d->packet_pending) {
                d->packet_pending = 0;
            } else {
                int old_serial = d->pkt_serial;
                if (packet_queue_get(d->queue, d->pkt, 1, &d->pkt_serial) < 0)
                    return -1;
                if (old_serial != d->pkt_serial) {
                    avcodec_flush_buffers(d->avctx);
                    d->finished    = 0;
                    d->next_pts    = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            }
            if (d->queue->serial == d->pkt_serial)
                break;
            av_packet_unref(d->pkt);
        } while (true);

        if (d->avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            int got_frame = 0;
            ret           = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, d->pkt);
            if (ret < 0) {
                ret = AVERROR(EAGAIN);
            } else {
                if (got_frame && !d->pkt->data)
                    d->packet_pending = 1;
                ret = got_frame ? 0 : (d->pkt->data ? AVERROR(EAGAIN) : AVERROR_EOF);
            }
            av_packet_unref(d->pkt);
        } else {
            if (avcodec_send_packet(d->avctx, d->pkt) == AVERROR(EAGAIN)) {
                elog("receive_frame and send_packet_both return EAGAIN, which is an API "
                     "violation.\n");
                d->packet_pending = 1;
            } else {
                av_packet_unref(d->pkt);
            }
        }
    }
}

int audio_thread(void *arg) {
#if CONFIG_AVFILTER
    int     last_serial = -1;
    int64_t dec_channel_layout;
    int     reconfigure;
#endif
    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return AVERROR(ENOMEM);

    MainState *is = ( MainState * )arg;
    Frame *    af;
    int        got_frame = 0;
    int        ret       = 0;
    do {
        if ((got_frame = decoder_decoder_frame(&is->auddec, frame, NULL)) < 0)
            goto the_end;
        if (got_frame) {
            tb = (AVRational){ 1, frame->sample_rate };

#if CONFIG_AVFILTER
            dec_channel_layout = get_valid_layout(frame->channel_layout, frame->channels);
            reconfigure        = cmd_audio_fmts(is->audio_filter_src.fmt,
                                         is->audio_filter_src.channels,
                                         frame->format,
                                         frame->channels)
                          || is->audio_filter_src.channel_layout != dec_channel_layout
                          || is->audio_filter_src.freq != frame->sample_rate
                          || is->auddec.pkt_serial != last_serial;

            if (reconfigure) {
                char buf1[1024], buf2[1024];
                av_get_channel_layout_string(
                    buf1, sizeof(buf1), -1, is->audio_filter_src.channel_layout);
                av_get_channel_layout_string(buf2, sizeof(buf2), -1, dec_channel_layout);
                av_log(NULL,
                       AV_LOG_DEBUG,
                       "Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to "
                       "rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
                       is->audio_filter_src.freq,
                       is->audio_filter_src.channels,
                       av_get_sample_fmt_name(is->audio_filter_src.fmt),
                       buf1,
                       last_serial,
                       frame->sample_rate,
                       frame->channels,
                       av_get_sample_fmt_name(frame->format),
                       buf2,
                       is->auddec.pkt_serial);

                is->audio_filter_src.fmt            = frame->format;
                is->audio_filter_src.channels       = frame->channels;
                is->audio_filter_src.channel_layout = dec_channel_layout;
                is->audio_filter_src.freq           = frame->sample_rate;
                last_serial                         = is->auddec.pkt_serial;

                if ((ret = configure_audio_filters(is, afilters, 1)) < 0)
                    goto the_endl;
            }

            if ((ret = av_buffersrc_add_frame(is->in_audio_filter, frame)) < 0)
                goto the_end;

            while ((ret = av_buffersink_get_frame_flags(is->out_audio_filter, frame, 0)) >= 0) {
                tb = av_buffersink_get_time_base(is->out_audio_filter);

#endif
                if (!(af = frame_queue_peek_writable(&is->sampq)))
                    goto the_endl;

                af->pts      = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                af->pos      = frame->pkt_pos;
                af->serial   = is->auddec.pkt_serial;
                af->duration = av_q2d(AVRational{ frame->nb_samples, frame->sample_rate });

                av_frame_move_ref(af->frame, frame);
                frame_queue_push(&is->sampq);
#if CONFIG_AVFILTER
                if (is->audioq.serial != is->auddec.pkt_serial)
                    break;
            }
            if (ret == AVERROR_EOF)
                is->auddec.finished = is->auddec.pkt_serial;
#endif
        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

the_end:
#if CONFIG_AVFILTER
    avfilter_graph_free(&is->agrph);
#endif
    av_frame_free(&frame);
    return ret;
}

int video_thread(void *arg) {
    MainState *is    = ( MainState * )arg;
    AVFrame *  frame = av_frame_alloc();
    double     pts, duration;
    int        ret;
    AVRational tb         = is->video_st->time_base;
    AVRational frame_rate = av_guess_frame_rate(is, is->video_st, NULL);

#if CONFIG_AVFILTER
    AVFilterGraph* graph = NULL;
    AVFilterContext *filt_out = NULL, *filt_in = NULL;
    int              last_w = 0, last_h = 0;
    AVPixelFormat    last_format = -2;
    int              last_serial = -1, last_vfiler_idx = 0;
#endif

    if (!frame)
        return AVERROR(ENOMEM);

    while (true) {
        ret = get_video_frame(is, frame);
        if (ret < 0)
            goto thd_end;
        if (!ret)
            continue;

#if CONFIG_AVFILTER
        if (last_w != frame->width || 
            last_h != frame->height || 
            last_format != frame->format || 
            last_serial != is->viddec.pkt_serial || 
            last_vfiler_idx != is->vfilter_idx) {
            dlog("Video frame changed from size:%dx%d format:%s serial:%d to size:%dx%d format:%s serial:%d\n", 
                    last_w, 
                    last_h, 
                    last_format, 
                    (const char *)av_x_if_null(av_get_pix_fmt_name(last_format), "none"), 
                    last_serial,
                    frame->width, 
                    frame->height,
                    frame->format,
                    (cons char*)av_x_if_null(av_get_pix_fmt_name(frame->format), "none"),
                    is->viddec.pkt_serial);

            avfilter_graph_free(&graph);
            graph = avfilter_graph_alloc();
            if(!graph){
                ret = AVERROR(ENOMEM);
                goto the_end;
            }

            graph->nb_threads = filter_nbthreads;
            if ((ret = configure_video_filters(graph, is, vfilters_list ? vfilters_list[is->vfilter_idx]:NULL, frame)) < 0){
                SDL_Event event;
                event.type = FF_QUIT_EVENT;
                event.user.data1 = is;
                SDL_PushEvent(&event);
                goto the_end;
            }
            filt_in = is->in_video_filter;
            filt_out = is->out_audio_filter;
            last_w   = frame->width;
            last_h   = frame->height;
            last_format = frame->format;
            last_serial = is->viddec.pkt_serial;
            last_vfiler_idx = is->vfilter_idx;
            frame_rate      = av_buffersink_get_frame_rate(filt_out);
        }

        ret = av_buffersrc_add_frame(frame);
        if (ret < 0)
            goto the_endl;

        while(ret >= 0){
            is->frame_last_returned_time = av_gettime_relative() / 1000000.0;

            ret = av_buffersink_get_frame_flags(filt_out, frame, 0);
            if (ret < 0){
                if (ret  == AVERROR_EOF)
                    is->viddec.finished = is->viddec.pkt_serial;
                ret = 0;
                break;
            }

            s->frame_last_filter_delay =
                av_gettime_relative() / 1000000.0 - is->frame_last_returned_time;
            if (fabs(is->frame_last_fitler_delay) > AV_NOSYNC_THRRESHOLD / 10.0)
                is->frame_last_fitler_delay = 0;
            tb = av_buffersink_get_time_base(filt_out);
    #endif

            duration = (frame_rate->num && frame_rate->den
                            ? av_q2d((AVRational){ frame_rate.den, frame_rate.den })
                            : 0);
            pts      = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            ret      = queue_picture(
                is, frame, pts, duration, frame->pkt_pos, is->viddec.pkt_serial);
            av_frame_unref(frame);

    #if CONFIG_AVFILTER
            if (is->videoq.serial != is->viddec.pkt_serial)
                break;
        }

#endif
        if (ret < 0)
            goto the_end;
    }

the_end:

#if CONFIG_AVFILTER
    avfilter_graph_free(&frame);
#endif
    av_frame_free(frame);
    return 0;
}

int subtitle_thread(void *arg) {
    MainState* is = arg;
    Frame*     sp;
    int        got_subtitle;
    double     pts;

    while(true){
        if (!(sp = frame_queue_peek_writable(&is->subpq)))
            return 0;

        if (got_subtitle = decoder_decoder_frame(&is->subdec, NULL, &sp->sub) < 0)
            break;

        pts = 0;

        if (got_subtitle && sp->sub->format == 0){
            if (sp->sub.pts != AV_NOPTS_VALUE)
                pts = sp->sub.pts / ( double )AV_TIME_BASE;
            sp->pts = pts;
            sp->serial = is->subdec.pkt_serial;
            sp->width  = is->subdec.avctx->width;
            sp->height = is->subdec.avctx->height;
            sp->uploaded = 0;

            frame_queue_push(&is->subpq);
        }else if (got_subtitle){
            avsubtitile_free(&sp->sub);
        }
    }
    return 0;
}

static int get_video_frame(MainState *is, AVFrame *frame){

}