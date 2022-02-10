
#ifndef __FRAME_H__
#define __FRAME_H__

#include "my_struct.h"

void frame_queue_unref_item(Frame*);
int  frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);
int  frame_queue_destroy(FrameQueue* f);
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
