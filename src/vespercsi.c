
#include "vespercsi/netlink.h"
#include <vespercsi/vespercsi.h>
#include <vespercsi/options.h>
#include <vespercsi/log.h>
#include <vespercsi/wifi.h>

int main(int argc, char* argv[]){

    vespercsi_options_parse(argc, argv);

    vespercsi_netlink_init(&vespercsi_wifi_controller.netlink);

    // vespercsi_wifi_interface_t interface = {0};

    vespercsi_wifi_get_interfaces(&vespercsi_wifi_controller.netlink);

    vespercsi_wifi_get_phys(&vespercsi_wifi_controller.netlink);

    // 74:df:bf:bf:dd:dc

    for(int i = 0; i < vespercsi_wifi_controller.phys_size; i++){
        printf("Physical: %s\n", vespercsi_wifi_controller.phys[i].name);
    }

        for(int i = 0; i < vespercsi_wifi_controller.interfaces_size; i++){
        printf("Interface: %s\n", vespercsi_wifi_controller.interfaces[i].name);
    }

    return EXIT_SUCCESS;
}
