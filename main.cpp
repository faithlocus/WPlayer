#include <iostream>
extern "C"
{
#include "libavcodec/avcodec.h"
}

int main()
{
    std::cout << "Hello FFmpeg" << std::endl;
    auto version = avcodec_version();
    std::cout << "version is:" << version << std::endl;

    return 0;
}
