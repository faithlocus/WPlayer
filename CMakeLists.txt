cmake_minimum_required(VERSION 3.5)

project(WPlayer LANGUAGES CXX)

# c++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rd)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/3rd/slog/ SRC_TRD)

set(SRC_LIST main.cpp)

# ffmpeg
IF (UNIX)
link_directories(~/ffmpeg_build/lib)
include_directories(~/ffmpeg_build/include)
# gdb
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
ELSEIF(WIN32)
include_directories(./ffmpeg-4.2.1-win32-dev/include/)
link_directories(./ffmpeg-4.2.1-win32-dev/lib/)
ENDIF()


foreach(mainfile IN LISTS SRC_LIST)
    get_filename_component(mainname ${mainfile} NAME_WE)
    add_executable(${mainname} ${mainfile} ${SRC_TRD})
    message(STATUS "${mainname} ${mainfile} ${SRC_TRD}")
    IF(UNIX)
    target_link_libraries(${mainname} libSDL2.a)

    target_link_libraries(${mainname} libavformat.a)

    target_link_libraries(${mainname} libavcodec.a)

    target_link_libraries(${mainname} libopus.a)
    target_link_libraries(${mainname} libx264.a)
    target_link_libraries(${mainname} libx265.a)
    target_link_libraries(${mainname} libvpx.a)
    target_link_libraries(${mainname} libfdk-aac.a)
    target_link_libraries(${mainname} libmp3lame.a)

    target_link_libraries(${mainname} libswresample.a)
    target_link_libraries(${mainname} libswscale.a)
    target_link_libraries(${mainname} libavutil.a)

    target_link_libraries(${mainname} vorbisenc vorbis pthread m z rt numa va)
    target_link_libraries(${mainname} dl)
    target_link_libraries(${mainname} va va-drm va-x11 vdpau X11 Xv Xext)

    ELSEIF(WIN32)
    target_link_libraries(${mainname} avformat.lib
        swresample.lib
        swscale.lib
        avcodec.lib
        avutil.lib )

    ENDIF()
endforeach()
