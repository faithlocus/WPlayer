// TODO(wangqing): 改变为类组织
#ifndef __PACKET_H__
#define __PACKET_H__

#include "my_struct.h"

int packet_queue_put(PacketQueue* q, AVPacket* pkt);
int packet_queue_put_nullpacket(PacketQueue* q,
                                AVPacket*    pkt,
                                int          stream_index);
int packet_queue_init(PacketQueue* q);
int packet_queue_flush(PacketQueue *q);
int packet_queue_destroy(PacketQueue *q);
int packet_queue_abort(PacketQueue* q);
int packet_queue_start(PacketQueue *q);
int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block, int* serial);

#endif  // __PACKET_H__
