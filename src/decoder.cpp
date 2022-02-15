#include "decoder.h"
#include "frame.h"
#include "packet.h"
#include "tools/log.h"

int decoder_init(Decoder*        d,
                 AVCodecContext* avctx,
                 PacketQueue*    queue,
                 SDL_cond*       empty_queue_cond) {
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

int decoder_start(Decoder* d,
                  int (*fn)(void*),
                  const char* thread_name,
                  void*       arg) {
    packet_queue_start(d->queue);
    d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (!d->decoder_tid) {
        elog("SDL_CreateThread():%s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    return 0;
}

int decoder_decoder_frame(Decoder* d, AVFrame* frame, AVSubtitle* sub) {
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
                            frame->pts = frame->pkt_pts;  // brief: 基本不会发生
                    }
                    break;
                case AVMEDIA_TYPE_AUDIO:
                    ret = avcodec_receive_frame(d->avctx, frame);
                    if (ret >= 0) {
                        // TODO(wangqing): 如何获取的音频pts
                        AVRational tb{ 1, frame->sample_rate };
                        if (frame->pts != AV_NOPTS_VALUE)
                            frame->pts = av_rescale_q(
                                frame->pts, d->avctx->pkt_timebase, tb);
                        else if (d->next_pts != AV_NOPTS_VALUE)
                            frame->pts =
                                av_rescale_q(d->next_pts, d->next_pts_tb, tb);
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
                if (old_serial != d->pkt_serial){
                    avcodec_flush_buffers(d->avctx);
                    d->finished = 0;
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
                }
            }
            if (d->queue->serial == d->pkt_serial)
                break;
            av_packet_unref(d->pkt);
        } while (true);

        if (d->avctx->codec_type == AVMEDIA_TYPE_SUBTITLE){
            int got_frame = 0;
            ret = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, d->pkt);
            if (ret < 0){
                ret = AVERROR(EAGAIN);
            }else{
                if (got_frame && !d->pkt->data)
                    d->packet_pending = 1;
                ret = got_frame
                          ? 0
                          : (d->pkt->data ? AVERROR(EAGAIN) : AVERROR_EOF);
            }
            av_packet_unref(d->pkt);
        }else{
            if (avcodec_send_packet(d->avctx, d->pkt) == AVERROR(EAGAIN)){
                elog("receive_frame and send_packet_both return EAGAIN, which is an API violation.\n");
                d->packet_pending = 1;
            }else{
                av_packet_unref(d->pkt);
            }
        }
    }
}

int audio_thread(void* arg) {
#if CONFIG_AVFILTER
    int     last_serial = -1;
    int64_t dec_channel_layout;
    int     reconfigure;
#endif
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return AVERROR(ENOMEM);

    MainState* is = ( MainState* )arg;
    Frame*     af;
    int        got_frame = 0;
    int        ret       = 0;
    do {
        if ((got_frame = decoder_decoder_frame(&is->auddec, frame, NULL)) < 0)
            goto the_end;
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

the_end:
#if CONFIG_AVFILTER
    avfilter_graph_free(&is->agrph);
#endif
    av_frame_free(&frame);
    return ret;
}

int video_thread(void* arg) {}

int subtitle_thread(void* arg) {}