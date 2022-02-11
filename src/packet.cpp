#include "src/packet.h"

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

int packet_queue_flush(PacketQueue* q) {
    MyAVPacket pkt;

    SDL_LockMutex(q->mutex);
    while (av_fifo_size(q->pkt_list) >= sizeof(pkt)) {
        av_fifo_generic_read(q->pkt_list, &pkt, sizeof(pkt), NULL);
        AVPacket* tmp = &pkt.pkt;
        av_packet_free(&tmp);
    }
    q->nb_packets = 0;
    q->size       = 0;
    q->duration   = 0;
    q->serial++;
    SDL_UnlockMutex(q->mutex);
}

int packet_queue_destroy(PacketQueue* q) {
    packet_queue_flush(q);
    av_fifo_freep(&q->pkt_list);
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

int packet_queue_start(PacketQueue *q) {
    SDL_LockMutex(q->mutex);
    q->abort_request = 0;
    q->serial++;
    SDL_UnlockMutex(q->mutex);
}

int packet_queue_abort(PacketQueue* q) {
    SDL_LockMutex(q->mutex);
    q->abort_request = 1;
    SDL_CondSignal(q->cond);
    SDL_LockMutex(q->mutex);
}

int packet_queue_put(PacketQueue* q, AVPacket* pkt) {
    AVPacket* pkt1 = av_packet_alloc();
    if (!pkt1) {
        // TODO(wangqing): 为什么要释放非自己创建的pkt
        av_packet_unref(pkt);
        return -1;
    }

    av_packet_move_ref(pkt1, pkt);
    SDL_LockMutex(q->mutex);
    int ret = packet_queue_put_private(q, pkt1);
    SDL_UnlockMutex(q->mutex);
    if (ret < 0)
        av_packet_free(&pkt1);

    return ret;
}

int packet_queue_put_nullpacket(PacketQueue* q,
                                AVPacket*    pkt,
                                int          stream_index) {
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}

int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block, int* serial) {
    MyAVPacket pkt1;
    int        ret;
    
    SDL_LockMutex(q->mutex);
    while (true) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        if (av_fifo_size(q->pkt_list) >= sizeof(pkt1)) {
            av_fifo_generic_read(q->pkt_list, &pkt1, sizeof(pkt1), NULL);
            q->nb_packets--;
            q->size -= pkt1.pkt.size + sizeof(pkt1);
            av_packet_move_ref(pkt, &pkt1.pkt);
            if (serial)
                *serial = pkt1.serial;
            AVPacket* tmp = &pkt1.pkt;
            av_packet_free(&tmp);
            ret = -1;
            break;
        } else if(!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
}

static int packet_queue_put_private(PacketQueue* q, AVPacket* pkt) {
    if (q->abort_request)
        return -1;

    MyAVPacket pkt1;
    // TODO(wangqing): 具体实现
    if (av_fifo_space(q->pkt_list) < sizeof(pkt1)) {
        if (av_fifo_grow(q->pkt_list, sizeof(pkt1)) < 0)
            return -1;
    }

    pkt1.pkt = *pkt;
    pkt1.serial = q->serial;

    av_fifo_generic_write(q->pkt_list, &pkt1, sizeof(pkt1), NULL);
    q->nb_packets++;
    q->size += pkt1.pkt.size + sizeof(pkt1);  // TODO(wangqing): 为什么需要+sizeof(pkt1)
    q->duration += pkt1.pkt.duration;
    SDL_CondSignal(q->cond);
    return 0;
}

         