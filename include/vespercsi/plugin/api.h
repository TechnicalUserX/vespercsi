#ifndef VESPERCSI_PLUGIN_API
#define VESPERCSI_PLUGIN_API

#include <vespercsi/plugin/types.h>

typedef int (*vespercsi_plugin_api_callback_t)(const vespercsi_csi_t* csi);

typedef int (*vespercsi_plugin_api_init_t)(vespercsi_plugin_context_t* context);

typedef int (*vespercsi_plugin_api_destroy_t)(void);

typedef void (*vespercsi_plugin_api_log_t)(vespercsi_log_level_t level, const char* format, ...);


#endif
