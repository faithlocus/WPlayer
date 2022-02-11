
#ifndef __FRAME_H__
#define __FRAME_H__

#include "my_struct.h"
struct PacketQueue;
// decode���һ��frame  = audio + video + subtitle
struct Frame {
    AVFrame*    frame;     // ����֡
    AVSubtitle* sub;       // ��Ļ
    int         serial;    // �������У���seek�Ĳ���ʱserial��仯
    double      pts;       // ��ʾ
    double      duration;  // ��֡����ʱ��
    int64_t     pos;       // ��֡�������ļ��е�λ��
    int         width;     // ͼ����
    int         height;    // ͼ��߶�
    int         format;    // AVPixelFormat | AVSampleFormat
    AVRational  sar;       // ͼ��Ŀ�߱�����δ֪��Ϊ0/1
    int         uploaded;  // ��֡�ͷ��Ѿ����Ź�
    int         flip_v;    // =1 ��ת180��=0��������
};

// decode���frame queue, ��Ƶ����Ƶ����Ļ�����洢
struct FrameQueue {  // TODO(���Կ���ʹ�û��ζ���):
    Frame        queue[FRAME_QUEUE_SIZE];  // ���˹��������ڴ�
    int          rindex;                   // ��������������ʱ��ȡ��֡���в��ţ����ź��֡��Ϊ��һ֡
    int          windex;                   // д����
    int          size;                     // ��ǰ��֡��
    int          max_size;                 // �ɴ洢�����֡��
    int          keep_last;                // =1˵��Ҫ�ڶ������汣�����һ֡�����ݲ��ͷţ�ֻ�����ٶ��е�ʱ����ܽ��������ͷ�
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
