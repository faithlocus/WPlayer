#ifndef __CMD_UTILS_H__ 
#define __CMD_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif 
#include <stdint.h>

#ifdef __cplusplus
}
#endif //__cplusplus

struct SpecifierOpt {
  char *specifier;
  union {
    uint8_t *str;
    int i;
    int64_t i64;
    uint64_t ui64;
    float f;
    double dbl;
  } u;
};


struct OptionDef {
  const char *name;
  int flag;
#define HAS_ARG         0x0001
#define OPT_BOOL        0x0002
#define OPT_EXPERT      0x0004
#define OPT_STRING      0x0008
#define OPT_VIDEO       0x0010
#define OPT_AUDIO       0x0020
#define OPT_INT         0x0080
#define OPT_FLOAT       0x0100
#define OPT_SUBTITLE    0x0200
#define OPT_INT64       0x0400
#define OPT_EXIT        0x0800
#define OPT_DATA        0x1000
#define OPT_PREFILE     0x2000
#define OPT_OFFSET      0x4000
#define OPT_SPEC        0x8000
#define OPT_TIME        0x10000
#define OPT_DOUBLE      0x20000
#define OPT_INPUT       0x40000
#define OPT_OUTPUT      0x80000
    union{
      void *dst_ptr;
      int (*func_arg)(void *, const char*, const char*);
      size_t off;
    } u;
    const char *help;
    const char *argname;
};

#if CONIF_AVDEVICE
#define CMDUTILS_COMMON_OPTIONS_AVDEVICE                                            \
    { "source"      , OPT_EXIT | HAS_ARG,   {.func_arg = show_sources},             \
      "list sources of the input device", "device"},                                \
    { "sinks"       , OPT_EXIT | HAS_ARG,   {.func_arg = show_sinks},               \
      "list sinks of the output device",  "device"},
#else 
#define CMDUTILS_COMMON_OPTIONS_AVDEVICE
#endif

#define CMDUTILS_COMMON_OPTIONS                                                                                         \
    { "L",              OPT_EXIT,               {.func_arg = show_license},     "show license"},                        \
    { "h",              OPT_EXIT,               {.func_arg = show_help},        "show help", "topic"},                  \
    { "?",              OPT_EXIT,               {.func_arg = show_help},        "show help", "topic"},                  \
    { "help",           OPT_EXIT,               {.func_arg = show_help},        "show help", "topic"},                  \
    { "-help",          OPT_EXIT,               {.func_arg = show_help},        "show help", "topic"},                  \
    { "version",        OPT_EXIT,               {.func_arg = show_version},     "show available version"},              \
    { "buildconf",      OPT_EXIT,               {.func_arg = show_buildconf},   "show available buildconf"},            \
    { "formats",        OPT_EXIT,               {.func_arg = show_formats},     "show available formats"},              \
    { "muxers",         OPT_EXIT,               {.func_arg = show_muxers},      "show available muxers"},               \
    { "demuxers",       OPT_EXIT,               {.func_arg = show_demuxers},    "show available demuxers"},             \
    { "devices",        OPT_EXIT,               {.func_arg = show_devices},     "show available devices"},              \
    { "codecs",         OPT_EXIT,               {.func_arg = show_codecs},      "show available codecs"},               \
    { "decoders",       OPT_EXIT,               {.func_arg = show_decoders},    "show available decoders"},             \
    { "encoders",       OPT_EXIT,               {.func_arg = show_encoders},    "show available encoders"},             \
    { "bsfs",           OPT_EXIT,               {.func_arg = show_bsfs},        "show available bit stream filters"},   \
    { "protocols",      OPT_EXIT,               {.func_arg = show_protocols},   "show available protocols"},            \
    { "filters",        OPT_EXIT,               {.func_arg = show_filters},     "show available filters"},              \
    { "pix_fmts",       OPT_EXIT,               {.func_arg = show_pix_fmts},    "show available pixel formats"},        \
    { "layouts",        OPT_EXIT,               {.func_arg = show_layouts},     "show standard channel layouts"},       \
    { "sample_fmts",    OPT_EXIT,               {.func_arg = show_sample_fmts}, "show available audio sample formats"}, \
    { "colors",         OPT_EXIT,               {.func_arg = show_colors},      "show available color names"},          \
    { "loglevel",       HAS_ARG,                {.func_arg = opt_loglevel},     "show logging level", "loglevel"},      \
    { "v",              HAS_ARG,                {.func_arg = opt_loglevel},     "show logging level", "loglevel"},      \
    { "report",         0,                      {.func_arg = opt_report},       "generate a report"},                   \
    { "max_alloc",      HAS_ARG,                {.func_arg = opt_max_alloc},    "set maximum size of single allocated block", "bytes"},  \
    { "cpuflags",       HAS_ARG | OPT_EXPERT,   {.func_arg = opt_cpuflags},     "force specific cpu flags", "flags"},   \
    { "hide_banner",    OPT_BOOL | OPT_EXPERT,  {&hide_banner},                 "do not show program banner", "hide_banner"},  \
    CMDUTILS_COMMON_OPTIONS_AVDEVICE

void init_opts();
void uninit_opts();

void show_banner(int, const char**, const OptionDef*);

#if CONFIG_AVDEVICE
int show_sinks(void *optctx, const char* opt, const  char *arg);
int show_sources(void *optctx, const char* opt, const  char *arg);
#endif

int show_license(void *optctx, const char* opt, const  char *arg);
int show_help(void *optctx, const char* opt, const  char *arg);
int show_version(void *optctx, const char* opt, const  char *arg);
int show_buildconf(void *optctx, const char* opt, const  char *arg);
int show_formats(void *optctx, const char* opt, const  char *arg);
int show_muxers(void *optctx, const char* opt, const  char *arg);
int show_demuxers(void *optctx, const char* opt, const  char *arg);
int show_devices(void *optctx, const char* opt, const  char *arg);
int show_codecs(void *optctx, const char* opt, const  char *arg);
int show_decoders(void *optctx, const char* opt, const  char *arg);
int show_encoders(void *optctx, const char* opt, const  char *arg);
int show_filters(void *optctx, const char* opt, const  char *arg);
int show_bsfs(void *optctx, const char* opt, const  char *arg);
int show_protocols(void *optctx, const char* opt, const  char *arg);
int show_pix_fmts(void *optctx, const char* opt, const  char *arg);
int show_layouts(void *optctx, const char* opt, const  char *arg);
int show_sample_fmts(void *optctx, const char* opt, const  char *arg);
int show_colors(void *optctx, const char* opt, const  char *arg);

int opt_loglevel(void *optctx, const char *opt, const char *arg);
int opt_report(void *optctx, const char *opt, const char *arg);
int opt_max_alloc(void *optctx, const char *opt, const char *arg);
int opt_cpuflags(void *optctx, const char *opt, const char *arg);

void init_dynload();

void register_exit(void (*cb)(int));
void exit_program(int);
void parse_loglevel(int argc, const char **argv, const OptionDef *options);

void parse_options(void*            optctx,
                   int              argc,
                   const char**     argv,
                   const OptionDef* options,
                   void (*parse_arg_function)(void*, const char*));
#endif // __CMD_UTILS_H__