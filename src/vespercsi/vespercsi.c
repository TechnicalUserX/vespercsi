#include <vespercsi/vespercsi.h>
#include <vespercsi/netlink.h>
#include <vespercsi/options.h>
#include <vespercsi/log.h>
#include <vespercsi/wifi.h>
#include <vespercsi/csi.h>
#include <vespercsi/signal.h>
#include <vespercsi/config.h>
#include <vespercsi/plugin.h>

int main(int argc, char* argv[]){

    vespercsi_banner();

    //==========================================================//
    //==================== Option Parse =======================//

    vespercsi_options_parse(argc, argv);

    vespercsi_options_check_mandatory();

    //==========================================================//
    //================== Root Permission Check =================//
    
    if (geteuid() != 0) {
        printf("VesperCSI: Root privileges required to proceed\n");        
        exit(EXIT_FAILURE);
    }

    //==========================================================//
    //==================== Initialization ======================//

    vespercsi_log_init();

    vespercsi_signal_init();

    vespercsi_config_init(&vespercsi_config);

    vespercsi_wifi_controller_init(&vespercsi_wifi_controller);

    vespercsi_csi_controller_init(&vespercsi_csi_controller);

    LOG(INFO, "VesperCSI initialized");

    //==========================================================//
    //====================== Load Plugin =======================//

    vespercsi_plugin_load();

    //==========================================================//
    //================ Interface Configuration =================//

    vespercsi_wifi_get_interfaces(&vespercsi_wifi_controller);
    
    vespercsi_wifi_get_phys(&vespercsi_wifi_controller);

    char interface_mac_string[VESPERCSI_WIFI_MAC_STR_SIZE] = {0};
    vespercsi_wifi_mac_string(interface_mac_string, vespercsi_options.interface_mac);

    bool phy_found = false;
    vespercsi_wifi_phy_t* phy = NULL;

    for(int i = 0; i < vespercsi_wifi_controller.phys_size; i++){
        LOG_VERBOSE(VERBOSITY_1, INFO, "Physical Wi-Fi device found on host: %s", vespercsi_wifi_controller.phys[i].name);

        if(vespercsi_wifi_check_phy_mac(vespercsi_wifi_controller.phys[i].name, interface_mac_string)){
            LOG_VERBOSE(VERBOSITY_0, INFO, "Found physical Wi-Fi device: %s, Index: %lu", vespercsi_wifi_controller.phys[i].name, vespercsi_wifi_controller.phys[i].index);
            phy_found = true;
            phy = &vespercsi_wifi_controller.phys[i];
            // Remvoe the interface bonded to the phy if present
            for(int j = 0; j < vespercsi_wifi_controller.interfaces_size; j++){
                if(vespercsi_wifi_controller.interfaces[j].wiphy == vespercsi_wifi_controller.phys[i].index){
                    LOG_VERBOSE(VERBOSITY_1,INFO, "Removing %s", vespercsi_wifi_controller.interfaces[j].name);
                    vespercsi_wifi_remove_interface(&vespercsi_wifi_controller, vespercsi_wifi_controller.interfaces[j].name, 0);
                    strncpy(vespercsi_wifi_original_interface, vespercsi_wifi_controller.interfaces[j].name, IFNAMSIZ - 1);
                    break;
                }
            }
            break;
        }
    }

    if(!phy_found){
        LOG(FAIL, "Intended physical Wi-Fi device not found on host");
        exit(EXIT_FAILURE);
    }

    LOG_VERBOSE(VERBOSITY_0,INFO, "Adding %s", VESPERCSI_WIFI_MONITOR_INTERFACE);
    
    vespercsi_wifi_add_device(&vespercsi_wifi_controller, VESPERCSI_WIFI_MONITOR_INTERFACE, phy->index, NL80211_IFTYPE_MONITOR);
    usleep(100);

    vespercsi_wifi_interface_t current_interface = {0};

    do{
        vespercsi_wifi_set_interface_updown(VESPERCSI_WIFI_MONITOR_INTERFACE, true);
        vespercsi_wifi_set_interface_frequency(&vespercsi_wifi_controller, VESPERCSI_WIFI_MONITOR_INTERFACE, vespercsi_options.frequency, vespercsi_options.bandwidth);
        vespercsi_wifi_get_single_interface(&vespercsi_wifi_controller, &current_interface, VESPERCSI_WIFI_MONITOR_INTERFACE);
        usleep(100);
    }while(current_interface.freq != vespercsi_options.frequency || current_interface.channel_width != vespercsi_options.channel_width);

    if(vespercsi_wifi_toggle_iwlwifi(true) == VESPERCSI_FAIL){
        LOG(FAIL, "Cannot enable CSI capturing on the interface %s", VESPERCSI_WIFI_MONITOR_INTERFACE);
        exit(EXIT_FAILURE);
    }

    //==========================================================//
    //=================== Start Main Threads ===================//

    vespercsi_csi_consume_start();

    vespercsi_csi_capture_start();

    //==========================================================//
    //========== Wait Until Graceful Shutdown Signal ===========//

    struct pollfd wait_shutdown_fd = {
        .fd = vespercsi_signal_shutdown_pipe[0],
        .events = POLLIN
    };

    while(1){
        int ret = poll(&wait_shutdown_fd, 1, -1);

        if (ret < 0) {
            if (errno == EINTR){
                continue;  // signal interrupted poll, loop again
            }
            exit(EXIT_FAILURE); // Something gone terribly bad
        }

        if (wait_shutdown_fd.revents & POLLIN){
            // Let's perform graceful shutdown
            break;
        }
    }

    //==========================================================//
    
    if(vespercsi_wifi_toggle_iwlwifi(false) == VESPERCSI_FAIL){
        LOG(WARN, "Cannot disable CSI capturing on the interface %s", VESPERCSI_WIFI_MONITOR_INTERFACE);
    }

    vespercsi_csi_capture_stop();

    vespercsi_csi_consume_stop();

    vespercsi_plugin_unload();

    //==========================================================//
    //=================== Interface Revert =====================//

    if(strcmp(vespercsi_wifi_original_interface, "") != 0){

        LOG_VERBOSE(VERBOSITY_1, INFO, "Reverting %s to %s", VESPERCSI_WIFI_MONITOR_INTERFACE, vespercsi_wifi_original_interface);

        vespercsi_wifi_remove_interface(&vespercsi_wifi_controller, VESPERCSI_WIFI_MONITOR_INTERFACE, 0);

        vespercsi_wifi_add_device(&vespercsi_wifi_controller, vespercsi_wifi_original_interface, phy->index, NL80211_IFTYPE_STATION);

    }

    //==========================================================//
    //================= Destroy Controllers ====================//

    vespercsi_csi_controller_destroy(&vespercsi_csi_controller);

    vespercsi_wifi_controller_destroy(&vespercsi_wifi_controller);

    vespercsi_csi_close_file();

    vespercsi_log_destroy();

    vespercsi_signal_destroy();

    return(EXIT_SUCCESS);
}
