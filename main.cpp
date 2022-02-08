#include <stdio.h>
#include <signal.h>


#include "slog/slog.h"
#include "my_struct.h"
#include "tools/cmdutils.h"


const char program_name[]     = "WPlayer";
const int  program_birth_year = 2000;
const char cc_ident[]         = "gcc 9.1.1 (GCC) 20190807";

extern const OptionDef options[];

static void sigterm_handler(int sig) {
    // todo-start/////////////////////////////////////
    // author: wangqing deadline: 2021/01/01
    // todo-end//////////////////////////////////////////////
    exit(123);
}

int main(int argc, char const *argv[])
{
    init_logger("log", _slog_level::S_DEBUG);

    init_dynload();

    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    parse_loglevel(argc, argv, options);

#if CONFIG_AVDEVICE
    avdevice_register_all();
#else
    avformat_network_init();
#endif

    init_opts();

    signal(SIGINT, sigterm_handler);
    signal(SIGTERM, sigterm_handler);

    show_banner(argc, argv, options);

    int flags;
    MainState *is;



    return 0;
}
