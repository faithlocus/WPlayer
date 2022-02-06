#include <iostream>

extern "C" {
#include "SDL2/SDL.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
}

#include "3rd/log/log.h"

int main(int argc, char **argv) {

    init_logger("./log/", S_TRACE);

    const char *file_path = "2_audio_track_5s.mp4";
    //    const char *     file_path = "cctv1.flv";
    AVFormatContext *ctx_fmt{ nullptr };

    int ret = avformat_open_input(&ctx_fmt, file_path, NULL, NULL);
    if (ret < 0) {
        //        printf("avformat_open_input failed");
        eLog("%s", "avformat_open_input failed");
        return -1;
    }

    ret = avformat_find_stream_info(ctx_fmt, NULL);
    if (ret < 0) {
        printf("avformat_find_stream_info failed");
        return -1;
    }

    int index_video = av_find_best_stream(ctx_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (AVERROR_STREAM_NOT_FOUND == index_video) {
        printf("video stream not found");
        return -1;
    }

    //    auto ctx_codec = ctx_fmt->streams[index_video]->codec;
    //    auto id_codec  = ctx_codec->codec_id;

    //    auto codec = avcodec_find_decoder(id_codec);
    auto codec = avcodec_find_decoder(ctx_fmt->streams[index_video]->codecpar->codec_id);
    if (!codec) {
        printf("codec not found");
        return -1;
    }
    auto ctx_codec = avcodec_alloc_context3(codec);
    if (!ctx_codec) {
        printf("avcodec_alloc_context3 failed");
        return -1;
    }

    ret = avcodec_open2(ctx_codec, codec, NULL);
    if (ret < 0) {
        printf("could not open codec");
        return -1;
    }

    AVPacket *pkt = av_packet_alloc();

    while (true) {
        ret = av_read_frame(ctx_fmt, pkt);
        if (ret < 0) {
            printf("av_read_frame failed");
            break;
        }

        if (index_video == pkt->stream_index) {
            printf("video pts:%lld\n", pkt->pts);
            printf("video dts:%lld\n", pkt->dts);
            printf("video size:%lld\n", pkt->size);
            printf("video pos:%lld\n", pkt->pos);
            printf("video duration:%lf\n\n",
                   pkt->duration * av_q2d(ctx_fmt->streams[index_video]->time_base));
        }
    }

    return 0;
}
