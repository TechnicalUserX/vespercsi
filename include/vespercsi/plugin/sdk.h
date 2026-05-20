#ifndef VESPERCSI_PLUGIN_H
#define VESPERCSI_PLUGIN_H

// ================================================================
// VesperCSI Plugin SDK
// 
// To create a plugin:
// 1. Include this header
// 2. Implement vespercsi_plugin_api_callback(), vespercsi_plugin_api_init() and vespercsi_plugin_api_destroy()
// 4. Compile as a shared library:
//    gcc -shared -fPIC -o my_plugin.so my_plugin.c
// ================================================================

#include <vespercsi/csi/defs.h>
#include <vespercsi/csi/types.h>
#include <vespercsi/log/level.h>
#include <vespercsi/log/verbosity.h>
#include <vespercsi/plugin/defs.h>
#include <vespercsi/plugin/compat.h>
#include <vespercsi/plugin/types.h>
#include <vespercsi/plugin/api.h>

// Information handshaked between main software and plugin
extern vespercsi_plugin_context_t vespercsi_plugin_context;

#define LOG(level, format, ...) {vespercsi_plugin_context.log.function(level, format, ##__VA_ARGS__);}

#define LOG_VERBOSE(verbosity_index, level, format, ...) if(vespercsi_plugin_context.log.verbose && verbosity_index <= vespercsi_plugin_context.log.verbosity ){vespercsi_plugin_context.log.function(level, format, ##__VA_ARGS__);}

#endif
