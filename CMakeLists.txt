cmake_minimum_required(VERSION 3.5)

project(WPlayer LANGUAGES CXX)

# c++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rd)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/3rd/slog/ SRC_TRD)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/3rd/log/ SRC_TRD_LOG)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/tools SRC_TOOLS)

set(SRC_MAIN main.cpp)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/ SRC_LIST)

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
include_directories(./SDL2-2.0.20/include/)
link_directories(./SDL2-2.0.20/lib/x86/)
ENDIF()


foreach(mainfile IN LISTS SRC_MAIN)
    get_filename_component(mainname ${mainfile} NAME_WE)
    add_executable(${mainname} ${mainfile} ${SRC_LIST} ${SRC_TRD} ${SRC_TRD_LOG} ${SRC_TOOLS})
    message(STATUS "${mainname} ${mainfile}")
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
    target_link_libraries(${mainname} SDL2.lib
        avformat.lib
        swresample.lib
        swscale.lib
        avcodec.lib
        avutil.lib )

    ENDIF()
endforeach()
