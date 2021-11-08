cmake_minimum_required(VERSION 3.5)

project(WPlayer LANGUAGES CXX)

include_directories(~/ffmpeg_build/include)
link_directories(~/ffmpeg_build/lib)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

add_executable(WPlayer main.cpp)
target_link_libraries(WPlayer libavcodec.a libavdevice.a libavfilter.a libavformat.a libavutil.a libpostproc.a libswresample.a libswscale.a)
