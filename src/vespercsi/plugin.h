#ifndef VESPERCSI_PLUGIN_H
#define VESPERCSI_PLUGIN_H

#include <linux/limits.h>
#include <vespercsi/vespercsi.h>
#include <vespercsi/plugin/types.h>
#include <vespercsi/plugin/api.h>
#include <vespercsi/csi.h>
#include <vespercsi/log.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>
#include <libgen.h>

typedef struct vespercsi_plugin_t {
    bool set;
    char path[PATH_MAX];
    char config_path[PATH_MAX];
    void*                               handle; // dlopen handle
    vespercsi_plugin_api_callback_t     callback;
    vespercsi_plugin_api_init_t         init;
    vespercsi_plugin_api_destroy_t      destroy;
    bool                                loaded;
}vespercsi_plugin_t;

extern vespercsi_plugin_t vespercsi_plugin;

void vespercsi_plugin_load(void);
void vespercsi_plugin_unload(void);

#endif
