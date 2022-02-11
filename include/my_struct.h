#ifndef __MY_STRUCT_H__
#define __MY_STRUCT_H__

#include "comm_variable.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#include "SDL2/SDL_mutex.h"
#include "SDL2/SDL_render.h"
#include "SDL2/SDL_thread.h"
#elif _MSC_VER
#include "SDL_mutex.h"
#include "SDL_render.h"
#include "SDL_thread.h"
#endif

#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/fifo.h"
#include "libavutil/frame.h"
#include "libavutil/rational.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"

#ifdef __cplusplus
}
#endif  //__cplusplus

// demux的一个packet
struct MyAVPacket {
    AVPacket pkt;
    int      serial;
};

// demux后的packet队列
struct PacketQueue {
    // TODO(wangqing): 读写独立的缓存队列
    AVFifoBuffer* pkt_list;
    int           nb_packets;
    int           size;
    int64_t       duration;
    int           abort_request;
    int           serial;
    SDL_mutex*    mutex;
    SDL_cond*     cond;
};

// 音频参数，重采样前 + 重采样后
struct AudioParams {
    int            freq;            // 采样率
    int            channels;        // 通道数量
    int64_t        channel_layout;  // 通道布局
    AVSampleFormat fmt;             // 采样强度
    int            frame_size;  // TODO(): <wangqing@gaugene.com>-<2022-02-06>
    int bytes_per_sec;          // TODO(): <wangqing@gaugene.com>-<2022-02-06>
};

// 音视频同步时钟，audio clock, video clock, subtitle clock
struct Clock {  
    double pts;            // 时钟基础，当前帧(待播放)显示时间戳，播放后，当前帧变为上一帧
    double pts_drift;      // = pts - last_updates
    double last_updates;   // 最后一次更新的系统时钟
    double speed;          // 时钟速度开支，用于控制播放速度
    int    serial;         // 时钟基于serial指定的packet
    int    paused;         // 暂定状态
    int*   queue_serial;   // 指向视频流/音频流/字幕流
};

// decode后的一个frame  = audio + video + subtitle
struct Frame {
    AVFrame*   frame;
    AVSubtitle sub;
    int        serial;
    double     pts;      // 显示
    double     duration; // 该帧持续时长
    int64_t    pos;
    int        width;
    int        height;
    int        format;
    AVRational sar;
    int        uploaded;
    int        flip_v;
};

// decode后的frame queue, 视频，视频，字幕独立存储
struct FrameQueue {  // TODO(可以考虑使用环形队列):
                     // <wangqing@gaugene.com>-<2022-02-06>
    Frame        queue[FRAME_QUEUE_SIZE];
    int          rindex;
    int          windex;
    int          size;
    int          max_size;
    int          keep_last;
    int          rindex_shown;
    SDL_mutex*   mutex;
    SDL_cond*    cond;
    PacketQueue* pktq;
};

// 时钟同步类别
enum ClockMaster {
    AV_SYNC_AUDIO_MASTER,  // default
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK  // 外部时钟
};

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

enum ShowMode {
    SHOW_MODE_NONE  = -1,
    SHOW_MODE_VIDEO = 1,
    SHOW_MODE_WAVES,
    SHOW_MODE_RDFT,
    SHOW_MODE_NB
};
struct MainState {
    SDL_Thread*    read_tid;  // 独立线程解封装,读取文件
    AVInputFormat* iformat;   // 指向demuxer
    AVFormatContext* ic;   // iformat的上下文
    int            abort_request; // =1请求退出播放
    int            force_refresh;  // =1 立即刷新页面
    int            paused;          // =1 即将暂停， =0 即将播放
    int            last_paused;     // 当前播放或者暂停状态 
    int            queue_attachmets_req;  // TODO(wangqing): issue

    // seek跳转
    int     seek_req;    // 标记一次seek请求
    int     seek_flags;  // seek标志，
    int64_t seek_pos;    // seek的目标位置（当前位置 + 增量）
    int64_t seek_rel;    // 本次seek的位置增量

    int              read_pause_return;
    int              realtime;  // =1 实时流

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

    int audio_stream;    // 音频流索引
    int av_sync_type;    // 音视频同步类型,默认AUDIO_MASTER

    double       audio_clock;              // 当前音频pts+当前帧duration
    int          audio_clock_serial;
    double       audio_diff_cum;
    double       audio_diff_threshold;
    int          audio_diff_avg_count;
    AVStream*    audio_st;             // 音频流
    PacketQueue  audioq;               // 音频packet队列
    int          audio_hw_buf_size;  // TODO(wangqing): SDL音频缓冲区
    uint8_t*     audio_buf;           // 指向重采样前的数据
    uint8_t*     audio_buf1;          // 指向重采样后的数据
    unsigned int audio_buf_size;      // 待播放的一帧音频数据(audio_buf)的大小
    unsigned int audio_buf1_size;     // 申请到的音频缓冲区(audio_buf1)
    int          audio_buf_index;
    int          audio_write_buf_size;
    int          audio_volume;        // 音量
    int          muted;               // =1静音，=0正常
    AudioParams  audio_src;           // 音频frame参数
#if CONFIG_AVFILTER
    AudioParams audio_filter_src;
#endif
    AudioParams audio_tgt;            // SDL支持的音频参数(重采样)
    SwrContext* swr_ctx;              // 音频重采样context
    int         frame_drop_early;     // 丢弃视频packet计数
    int         frame_drop_late;      // 丢弃视频frame计数

    ShowMode     show_mode;
    int16_t      sample_array[SAMPLE_ARRAY_SIZE];
    int          sample_array_index;
    int          last_start;
    RDFTContext* rdft;
    int          rdft_bits;
    FFTSample*   rdft_data;
    int          xpos;
    double       last_vis_time;
    SDL_Texture* vis_texture;
    SDL_Texture* sub_texture;
    SDL_Texture* vid_texture;

    int         subtitle_stream;
    AVStream*   subtitle_st;
    PacketQueue subtitleq;

    double      frame_timer;                 // 记录最后一帧播放的时刻
    double      frame_last_returned_time;
    double      frame_last_fitler_delay;
    int         video_stream;
    AVStream*   video_st;
    PacketQueue videoq;
    double      max_frame_duration;          // 一帧最大间隔，above this, we consider the jump a timestamp discontinuity
    SwsContext* img_convert_ctx;             // 视频尺寸格式变换
    SwsContext* sub_convert_ctx;             // 字母尺寸格式变换
    int         eof;                         // 读取是否结束

    char* filename;
    int   width, height, xleft, ytop;
    int   step;         // =1 步进播放模式， =0其他

#if CONFIG_AVFILTER
    int              vfilter_idx;
    AVFilterContext* in_video_filter;
    AVFilterContext* out_video_filter;
    AVFilterContext* in_audio_filter;
    AVFilterContext* out_audio_filter;
    AVFilterGraph*   agraph;
#endif

    // 保留最近的相应audio, video, subtitle流的stream_index
    int last_video_stream, last_audio_stream, last_subtitle_stream;

    // 当读取数据队列满了后进入休眠时，可以通过改condition唤醒线程
    SDL_cond* continue_read_thread;
};

ShowMode show_mode = SHOW_MODE_NONE;

#endif  // __MY_STRUCT_H__