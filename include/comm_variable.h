#ifndef __COMM_VARIABLE_H__
#define __COMM_VARIABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libavutil/common.h"
#include "SDL_events.h"

#ifdef __cplusplus
}
#endif  //__cplusplus

#define FF_QUIT_EVENT (SDL_USEREVENT + 2)

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define SAMPLE_ARRAY_SIZE (8 * 65535)

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9

#define FRAME_QUEUE_SIZE     \
    FFMAX(SAMPLE_QUEUE_SIZE, \
          FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

#define SDL_AUDIO_MIN_BUFFER_SIZE 512
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30
#define SDL_VOLUME_STEP (0.75)
#define CURSOR_HIDE_DELAY 1000000
#define REFRESH_RATE 0.01

const char* input_filename;
int         display_disable;

int video_disable;
int audio_disable;
int subtitle_disable;
int alwaysontop;
int startup_volume = 100;
int show_status    = -1;
int borderless;
int genpts = 0;
int loop   = 1;
int autoexit;
int64_t pkt_ts;

int                  default_width  = 640;
int                  default_height = 480;
int                  av_sync_type   = AV_SYNC_AUDIO_MASTER;
const AVInputFormat* file_iformat;

int scan_all_pmts_set = 0;
int find_stream_info  = 1;
int seek_by_bytes     = -1;
const char* window_title;

int64_t start_time = AV_NOPTS_VALUE;
int64_t duration   = AV_NOPTS_VALUE;
const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = {0};
int         infinite_buffer                    = -1;
int         lowres                              = 0; // brief(wangqing): lowres低分辨率

const char* audio_codec_name;
const char* video_codec_name;
const char* subtitle_codec_name;
int64_t     cursor_last_shown;
int         cursor_hidden = 0;
int decoder_reorder_pts = -1;

#endif  // __COMM_VARIABLE_H__
