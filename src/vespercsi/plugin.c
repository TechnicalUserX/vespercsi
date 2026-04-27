#include <vespercsi/plugin.h>
#include <vespercsi/config.h>

vespercsi_plugin_t vespercsi_plugin = {0};

void vespercsi_plugin_load(void){
    if(vespercsi_plugin.set == false){
        return;
    }

    // RTLD_NOW  — resolve all symbols immediately, fail fast if missing
    // RTLD_LOCAL — don't pollute global symbol namespace
    void* handle = NULL;

    char plugin_path[512] = {0};
    char* plugin_path_dirname = NULL;
    strncpy(plugin_path, vespercsi_plugin.path, sizeof(plugin_path)-1);

    plugin_path_dirname = dirname(plugin_path);

    if(strcmp(plugin_path_dirname, ".") == 0 && strstr(vespercsi_plugin.path, "/") == NULL){
        // Search under /etc/vespercsi/plugins
        char search_path[512] = VESPERCSI_PLUGIN_FOLDER "/";
        strncat(search_path, vespercsi_plugin.path, sizeof(search_path)-1-sizeof(VESPERCSI_PLUGIN_FOLDER));
        handle = dlopen(search_path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            fprintf(stdout, "VesperCSI: Plugin load failed: %s\n", dlerror());
            exit(EXIT_FAILURE);
        }
    }else{
        // Absolute (or relative) path
        handle = dlopen(vespercsi_plugin.path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            fprintf(stdout, "VesperCSI: Plugin load failed: %s\n", dlerror());
            exit(EXIT_FAILURE);
        }
    }

    // Clear any existing dlerror
    dlerror();

    // Required symbol
    vespercsi_plugin_function_callback_t callback = (vespercsi_plugin_function_callback_t)dlsym(handle, "vespercsi_plugin_function_callback");

    const char* err = dlerror();
    if (err) {
        fprintf(stdout, "VesperCSI: Plugin missing required symbol vespercsi_plugin_function_callback: %s\n", err);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    // Optional symbols — missing is fine
    vespercsi_plugin_function_init_t init = (vespercsi_plugin_function_init_t)dlsym(handle, "vespercsi_plugin_function_init");
    
    err = dlerror();
    if (err) {
        fprintf(stdout, "VesperCSI: Plugin missing required symbol vespercsi_plugin_function_init: %s\n", err);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    vespercsi_plugin_function_destroy_t destroy = (vespercsi_plugin_function_destroy_t)dlsym(handle, "vespercsi_plugin_function_destroy");
    
    err = dlerror();
    if (err) {
        fprintf(stdout, "VesperCSI: Plugin missing required symbol vespercsi_plugin_function_destroy: %s\n", err);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    vespercsi_plugin.handle  = handle;
    vespercsi_plugin.callback = callback;
    vespercsi_plugin.init    = init;
    vespercsi_plugin.destroy = destroy;
    vespercsi_plugin.loaded  = true;

    // Call init
    vespercsi_plugin_context_t new_context = {
        .sdk_version = VESPERCSI_PLUGIN_SDK_VERSION,
        .log.function = vespercsi_log,
        .log.verbose = vespercsi_options.verbose,
        .log.verbosity = vespercsi_options.verbosity
    };

    if(strcmp(vespercsi_plugin.config_path, "") != 0){
        strncpy(new_context.config.path, vespercsi_plugin.config_path, sizeof(new_context.config.path) - 1);
        new_context.config.set = true;
    }

    if(vespercsi_plugin.init(&new_context) == VESPERCSI_FAIL){
        fprintf(stdout, "VesperCSI: Plugin cannot be initialized\n");
        dlclose(handle);
        exit(EXIT_FAILURE);
    }else{
        vespercsi_plugin.loaded = true;
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

