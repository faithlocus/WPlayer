#ifndef __COMM_VARIABLE_H__
#define __COMM_VARIABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libavutil/common.h"

#ifdef __cplusplus
}
#endif  //__cplusplus

#define SAMPLE_ARRAY_SIZE (8 * 65535)

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9

#define FRAME_QUEUE_SIZE     \
    FFMAX(SAMPLE_QUEUE_SIZE, \
          FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

const char* input_filename;
int         display_disable;

int video_disable;
int audio_disable;
int subtitle_disable;
int alwaysontop;
int borderless;

int                  default_width  = 640;
int                  default_height = 480;
const AVInputFormat* file_iformat;

int show_status = -1;

#endif  // __COMM_VARIABLE_H__
