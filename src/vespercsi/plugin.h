#ifndef VESPERCSI_PLUGIN_H
#define VESPERCSI_PLUGIN_H

#include <vespercsi/vespercsi.h>
#include <vespercsi/csi.h>
#include <vespercsi/log.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>

#define VESPERCSI_PLUGIN_SDK_VERSION 1

typedef void (*vespercsi_plugin_function_log_t)(vespercsi_log_level_t level, const char* format, ...);

typedef struct vespercsi_plugin_log_t{
    vespercsi_plugin_function_log_t function;
    bool verbose;
    uint8_t verbosity;
}vespercsi_plugin_log_t;

typedef struct vespercsi_plugin_context_t{
    uint64_t sdk_version;
     vespercsi_plugin_log_t log;
}vespercsi_plugin_context_t;


// Obligatory Function
typedef void (*vespercsi_plugin_function_callback_t)(const vespercsi_csi_t* csi);
typedef void (*vespercsi_plugin_function_init_t)(vespercsi_plugin_context_t* context);
typedef void (*vespercsi_plugin_function_destroy_t)(void);

typedef struct vespercsi_plugin_t {
    bool set;
    char path[512];
    void*                                   handle;      // dlopen handle
    vespercsi_plugin_function_callback_t    callback;
    vespercsi_plugin_function_init_t        init;
    vespercsi_plugin_function_destroy_t     destroy;
    bool                                    loaded;
}vespercsi_plugin_t;

extern vespercsi_plugin_t vespercsi_plugin;

void  vespercsi_plugin_load(void);
void vespercsi_plugin_unload(void);

#endif
