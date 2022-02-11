
#ifndef __FRAME_H__
#define __FRAME_H__

#include "my_struct.h"
struct PacketQueue;
// decode后的一个frame  = audio + video + subtitle
struct Frame {
    AVFrame*    frame;     // 数据帧
    AVSubtitle* sub;       // 字幕
    int         serial;    // 播放序列，在seek的操作时serial会变化
    double      pts;       // 显示
    double      duration;  // 该帧持续时长
    int64_t     pos;       // 该帧在输入文件中的位置
    int         width;     // 图像宽度
    int         height;    // 图像高度
    int         format;    // AVPixelFormat | AVSampleFormat
    AVRational  sar;       // 图像的宽高比例，未知则为0/1
    int         uploaded;  // 该帧释放已经播放过
    int         flip_v;    // =1 旋转180，=0正常播放
};

// decode后的frame queue, 视频，视频，字幕独立存储
struct FrameQueue {  // TODO(可以考虑使用环形队列):
    Frame        queue[FRAME_QUEUE_SIZE];  // 不宜过大，消耗内存
    int          rindex;                   // 读索引，待播放时读取该帧进行播放，播放后该帧称为上一帧
    int          windex;                   // 写索引
    int          size;                     // 当前总帧数
    int          max_size;                 // 可存储的最大帧数
    int          keep_last;                // =1说明要在队列里面保持最后一帧的数据不释放，只在销毁队列的时候才能将其真正释放
    int          rindex_shown;             // TODO(wangqing): issue
    SDL_mutex*   mutex;
    SDL_cond*    cond;
    PacketQueue* pktq;
};

void frame_queue_unref_item(Frame*);
int  frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);
void  frame_queue_destroy(FrameQueue* f);
void frame_queue_signal(FrameQueue* f);
Frame* frame_queue_peek(FrameQueue* f);
Frame* frame_queue_peek_next(FrameQueue* f);
Frame* frame_queue_peek_last(FrameQueue* f);
Frame* frame_queue_peek_writable(FrameQueue* f);
Frame* frame_queue_peek_readable(FrameQueue* f);
void   frame_queue_push(FrameQueue* f);
void   frame_queue_next(FrameQueue* f);

int frame_queue_nb_remaining(FrameQueue* f);
int64_t frame_queue_last_pos(FrameQueue* f);


#endif  // __FRAME_H__
