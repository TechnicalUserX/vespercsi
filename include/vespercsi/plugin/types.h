#ifndef VESPERCSI_PLUGIN_TYPES_H
#define VESPERCSI_PLUGIN_TYPES_H

#include <vespercsi/csi/types.h>
#include <vespercsi/log/level.h>

#include <stdbool.h>
#include <stdint.h>

typedef void (*vespercsi_plugin_function_log_t)(vespercsi_log_level_t level, const char* format, ...);

typedef struct vespercsi_plugin_log_t{
    vespercsi_plugin_function_log_t function;
    bool verbose;
    uint8_t verbosity;
}vespercsi_plugin_log_t;

typedef struct vespercsi_plugin_config_t{
    bool set;
    char path[512];
}vespercsi_plugin_config_t;

typedef struct vespercsi_plugin_context_t{
    uint64_t sdk_version;
    vespercsi_plugin_log_t log;
    vespercsi_plugin_config_t config;
}vespercsi_plugin_context_t;

#endif
