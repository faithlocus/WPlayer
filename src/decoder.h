#ifndef __DECODER_H__ 
#define __DECODER_H__

#ifdef __cplusplus
extern "C" {
#endif 

#include "libavcodec/avcodec.h"
#ifdef __GNUC__
#include "SDL/SDL.h"
#include "SDL/SDL_thread.h"
#else
#include "SDL.h"
#include "SDL_thread.h"
#endif

#ifdef __cplusplus
}
#endif //__cplusplus

struct PacketQueue;
// 解码器，音频，视频，字幕独立不影响,即独立线程解码
struct Decoder {
    AVPacket*       pkt; 
    PacketQueue*    queue;          // 数据包队列
    AVCodecContext* avctx;          // 解码上下文

    int       pkt_serial;           // 包序列
    int       finished;             // =0工作状态， !=0空闲状态
    int       packet_pending;       // =0解码器异常，需要重置，=1解码器处于正常状态
    SDL_cond* empty_queue_cond;     // 检测到packet队列空时发送该信号通知read_thread读取数据

    int64_t     start_pts;          // 初始化时stream的start_time
    AVRational  start_pts_tb;       // 初始化时stream的time_base
    int64_t     next_pts;           // 记录最近一次解码后的frame的pts，当解出来的部分帧没有有效的pts时，使用该变量进行推算
    AVRational  next_pts_tb;        // next_pts的单位
    SDL_Thread* decoder_tid;        // 线程句柄
};

int audio_thread(void* arg);
int video_thread(void* arg);
int subtitle_thread(void* arg);

int decoder_decoder_frame(Decoder* d, AVFrame* frame, AVSubtitle* sub);

int decoder_init(Decoder*        d,
                  AVCodecContext* avcx,
                  PacketQueue*    queue,
                  SDL_cond*       empty_queue_cond);

int decoder_start(Decoder* d,
                  int (*fn)(void*),
                  const char* thread_name,
                  void*       arg);
#endif  // __DECODER_H__