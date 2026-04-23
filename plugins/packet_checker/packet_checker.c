#include <vespercsi/plugin.h>

vespercsi_plugin_context_t vespercsi_plugin_context;

void vespercsi_plugin_function_init(vespercsi_plugin_context_t* context){
    vespercsi_plugin_context = *context;
    LOG(INFO, "packet_checker: Plugin initalized");
}

void vespercsi_plugin_function_callback(const vespercsi_csi_t* csi){

    static uint64_t csi_counter = 0;
    static bool flag=false;

    if(flag){
        return;
    }

    if(csi_counter == 100){
        LOG(INFO, "CSI PACKET COUNT REACHED: 100");
        flag=true;
        return;
    }

    csi_counter++;

}

void vespercsi_plugin_function_destroy(void){
    LOG(INFO, "packet_checker: Plugin destroyed");
}
