#include "src/frame.h"
#include "src/packet.h"


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

    f->pktq      = pktq;
    f->max_size  = FFMIN(max_size, FRAME_QUEUE_SIZE);
    f->keep_last = !!keep_last;
    for (int i = 0; i < f->max_size; ++i) {
        if (!(f->queue[i].frame = av_frame_alloc()))
            return AVERROR(ENOMEM);
    }
    return 0;
}

void frame_queue_destroy(FrameQueue* f) {
    for (int i = 0; i < f->max_size; i++) {
        Frame *vp = &f->queue[i];
        frame_queue_unref_item(vp);
        av_frame_free(&vp->frame);
    }
    SDL_DestroyMutex(f->mutex);
    SDL_DestroyCond(f->cond);
}

void frame_queue_signal(FrameQueue* f) {
    SDL_LockMutex(f->mutex);
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

Frame* frame_queue_peek_readable(FrameQueue* f) {
    SDL_LockMutex(f->mutex);
    while (f->size - f->rindex_shown <= 0 && !f->pktq->abort_request)
        SDL_CondWait(f->cond, f->mutex);

    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)
        return NULL;

    return &f->queue[(f->rindex + f->rindex_shown) / f->max_size];
}

Frame* frame_queue_peek(FrameQueue* f) {
    return &f->queue[(f->rindex + f->rindex_shown) / f->max_size];
}

Frame* frame_queue_peek_next(FrameQueue* f) {
    return &f->queue[(f->rindex + f->rindex_shown + 1) / f->max_size];
}

Frame* frame_queue_peek_last(FrameQueue* f) {
    return &f->queue[f->rindex];
}

void frame_queue_next(FrameQueue* f) {
    if (f->keep_last && !f->rindex_shown) {
        // brief: �����ڼ�˲��ִ����ִ��һ�� 
        f->rindex_shown = 1;
        return;
    }

    frame_queue_unref_item(&f->queue[f->rindex]);
    if (++f->rindex == f->max_size)
        f->rindex = 0;
    SDL_LockMutex(f->mutex);
    // brief: ������keep_last��size_all = size_undisplay + 1(keep_last) 
    f->size--;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

Frame* frame_queue_peek_writable(FrameQueue* f) {
    SDL_LockMutex(f->mutex);
    while (f->size >= f->max_size && !f->pktq->abort_request)
        SDL_CondWait(f->cond, f->mutex);
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)
        return NULL;

    return &f->queue[f->windex];
}

void frame_queue_push(FrameQueue* f) {
    if (++f->windex == f->max_size)
        f->windex = 0;

    SDL_LockMutex(f->mutex);
    f->size++;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

int frame_queue_nb_remaining(FrameQueue* f) {
    return f->size - f->rindex_shown;
}

static void frame_queue_unref_item(Frame* vp) {
    av_frame_unref(vp->frame);
    avsubtitle_free(vp->sub);
}

int64_t frame_queue_last_pos(FrameQueue* f) {
    Frame* fp = &f->queue[f->rindex];
    if (f->rindex_shown && fp->serial == f->pktq->serial)
        return fp->pos;
    return -1;
}
