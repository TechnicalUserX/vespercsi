#ifndef VESPERCSI_PLUGIN_H
#define VESPERCSI_PLUGIN_H

#include <vespercsi/vespercsi.h>
#include <vespercsi/csi.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>

// Obligatory Function
typedef void (*vespercsi_plugin_function_main)(const vespercsi_csi_t* csi);

// Optional Functions
typedef void (*vespercsi_plugin_function_init)(void);
typedef void (*vespercsi_plugin_function_destroy)(void);

typedef struct vespercsi_plugin_t {
    void*                               handle;      // dlopen handle
    vespercsi_plugin_function_main      callback;     // required
    vespercsi_plugin_function_init      init;        // optional
    vespercsi_plugin_function_destroy   destroy;     // optional
    bool                                loaded;
}vespercsi_plugin_t;

extern vespercsi_plugin_t vespercsi_plugin;

void  vespercsi_plugin_load(void);
void vespercsi_plugin_unload(void);

#endif