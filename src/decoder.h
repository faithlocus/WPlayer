#ifndef __DECODER_H__ 
#define __DECODER_H__

#ifdef __cplusplus
extern "C" {
#endif 

#include "libavcodec/avcodec.h"
#include "SDL/SDL.h"

#ifdef __cplusplus
}
#endif //__cplusplus

struct PacketQueue;
// 解码器，音频，视频，字幕独立不影响,即独立线程解码
struct Decoder {
    AVPacket        pkt;
    PacketQueue*    queue;
    AVCodecContext* avctx;

    int       pkt_serial;
    int       finished;
    int       packet_pending;
    SDL_cond* empty_queue_cond;

    int64_t     start_pts;
    AVRational  start_pts_tb;
    int64_t     next_pts;
    AVRational  next_pts_tb;
    SDL_Thread* decoder_tid;
};

int audio_thread(void* arg);
int video_thread(void* arg);
int subtitle_thread(void* arg);

void decoder_init(Decoder*        d,
                  AVCodecContext* avcx,
                  PacketQueue*    queue,
                  SDL_cond*       empty_queue_cond);

int decoder_start(Decoder* d,
                  int (*fc)(void*),
                  const char* thread_name,
                  void*       arg);
#endif  // __DECODER_H__