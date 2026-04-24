#ifndef VESPERCSI_LOG_H
#define VESPERCSI_LOG_H

// DO NOT USE THE LOG FUNCTION INSIDE CAPTURE AND CONSUME THREADS WHILE IN REAL USAGE, ONLY FOR DEBUGGING

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

#define VERBOSITY_0 0
#define VERBOSITY_1 1
#define VERBOSITY_2 2

#define LOG(level, format, ...) {vespercsi_log(level, format, ##__VA_ARGS__);}

extern bool vespercsi_log_verbose;
extern int vespercsi_log_verbosity;

#define LOG_VERBOSE(verbosity_index, level, format, ...) if(vespercsi_log_verbose && verbosity_index <= vespercsi_log_verbosity){vespercsi_log(level, format, ##__VA_ARGS__);}

extern FILE* vespercsi_log_file;

void vespercsi_log_init(void);

void vespercsi_log_destroy(void);

void vespercsi_log(vespercsi_log_level_t log_level, const char* format, ...);


#endif
