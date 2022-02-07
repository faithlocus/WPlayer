#include <stdio.h>
#include "slog/slog.h"
#include "my_struct.h"
#include "tools/cmdutils.h"


int main(int argc, char const *argv[])
{
    init_logger("log", _slog_level::S_DEBUG);

    init_dynload();

    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    int flags;
    MainState *is;



    return 0;
}
