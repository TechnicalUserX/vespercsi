#include "vespercsi/csi.h"
#include <vespercsi/signal.h>
#include <vespercsi/log.h>
#include <vespercsi/plugin.h>

void vespercsi_signal_init(void){

    struct sigaction sa = {
        .sa_flags = SA_RESTART,
        .sa_handler = vespercsi_signal_sigint_handler
    };

    sigaction(SIGINT, &sa, NULL);
}


void vespercsi_signal_sigint_handler(int s){

    if(vespercsi_log_file){
        fclose(vespercsi_log_file);
    }

    if(vespercsi_options.out_file_set){
        close(vespercsi_csi_controller.vespercsi_csi_out_file);
    }

    vespercsi_plugin_unload();

    exit(EXIT_SUCCESS);
}