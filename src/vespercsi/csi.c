#include "netlink/handlers.h"
#include <vespercsi/csi.h>
#include <vespercsi/log.h>
#include <vespercsi/wifi.h>


vespercsi_csi_controller_t vespercsi_csi_controller = {0};

// ======== CSI LISTPOOL RING BUFFERS ======== //

vespercsi_csi_ringbuffer_listpool_t vespercsi_csi_ringbuffer_1 = {0};
vespercsi_csi_ringbuffer_listpool_t vespercsi_csi_ringbuffer_2 = {0};

// ======== CSI LISTPOOL ATOMIC POINTER ======== //

vespercsi_csi_ringbuffer_listpool_t* _Atomic vespercsi_csi_ringbuffer_capture;
vespercsi_csi_ringbuffer_listpool_t* _Atomic vespercsi_csi_ringbuffer_consume;


LISTPOOL_TEMPLATE_DEFINITION(vespercsi_csi_ringbuffer, VESPERCSI_CSI_RINGBUFFER, vespercsi_csi_t, VESPERCSI_CSI_SUPPORTED_RATE_MAX * VESPERCSI_CSI_SUPPORTED_TIME_FRAME_MAX)

uint32_t VESPERCSI_CSI_NO_NHT_20_PILOT_INDICES[] = {5, 19, 32, 46};                                                                                                                                                              // 52 subcarriers
uint32_t VESPERCSI_CSI_HT_VHT_20_PILOT_INDICES[] = {7, 21, 34, 48};                                                                                                                                                              // 56 subcarriers
uint32_t VESPERCSI_CSI_HT_VHT_40_PILOT_INDICES[] = {5, 33, 47, 66, 80, 108};                                                                                                                                                     // 114 subcarriers
uint32_t VESPERCSI_CSI_VHT_80_PILOT_INDICES[] = {19, 47, 83, 111, 130, 158, 194, 222};                                                                                                                                           // 242 subcarriers
uint32_t VESPERCSI_CSI_VHT_160_PILOT_INDICES[] = {19, 47, 83, 111, 130, 158, 194, 222, 261, 289, 325, 353, 372, 400, 436, 464};                                                                                                  // 484 subcarriers
uint32_t VESPERCSI_CSI_HE_20_PILOT_INDICES[] = {6, 32, 74, 100, 141, 167, 209, 235};                                                                                                                                             // 242 subcarriers
uint32_t VESPERCSI_CSI_HE_40_PILOT_INDICES[] = {6, 32, 74, 100, 140, 166, 208, 234, 249, 275, 317, 343, 383, 409, 451, 477};                                                                                                     // 484 subcarriers
uint32_t VESPERCSI_CSI_HE_80_PILOT_INDICES[] = {32, 100, 166, 234, 274, 342, 408, 476, 519, 587, 653, 721, 761, 829, 895, 963};                                                                                                  // 996 subcarriers
uint32_t VESPERCSI_CSI_HE_160_PILOT_INDICES[] = {32, 100, 166, 234, 274, 342, 408, 476, 519, 587, 653, 721, 761, 829, 895, 963, 1028, 1096, 1162, 1230, 1270, 1338, 1404, 1472, 1515, 1583, 1649, 1717, 1757, 1825, 1891, 1959}; // 1992 subcarriers


void* vespercsi_csi_runner_capture(void* arg){



    return NULL;
}

void* vespercsi_csi_runner_consume(void* arg){

    return NULL;
}


int vespercsi_csi_listen(vespercsi_netlink_t* netlink)
{
    vespercsi_netlink_command_t command = {
        .id = NL80211_CMD_VENDOR,
        .idby = VESPERCSI_NETLINK_CIB_NETDEV,
        .nlFlags = 0,
        .device = if_nametoindex(VESPERCSI_WIFI_MONITOR_INTERFACE),
        .handler = vespercsi_csi_listen_handler,
        .valid_handler = vespercsi_csi_capture_handler,
    };

    return vespercsi_netlink_execute(netlink, &command);
}

int vespercsi_csi_listen_handler(struct vespercsi_netlink_t *state, struct nl_msg *msg, void *arg)
{
    NLA_PUT_U32(msg, NL80211_ATTR_VENDOR_ID, 0x001735);
    NLA_PUT_U32(msg, NL80211_ATTR_VENDOR_SUBCMD, 0x24);
    return 0;
nla_put_failure:
    return -ENOBUFS;
}

int vespercsi_csi_capture_handler(struct nl_msg *msg, void *arg){

    static vespercsi_csi_metadata_t current_csi_metadata = {0};
    static vespercsi_csi_data_t current_csi_data = {0};

    struct nlattr *attrs[MAX_CMD + 1];
    struct nlmsghdr *nlh = nlmsg_hdr(msg);

    nlmsg_parse(nlh, 32, attrs, MAX_CMD, NULL);

    if (attrs[IWL_MVM_VENDOR_ATTR_CSI_HDR])
    {
        unsigned int header_length = nla_len(attrs[IWL_MVM_VENDOR_ATTR_CSI_HDR]);

        if(header_length != VESPERCSI_CSI_HEADER_LENGTH){
            return NL_SKIP;
        }

        uint8_t *header = (uint8_t *)nla_data(attrs[IWL_MVM_VENDOR_ATTR_CSI_HDR]);

        memcpy(&current_csi_metadata.header, header, header_length);

        if (attrs[IWL_MVM_VENDOR_ATTR_CSI_DATA]){

            unsigned int data_length = nla_len(attrs[IWL_MVM_VENDOR_ATTR_CSI_DATA]);

            uint8_t* data = (uint8_t *)nla_data(attrs[IWL_MVM_VENDOR_ATTR_CSI_DATA]);
            
            memcpy(current_csi_data.csi_data, data, data_length);

            current_csi_metadata.num_rx = current_csi_metadata.header.num_rx;
            current_csi_metadata.num_tx = current_csi_metadata.header.num_tx;
            current_csi_metadata.num_subcarriers = current_csi_metadata.header.num_subcarriers;
            current_csi_metadata.format = current_csi_metadata.header.rateNflag & RATE_MCS_MOD_TYPE_MSK;
            current_csi_metadata.channel_width = current_csi_metadata.header.rateNflag & RATE_MCS_CHAN_WIDTH_MSK;

            if(vespercsi_csi_fix_bugs(&current_csi_metadata, &current_csi_data) == VESPERCSI_FAIL){
                return NL_SKIP;
            }

            
            // c->loadFromMemory(header, dataCsi);

            // if (
            //     (c->channelWidth == RATE_MCS_CHAN_WIDTH_20 && vespercsi_options.channel_width == 20) ||
            //     (c->channelWidth == RATE_MCS_CHAN_WIDTH_40 && vespercsi_options.channel_width == 40) ||
            //     (c->channelWidth == RATE_MCS_CHAN_WIDTH_80 && vespercsi_options.channel_width == 80) ||
            //     (c->channelWidth == RATE_MCS_CHAN_WIDTH_160 && vespercsi_options.channel_width == 160)
            
            // )
            // {
            //     if (
            //         (c->format == RATE_MCS_LEGACY_OFDM_MSK && Arguments::arguments.format == "NOHT") ||
            //         (c->format == RATE_MCS_HT_MSK && Arguments::arguments.format == "HT") ||
            //         (c->format == RATE_MCS_VHT_MSK && Arguments::arguments.format == "VHT") ||
            //         (c->format == RATE_MCS_HE_MSK && Arguments::arguments.format == "HESU") ||
            //         (c->format == RATE_MCS_EHT_MSK && Arguments::arguments.format == "EHT")
                
            //     )
            //     {
            //         printf("masm csi\n");
            //         if (!Arguments::arguments.strict || (Arguments::arguments.strict && (c->rawHeaderData.rateNflag & RATE_LEGACY_RATE_MSK) == Arguments::arguments.mcs))
            //         {
            //             if (Arguments::arguments.verbose) {
            //                 printDetail(c);
            //             }
            //             if ( MainController::getInstance()->udpSocket ) {
            //                 c->sendUDP(MainController::getInstance()->udpSocket);
            //             } else {
            //                 c->save();
            //             }
            //             if (Arguments::arguments.plot)
            //             {
            //                 WiFiCsiController::csiQueueMutex.lock();
            //                 WiFiCsiController::csiQueue.push(c);
            //                 WiFiCsiController::csiQueueMutex.unlock();
            //             }
            //         }
                    
            //     }

                
            //}
        }
    }

    /*
    WiFiCsiController *wcc = (WiFiCsiController*) arg;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (wcc->stopTime != 0 && wcc->stopTime < now)
    {
        return NL_OK;
    }
    */
    return NL_SKIP;
}


int vespercsi_csi_fix_bugs(vespercsi_csi_metadata_t* metadata, vespercsi_csi_data_t* data){
    if (metadata->channel_width != RATE_MCS_CHAN_WIDTH_160){
        return VESPERCSI_OK;
    }

    if (metadata->format != RATE_MCS_VHT_MSK && metadata->format != RATE_MCS_HE_MSK){
        return VESPERCSI_OK;
    }

    uint16_t new_subcarrier_size = 0;

    if (metadata->format == RATE_MCS_VHT_MSK && metadata->header.num_subcarriers == 484){
        return VESPERCSI_OK;
    }else if(metadata->format == RATE_MCS_VHT_MSK){
        new_subcarrier_size = 484;
    }

    if (metadata->format == RATE_MCS_HE_MSK && metadata->header.num_subcarriers == 1992){
        return VESPERCSI_OK;
    }else if(metadata->format == RATE_MCS_HE_MSK){
        new_subcarrier_size = 1992;
    }

    for (uint32_t i = 0; i < metadata->header.csi_data_size; i = i + 4){
        if (metadata->format == RATE_MCS_VHT_MSK){
            if (i > (241 * 4) && i < (256 * 4)) // Fix the firmware bug
                continue;
        }
    }

    uint32_t new_total_size = new_subcarrier_size * 4 *metadata->num_rx * metadata->num_tx;

    if(new_total_size > VESPERCSI_CSI_DATA_SIZE_MAX){
        return VESPERCSI_FAIL;
    }

    uint8_t fixed_csi_data[new_total_size];

    uint32_t newIndex = 0;
    uint32_t oldIndex = 0;

    for (uint32_t rx = 0; rx < metadata->num_rx; rx++)
    {
        for (uint32_t tx = 0; tx < metadata->num_tx; tx++)
        {
            for (uint32_t n = 0; n < metadata->num_subcarriers; n++)
            {
                if (metadata->format == RATE_MCS_VHT_MSK)
                {
                    if (n > 241 && n < 256)
                    {
                        oldIndex += 4;
                        continue;
                    }
                }
                if (metadata->format == RATE_MCS_HE_MSK)
                {
                    if (n > 995 && n < 1024)
                    {
                        oldIndex += 4;
                        continue;
                    }
                }
                memcpy(&fixed_csi_data[newIndex], &data->csi_data[oldIndex], 4);
                oldIndex += 4;
                newIndex += 4;
            }
        }
    }

    metadata->num_subcarriers = new_subcarrier_size;
    metadata->header.num_subcarriers = metadata->num_subcarriers;
    metadata->header.csi_data_size = new_total_size;
    memcpy(data->csi_data, fixed_csi_data, new_total_size);
    return VESPERCSI_OK;
}
