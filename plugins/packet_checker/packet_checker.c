#include <vespercsi/plugin.h>

vespercsi_plugin_context_t vespercsi_plugin_context;

int vespercsi_plugin_function_init(vespercsi_plugin_context_t* context){
    vespercsi_plugin_context = *context;
    LOG(INFO, "packet_checker: Plugin initalized");
    return VESPERCSI_OK;
}

int vespercsi_plugin_function_callback(const vespercsi_csi_t* csi){

    static uint64_t csi_counter = 0;
    static bool flag=false;

    if(flag){
        return VESPERCSI_OK;
    }

    if(csi_counter == 100){
        LOG(INFO, "CSI PACKET COUNT REACHED: 100");
        flag=true;
        return VESPERCSI_OK;
    }

    csi_counter++;
    return VESPERCSI_OK;
}

int vespercsi_plugin_function_destroy(void){
    LOG(INFO, "packet_checker: Plugin destroyed");
    return VESPERCSI_OK;
}
