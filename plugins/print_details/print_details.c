#include <vespercsi/plugin.h>

vespercsi_plugin_context_t vespercsi_plugin_context;

void vespercsi_plugin_function_init(vespercsi_plugin_context_t* context){
    vespercsi_plugin_context = *context;
    LOG(INFO, "print_details: Plugin initalized");
}

void vespercsi_plugin_function_callback(const vespercsi_csi_t* csi){

    static uint64_t csi_counter = 0;

    csi_counter++;

    uint32_t format = csi->header.rateNflag & VESPERCSI_RATE_MCS_MOD_TYPE_MSK;
    uint32_t channel_width = csi->header.rateNflag & VESPERCSI_RATE_MCS_CHAN_WIDTH_MSK;

    uint32_t channel_width_number = 0;

    char format_str[16] = {0};
    switch(format){
        case VESPERCSI_RATE_MCS_VHT_MSK:
            strcpy(format_str, "VHT");
            break;
        case VESPERCSI_RATE_MCS_HT_MSK:
            strcpy(format_str, "HT");
            break;
        case VESPERCSI_RATE_MCS_HE_MSK:
            strcpy(format_str, "HE");
            break;
        case VESPERCSI_RATE_MCS_LEGACY_OFDM_MSK:
            strcpy(format_str, "Legayc OFDM");
            break;
        case VESPERCSI_RATE_MCS_EHT_MSK:
            strcpy(format_str, "EHT");
            break;
    }

    switch(channel_width){
        case VESPERCSI_RATE_MCS_CHAN_WIDTH_20:
            channel_width_number = 20;
            break;
        case VESPERCSI_RATE_MCS_CHAN_WIDTH_40:
            channel_width_number = 40;
            break;
        case VESPERCSI_RATE_MCS_CHAN_WIDTH_80:
            channel_width_number = 80;
            break;
        case VESPERCSI_RATE_MCS_CHAN_WIDTH_160:
            channel_width_number = 160;
            break;                                    
    }

    LOG(INFO, "print_details: CSI TIME %lu RX: %hhu TX: %hhu SUBCARRIERS: %u DATA SIZE: %u MAC: %02X:%02X:%02X:%02X:%02X:%02X FORMAT: %s BANDWIDTH: %u COUNT: %lu", 
                                                                            csi->header.timestamp,
                                                                            csi->header.num_rx, 
                                                                            csi->header.num_tx, 
                                                                            csi->header.num_subcarriers, 
                                                                            csi->header.csi_data_size, 
                                                                            csi->header.src_mac[0], csi->header.src_mac[1], csi->header.src_mac[2], 
                                                                            csi->header.src_mac[3], csi->header.src_mac[4], csi->header.src_mac[5], 
                                                                            format_str,
                                                                            channel_width_number,
                                                                            csi_counter
                                                                            );
}

void vespercsi_plugin_function_destroy(void){
    LOG(INFO, "print_details: Plugin destroyed");
}
