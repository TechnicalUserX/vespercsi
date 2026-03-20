#include <vespercsi/netlink.h>
#include <vespercsi/vespercsi.h>
#include <vespercsi/options.h>
#include <vespercsi/log.h>
#include <vespercsi/wifi.h>
#include <vespercsi/csi.h>
#include <vespercsi/signal.h>
#include <vespercsi/config.h>
#include <vespercsi/plugin.h>

int main(int argc, char* argv[]){

    vespercsi_options_parse(argc, argv);

    vespercsi_options_check_mandatory();

    vespercsi_config_init(&vespercsi_config);

    vespercsi_plugin_load();
    
    vespercsi_wifi_controller_init(&vespercsi_wifi_controller);

    vespercsi_csi_controller_init(&vespercsi_csi_controller);

    LOG(INFO, "VesperCSI Started");

    vespercsi_wifi_get_interfaces(&vespercsi_wifi_controller);
    
    vespercsi_wifi_get_phys(&vespercsi_wifi_controller);

    char interface_mac_string[VESPERCSI_WIFI_MAC_STR_SIZE] = {0};
    vespercsi_wifi_mac_string(interface_mac_string, vespercsi_options.interface_mac);

    bool phy_found = false;
    vespercsi_wifi_phy_t* phy = NULL;

    for(int i = 0; i < vespercsi_wifi_controller.phys_size; i++){
        LOG_VERBOSE(INFO, "Physical Wi-Fi device found on host: %s", vespercsi_wifi_controller.phys[i].name);

        if(vespercsi_wifi_check_phy_mac(vespercsi_wifi_controller.phys[i].name, interface_mac_string)){
            LOG_VERBOSE(INFO, "Found physical Wi-Fi device: %s, Index: %lu", vespercsi_wifi_controller.phys[i].name, vespercsi_wifi_controller.phys[i].index);
            phy_found = true;
            phy = &vespercsi_wifi_controller.phys[i];
            // Remvoe the interface bonded to the phy if present
            for(int j = 0; j < vespercsi_wifi_controller.interfaces_size; j++){
                if(vespercsi_wifi_controller.interfaces[j].wiphy == vespercsi_wifi_controller.phys[i].index){
                    LOG_VERBOSE(INFO, "Removing %s", vespercsi_wifi_controller.interfaces[j].name);
                    vespercsi_wifi_remove_interface(&vespercsi_wifi_controller, vespercsi_wifi_controller.interfaces[j].name, 0);
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

    LOG_VERBOSE(INFO, "Adding %s", VESPERCSI_WIFI_MONITOR_INTERFACE);
    

    vespercsi_wifi_add_device(&vespercsi_wifi_controller, VESPERCSI_WIFI_MONITOR_INTERFACE, phy->index, NL80211_IFTYPE_MONITOR);
    usleep(100);

    vespercsi_wifi_interface_t current_interface = {0};

    do{
        vespercsi_wifi_set_interface_updown(VESPERCSI_WIFI_MONITOR_INTERFACE, true);
        vespercsi_wifi_set_interface_frequency(&vespercsi_wifi_controller, VESPERCSI_WIFI_MONITOR_INTERFACE, vespercsi_options.frequency, vespercsi_options.bandwidth);
        vespercsi_wifi_get_single_interface(&vespercsi_wifi_controller, &current_interface, VESPERCSI_WIFI_MONITOR_INTERFACE);
    }while(current_interface.freq != vespercsi_options.frequency);

    // This may not work in many wifi cards
    vespercsi_wifi_set_tx_power(&vespercsi_wifi_controller, VESPERCSI_WIFI_MONITOR_INTERFACE);

    if(vespercsi_wifi_toggle_iwlwifi(true) == VESPERCSI_FAIL){
        LOG(FAIL, "Cannot enable CSI capturing on the interface %s", VESPERCSI_WIFI_MONITOR_INTERFACE);
        exit(EXIT_FAILURE);
    }

    vespercsi_csi_consume_start();

    vespercsi_csi_capture_start();

    // Command server
    pause();

    return EXIT_SUCCESS;
}
