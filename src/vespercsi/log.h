#ifndef VESPERCSI_LOG_H
#define VESPERCSI_LOG_H

// DO NOT USE THE LOG FUNCTION INSIDE CAPTURE AND CONSUME THREADS

#include <vespercsi/vespercsi.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <vespercsi/options.h>

#define VESPERCSI_LOG_COLOR_RED            "\033[31m"
#define VESPERCSI_LOG_COLOR_GREEN          "\033[32m"
#define VESPERCSI_LOG_COLOR_YELLOW         "\033[33m"
#define VESPERCSI_LOG_COLOR_BLUE           "\033[34m"
#define VESPERCSI_LOG_COLOR_MAGENTA        "\033[35m"
#define VESPERCSI_LOG_COLOR_CLEAR          "\033[0m"

typedef enum vespercsi_log_level_t{
    VESPERCSI_LOG_LEVEL_FAIL = 0,
    VESPERCSI_LOG_LEVEL_WARN,
    VESPERCSI_LOG_LEVEL_INFO,
    VESPERCSI_LOG_LEVEL_PASS,
    VESPERCSI_LOG_LEVEL_DEBG
}vespercsi_log_level_t;

#define FAIL VESPERCSI_LOG_LEVEL_FAIL
#define WARN VESPERCSI_LOG_LEVEL_WARN
#define INFO VESPERCSI_LOG_LEVEL_INFO
#define PASS VESPERCSI_LOG_LEVEL_PASS
#define DEBG VESPERCSI_LOG_LEVEL_DEBG

#define LOG(level, format, ...) {vespercsi_log(level, format, ##__VA_ARGS__);}

#define LOG_VERBOSE(level, format, ...) if(vespercsi_options.verbose){vespercsi_log(level, format, ##__VA_ARGS__);}

extern FILE* vespercsi_log_file;

void vespercsi_log(vespercsi_log_level_t log_level, const char* format, ...);

#endif
