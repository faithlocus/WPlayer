#include "cmdutils.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif 

#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"

#ifdef __cplusplus
}
#endif //__cplusplus

// 动态加载动态库路径
// TODO(有什么意义): <wangqing@gaugene.com>-<2022-02-07>
void init_dynload(){
#if HAVE_SETDLLDIRECTORY & defined(_WIN32)
  SetDllDirectory("");
#endif
}

static FILE *report_file;
static int init_report(const char *env) {
  char *filename_template = NULL;
  char *key, *val;
  int ret, count = 0;
  int prog_loglevel, envlevel = 0;
  time_t now;
  tm *tm;
  AVBPrint filename;

  if (report_file) // already opend
    return 0;

  time(&now);
  tm = localtime(&now);

  while(env && *env){
      
  }
}

int opt_report(void *optctx, const char *opt, const char *arg){
  return init_report(NULL);
}

static void (*program_exit)(int ret);
void register_exit(void (*cb)(int ret)) { program_exit = cb; }
void exit_program(int ret){
    if (program_exit)
      exit_program(ret);
    exit(ret);
}

static void dump_argument(const char *a) { 
    const unsigned char *p;

    for (p = (const unsigned char*)a; *p; ++p){
        if (!((/*连接符与数字*/*p >= '+' && *p <= ':') || (/*大写字母*/*p >= '@' && *p <= 'Z') ||
               /*连接符*/*p == '_' || (/*小写字母*/*p >= 'a' && *p <= 'z')))
          break;
    }

    if (!*p){
      fputs(a, report_file);
      return;
    }

    fputc('"', report_file);
    for (p = a; *p; ++p){
        if (*p == '\\' || *p == '"' || *p == '$' || *p == '`')
          fprintf(report_file, "\\%c", *p);
        else if(*p < ' ' || *p > '~')
          fprintf(report_file, "\\x%02x", *p);
        else
          fputc(*p, report_file);
    }
    fputc('"', report_file);
}

int opt_loglevel(void *optctx, const char *opt, const char *arg){
  const struct {
    const char *name;
    int level;
  } log_levels[] = {
      { "quiet",     AV_LOG_QUIET},
      { "panic",     AV_LOG_PANIC},
      { "fatal",     AV_LOG_FATAL},
      { "error",     AV_LOG_ERROR},
      { "warning",   AV_LOG_WARNING},
      { "info",      AV_LOG_INFO},
      { "verbose",   AV_LOG_VERBOSE},
      { "debug",     AV_LOG_DEBUG},
      { "trace",     AV_LOG_TRACE},
  };

  av_assert0(arg);

  const char *token;
  char *tail;
  int flags = av_log_get_flags();
  int level = av_log_get_level();
  int cmd, i = 0;

  while(*arg){
    token = arg;
    if (*token == '+' || *token == '-'){
      cmd = *token++;
    }else{
      cmd = 0;
    }

    if (!i && !cmd){
      flags = 0;
    }

    if (!strncmp(token, "repeat", 6)){
        if (cmd == '-'){
          flags |= AV_LOG_SKIP_REPEATED;
        }else{
          flags &= !AV_LOG_SKIP_REPEATED;
        }
    }else if (!strncmp(token, "level", 5)){
        if (cmd == '-'){
          flags &= ~AV_LOG_SKIP_REPEATED;
        }else{
          flags |= AV_LOG_PRINT_LEVEL;
        }
        arg = token + 5;
    }else{
      break;
    }
    ++i;
  }

  if (!*arg){
    goto end;
  }else if(*arg == '+'){
    arg++;
  }else if (!i){
    flags = av_log_get_flags();
  }

  for (i = 0; i < FF_ARRAY_ELEMS(log_levels); ++i){
      if (!strcmp(log_levels[i].name, arg)){
        level = log_levels[i].level;
        goto end;
      }
  }

  level = strtol(arg, &tail, 10);
  if (*tail){
    av_log(NULL, AV_LOG_FATAL, "Invalid loglevel \"%s\". Possible levels are numbers or:\n", arg);
    for (i = 0; i < FF_ARRAY_ELEMS(log_levels); ++i) {
      av_log(NULL, AV_LOG_FATAL, "\%s\"\n", log_levels[i].name);
    }
    exit_program(1);
  }

  end:
    av_log_set_flags(flags);
    av_log_set_level(level);
    return 0;
}

// 判断参数输入是否合理 
static void check_options(const OptionDef *po){
    while(po->name){
        if (po->flag & OPT_PREFILE)  // 只有ffmpeg存在此参数
          av_assert0(po->flag & (OPT_INPUT | OPT_OUTPUT));
        po++;
    }
}

// 获取指定的OptionDef,失败返回原option,成功返回指定的OptionDef
static const OptionDef *find_option(const OptionDef *po, const char *name){
    while(po->name){
      const char *end;
      if (av_strstart(name, po->name, &end) && (!*end || *end == ':'))
        break;
      po++;
    }
    return po;
}

// 定位指定参数，0表示未找到，找到则返回（argv的）指定index(>0)
static int locate_option(int argc, 
                         const char **argv, 
                         const OptionDef *options, 
                         const char *optname){
  for (int i = 1; i < argc; ++i) {
    const char *cur_opt = argv[i];
    if (*cur_opt ++ != '-')
      continue;

    const OptionDef *po = find_option(options, cur_opt); 
    if (!po->name && cur_opt[0] == 'n' && cur_opt[1] = 'o')
      po = find_option(options, cur_opt + 2);

    if ((!po->name && !strcmp(cur_opt, optname)) ||
         (po->name && !strcmp(cur_opt, po->name)))
      return i;
    
    if (!po->name || po->flag & HAS_ARG)
      i++;
  }
  return 0;
}

void parse_loglevel(int argc, const char **argv, const OptionDef *options){

    check_options(options);
    int idx = locate_option(argc, argv, options, "loglevel"); 
    if (!idx)
      idx = locate_option(argc, argv, options, "v");
    if (idx && argv[idx + 1])
      opt_loglevel(NULL, "loglevel", argv[idx + 1]); 

    idx = locate_option(argc, argv, options, "report");
    const char *env;
    if ((env = getenv("FFREPORT")) || idx){       // 要求打印参数，环境变量 || 传入参数
      init_report(env); 
      if (report_file){
        fprintf(report_file, "command line:\n");
        for (int i = 0; i < argc; ++i){
          dump_argument(argv[i]); 
          fputc(i < argc - 1 ? ' ' : '\n', report_file);
        }
        fflush(report_file);
      }
    }

    idx = locate_option(argc, argv, options, "hide_banner");
    if (idx)
      hide_banner = 1;
}