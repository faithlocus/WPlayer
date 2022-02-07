#ifndef __MY_STRUCT_H__ 
#define __MY_STRUCT_H__ 

#include "comm_variable.h"

#ifdef __cplusplus
extern "C" {
#endif 

#ifdef __GNUC__
#include "SDL2/SDL_mutex.h"
#include "SDL2/SDL_thread.h"
#include "SDL2/SDL_render.h"
#elif _MSC_VER
#include "SDL_mutex.h"
#include "SDL_thread.h"
#include "SDL_render.h"
#endif

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"
#include "libavutil/frame.h"
#include "libavutil/rational.h"
#include "libswresample/swresample.h"

#ifdef __cplusplus
}
#endif //__cplusplus

// demux的一个packet
struct MyAVPacketList
{
    AVPacket pkt;
    MyAVPacketList* next;
    int             serial;
};

// demux后的packet队列
struct PacketQueue
{
    MyAVPacketList *first_pkt, *last_pkt;
    int             nb_packets;
    int             size;
    int64_t         duration;
    int             abort_request;
    int             serial;
    SDL_mutex*      mutet;
    SDL_cond*       cond;
};

// 音频参数，重采样前 + 重采样后
struct AudioParams
{
    int freq;    // 采样率
    int channels; // 通道数量
    int64_t channel_layout; // 通道布局
    AVSampleFormat fmt; // 采样强度
    int            frame_size; // TODO(): <wangqing@gaugene.com>-<2022-02-06>
    int            bytes_per_sec; // TODO(): <wangqing@gaugene.com>-<2022-02-06>
};

// 音视频同步时钟，audio clock, video clock, subtitle clock
struct Clock
{// TODO(): <wangqing@gaugene.com>-<2022-02-06>
    double pts;
    double pts_drift;
    double last_updates;
    double speed;
    int    serial;
    int    paused;
    int*   queue_serial;
};

// decode后的一个frame  = audio + video + subtitle
struct Frame
{
    AVFrame* frame;
    AVSubtitle sub;
    int        serial;
    double     pts;
    double duration;
    int64_t    pos;
    int        width;
    int        height;
    int        format;
    AVRational sar;
    int        uploaded;
    int        flip_v;
};

// decode后的frame queue, 视频，视频，字幕独立存储
struct FrameQueue
{// TODO(可以考虑使用环形队列): <wangqing@gaugene.com>-<2022-02-06>
    Frame queue[FRAME_QUEUE_SIZE];
    int   rindex;
    int   windex;
    int   size;
    int   max_size;
    int   keep_last;
    int   rindex_shown;
    SDL_mutex* mutex;
    SDL_cond*  cond;
    PacketQueue* pktq;
};

// 时钟同步类别
enum ClockMaster
{
    AV_SYNC_AUDIO_MASTER,  // default 
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK // 外部时钟
};

// 解码器，音频，视频，字幕独立不影响,即独立线程解码
struct Decoder
{
    AVPacket pkt;
    PacketQueue* queue;
    AVCodecContext* avctx;

    int pkt_serial;
    int finished;
    int packet_pending;
    SDL_cond* empty_queue_cond;

    int64_t   start_pts;
    AVRational start_pts_tb;
    int64_t    next_pts;
    AVRational next_pts_tb;
    SDL_Thread* decoder_tid;
};

struct MainState 
{
    SDL_Thread* read_tid; // 独立线程解封装
    AVInputFormat* iformat;
    int            abort_request;
    int            force_refresh;
    int            paused;
    int            last_paused;
    int            queue_attachmets_req;
    
    // seek跳转
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;

    int read_pause_return;
    AVFormatContext* ic;
    int              realtime;

    // 同步时钟
    Clock audclk;
    Clock vidclk;
    Clock extclk;

    // 解码后framequeue
    FrameQueue pictq;
    FrameQueue subpq;
    FrameQueue sampq;

    // 解码器
    Decoder auddec;
    Decoder viddec;
    Decoder subdec;

    int audio_stream;

    int av_sync_type;

    double audio_clock;
    int    auio_clock_serial;
    double audio_diff_cum;
    double audio_diff_threshold;
    int    audio_diff_avg_count;
    AVStream* audio_st;
    PacketQueue audioq;
    int         audio_hw_buf_size;
    uint8_t*    audio_buf;
    uint8_t*    audio_buf1;
    unsigned int audio_buf_size;
    unsigned int audio_buf1_size;
    int          audio_buf_index;
    int          audio_write_buf_size;
    int          audio_volume;
    int          muted;
    AudioParams  audio_src;
    AudioParams  audio_tgt;
    SwrContext*  swr_ctx;
    int          frame_drop_early;
    int          frame_drop_late;

    enum ShowMode {
        SHOW_MODE_NONE  = -1,
        SHOW_MODE_VIDEO = 1,
        SHOW_MODE_WAVES,
        SHOW_MODE_RDFT,
        SHOW_MODE_NB
    } show_mode;
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int     sample_array_index;
    int     last_start;
    RDFTContext* rdft;
    int          rdft_bits;
    FFTSample*   rdft_data;
    int          xpos;
    double       last_vis_time;
    SDL_Texture* vis_texture;
    SDL_Texture* sub_texture;
    SDL_Texture* vid_texture;
};

#endif  // __MY_STRUCT_H__