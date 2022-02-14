#include "decoder.h"
#include "packet.h"
#include "tools/log.h"

int decoder_init(Decoder *d,
                  AVCodecContext *avctx,
                  PacketQueue *queue,
                  SDL_cond *empty_queue_cond){
    memset(d, 0, sizeof(Decoder));
    d->pkt = av_packet_alloc();
    if(!d->pkt)
        return AVERROR(ENOMEM);
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts        = AV_NOPTS_VALUE;
    d->pkt_serial       = -1;
    return 0;
}

int decoder_start(Decoder *d,
                  int (*fn)(void *),
                  const char *thread_name,
                  void *arg){
    packet_queue_start(d->queue);
    d->decoder_tid = SDL_CreateThread(fn, thread_name, arg);
    if (!d->decoder_tid){
        elog("SDL_CreateThread():%s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    return 0;
}

int audio_thread(void *arg){

}

int video_thread(void *arg){

}

int subtitle_thread(void *arg){

}