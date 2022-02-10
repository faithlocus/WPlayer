#ifndef __LOG_H__
#define __LOG_H__

#ifdef __cplusplus
extern "C" {
#endif 
#include "libavutil/log.h"
#ifdef __cplusplus
}
#endif //__cplusplus

#define flog(...) av_log(NULL, AV_LOG_FATAL, __VA_ARGS__)
#define elog(...) av_log(NULL, AV_LOG_ERROR, __VA_ARGS__)
#define wlog(...) av_log(NULL, AV_LOG_WARNING, __VA_ARGS__)
#define ilog(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)
#define dlog(...) av_log(NULL, AV_LOG_DEBUG, __VA_ARGS__)
#define tlog(...) av_log(NULL, AV_LOG_TRACE, __VA_ARGS__)

#endif  // __LOG_H__
