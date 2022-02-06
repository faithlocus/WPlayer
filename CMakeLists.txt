cmake_minimum_required(VERSION 3.5)

project(WPlayer LANGUAGES CXX)

# ffmpeg
link_directories(~/ffmpeg_build/lib)
include_directories(~/ffmpeg_build/include)

# c++17
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# gdb
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(SRC_LIST main.cpp)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rd)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/3rd/slog/ SRC_TRD)

add_executable(${PROJECT_NAME} ${SRC_LIST} ${SRC_TRD})

target_link_libraries(${PROJECT_NAME} libSDL2.a)

target_link_libraries(${PROJECT_NAME} libavformat.a)

target_link_libraries(${PROJECT_NAME} libavcodec.a)

target_link_libraries(${PROJECT_NAME} libopus.a)
target_link_libraries(${PROJECT_NAME} libx264.a)
target_link_libraries(${PROJECT_NAME} libx265.a)
target_link_libraries(${PROJECT_NAME} libvpx.a)
target_link_libraries(${PROJECT_NAME} libfdk-aac.a)
target_link_libraries(${PROJECT_NAME} libmp3lame.a)

target_link_libraries(${PROJECT_NAME} libavutil.a)
target_link_libraries(${PROJECT_NAME} libswresample.a)
target_link_libraries(${PROJECT_NAME} libswscale.a)

target_link_libraries(${PROJECT_NAME} vorbisenc vorbis pthread m z rt numa va)
target_link_libraries(${PROJECT_NAME} dl)
target_link_libraries(${PROJECT_NAME} va va-drm va-x11 vdpau X11 Xv Xext)

