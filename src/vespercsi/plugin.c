#include <vespercsi/plugin.h>
#include <vespercsi/config.h>

vespercsi_plugin_t vespercsi_plugin = {0};

void vespercsi_plugin_load(void){
    if(vespercsi_config.plugin.path_set == false){
        return;
    }

    // RTLD_NOW  — resolve all symbols immediately, fail fast if missing
    // RTLD_LOCAL — don't pollute global symbol namespace
    void* handle = dlopen(vespercsi_config.plugin.path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stdout, "VesperCSI: Plugin load failed: %s", dlerror());
        exit(EXIT_FAILURE);
    }

    // Clear any existing dlerror
    dlerror();

    // Required symbol
    vespercsi_plugin_function_main callback = 
        (vespercsi_plugin_function_main)dlsym(handle, "vespercsi_plugin_consume");

    const char* err = dlerror();
    if (err) {
        fprintf(stdout, "VesperCSI: Plugin missing required symbol vespercsi_plugin_consume: %s", err);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    // Optional symbols — missing is fine
    vespercsi_plugin_function_init init = 
        (vespercsi_plugin_function_init)dlsym(handle, "vespercsi_plugin_init");
    dlerror(); // clear error whether found or not

    vespercsi_plugin_function_destroy destroy = 
        (vespercsi_plugin_function_destroy)dlsym(handle, "vespercsi_plugin_destroy");
    dlerror();

    vespercsi_plugin.handle  = handle;
    vespercsi_plugin.callback = callback;
    vespercsi_plugin.init    = init;
    vespercsi_plugin.destroy = destroy;
    vespercsi_plugin.loaded  = true;

    // Call init if the plugin provided one
    if (vespercsi_plugin.init) {
        vespercsi_plugin.init();
    }

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

