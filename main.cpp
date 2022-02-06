#include <stdio.h>
#include "slog/slog.h"
#include "my_struct.h"

int main(int argc, char const *argv[])
{
    init_logger("log", _slog_level::S_DEBUG);
    for(int i = 0; i < 10; i++)
    {
        dLog("slog hello world");
    }
    
    return 0;
}
