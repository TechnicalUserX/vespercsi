#include <vespercsi/plugin/sdk.h>

int vespercsi_plugin_api_callback(const vespercsi_csi_t* csi){
    return VESPERCSI_OK;
}

int vespercsi_plugin_api_init(vespercsi_plugin_context_t* context){
    return VESPERCSI_OK;
}

int vespercsi_plugin_api_destroy(void){
    return VESPERCSI_OK;
}
