#include <vespercsi/plugin.h>
#include <vespercsi/config.h>


vespercsi_plugin_t vespercsi_plugin = {0};

void vespercsi_plugin_load(void){
    if(vespercsi_plugin.set == false){
        return;
    }

    // RTLD_NOW  — resolve all symbols immediately, fail fast if missing
    // RTLD_LOCAL — don't pollute global symbol namespace
    void* handle = dlopen(vespercsi_plugin.path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stdout, "VesperCSI: Plugin load failed: %s", dlerror());
        exit(EXIT_FAILURE);
    }

    // Clear any existing dlerror
    dlerror();

    // Required symbol
    vespercsi_plugin_function_callback_t callback = (vespercsi_plugin_function_callback_t)dlsym(handle, "vespercsi_plugin_function_callback");

    const char* err = dlerror();
    if (err) {
        fprintf(stdout, "VesperCSI: Plugin missing required symbol vespercsi_plugin_function_callback: %s", err);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    // Optional symbols — missing is fine
    vespercsi_plugin_function_init_t init = (vespercsi_plugin_function_init_t)dlsym(handle, "vespercsi_plugin_function_init");
    
    err = dlerror();
    if (err) {
        fprintf(stdout, "VesperCSI: Plugin missing required symbol vespercsi_plugin_function_init: %s", err);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    vespercsi_plugin_function_destroy_t destroy = (vespercsi_plugin_function_destroy_t)dlsym(handle, "vespercsi_plugin_function_destroy");
    
    err = dlerror();
    if (err) {
        fprintf(stdout, "VesperCSI: Plugin missing required symbol vespercsi_plugin_function_destroy: %s", err);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    vespercsi_plugin.handle  = handle;
    vespercsi_plugin.callback = callback;
    vespercsi_plugin.init    = init;
    vespercsi_plugin.destroy = destroy;
    vespercsi_plugin.loaded  = true;

    // Call init if the plugin provided one
    if (vespercsi_plugin.init){
        vespercsi_plugin_context_t new_context = {
            .sdk_version = VESPERCSI_PLUGIN_SDK_VERSION,
            .log.function = vespercsi_log,
            .log.verbose = vespercsi_options.verbose,
            .log.verbosity = vespercsi_options.verbosity
        };

        vespercsi_plugin.init(&new_context);
    }

    vespercsi_plugin.loaded = true;
    return;
}


void vespercsi_plugin_unload(void) {
    if (!vespercsi_plugin.loaded) return;

    if (vespercsi_plugin.destroy) {
        vespercsi_plugin.destroy();
    }

    dlclose(vespercsi_plugin.handle);
    memset(&vespercsi_plugin, 0, sizeof(vespercsi_plugin));
}

