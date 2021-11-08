TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += /home/fl/ffmpeg_build/include

HEADERS += config.h \
           cmdutils.h

SOURCES += \
        ffplay.c \
        main.cpp

LIBS +=     \
    /usr/lib/x86_64-linux-gnu/libvorbis.a \
    /usr/lib/x86_64-linux-gnu/libvorbisenc.a \
    /usr/lib/x86_64-linux-gnu/libvorbisfile.a \
    /usr/lib/x86_64-linux-gnu/libogg.a \
    /home/fl/ffmpeg_build/lib/libavformat.a \
    /home/fl/ffmpeg_build/lib/libavcodec.a \
    /home/fl/ffmpeg_build/lib/libavdevice.a \
    /home/fl/ffmpeg_build/lib/libavfilter.a \
    /home/fl/ffmpeg_build/lib/libavutil.a \
    /home/fl/ffmpeg_build/lib/libswresample.a \
    /home/fl/ffmpeg_build/lib/libswscale.a \
    /home/fl/ffmpeg_build/lib/libmp3lame.a \
    /home/fl/ffmpeg_build/lib/libfdk-aac.a    \
    /home/fl/ffmpeg_build/lib/libopus.a    \
    /home/fl/ffmpeg_build/lib/libvpx.a \
    /home/fl/ffmpeg_build/lib/libx265.a    \
    /home/fl/ffmpeg_build/lib/libx264.a

LIBS += -lz -ldl -lpthread -lX11 -lm -lvdpau -lva -lva-drm -lva-x11 -lnuma -lvorbis -lvorbisenc
