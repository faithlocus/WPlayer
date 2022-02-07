#include "cmdutils.h"

// 动态加载动态库路径
// TODO(有什么意义): <wangqing@gaugene.com>-<2022-02-07>
void init_dynload(){
#if HAVE_SETDLLDIRECTORY & defined(_WIN32)
  SetDllDirectory("");
#endif
}