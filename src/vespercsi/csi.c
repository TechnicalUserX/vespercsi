#include <vespercsi/csi.h>
#include <vespercsi/log.h>
#include <vespercsi/wifi.h>

uint32_t VESPERCSI_CSI_NO_NHT_20_PILOT_INDICES[] = {5, 19, 32, 46};                                                                                                                                                              // 52 subcarriers
uint32_t VESPERCSI_CSI_HT_VHT_20_PILOT_INDICES[] = {7, 21, 34, 48};                                                                                                                                                              // 56 subcarriers
uint32_t VESPERCSI_CSI_HT_VHT_40_PILOT_INDICES[] = {5, 33, 47, 66, 80, 108};                                                                                                                                                     // 114 subcarriers
uint32_t VESPERCSI_CSI_VHT_80_PILOT_INDICES[] = {19, 47, 83, 111, 130, 158, 194, 222};                                                                                                                                           // 242 subcarriers
uint32_t VESPERCSI_CSI_VHT_160_PILOT_INDICES[] = {19, 47, 83, 111, 130, 158, 194, 222, 261, 289, 325, 353, 372, 400, 436, 464};                                                                                                  // 484 subcarriers
uint32_t VESPERCSI_CSI_HE_20_PILOT_INDICES[] = {6, 32, 74, 100, 141, 167, 209, 235};                                                                                                                                             // 242 subcarriers
uint32_t VESPERCSI_CSI_HE_40_PILOT_INDICES[] = {6, 32, 74, 100, 140, 166, 208, 234, 249, 275, 317, 343, 383, 409, 451, 477};                                                                                                     // 484 subcarriers
uint32_t VESPERCSI_CSI_HE_80_PILOT_INDICES[] = {32, 100, 166, 234, 274, 342, 408, 476, 519, 587, 653, 721, 761, 829, 895, 963};                                                                                                  // 996 subcarriers
uint32_t VESPERCSI_CSI_HE_160_PILOT_INDICES[] = {32, 100, 166, 234, 274, 342, 408, 476, 519, 587, 653, 721, 761, 829, 895, 963, 1028, 1096, 1162, 1230, 1270, 1338, 1404, 1472, 1515, 1583, 1649, 1717, 1757, 1825, 1891, 1959}; // 1992 subcarriers

int vespercsi_csi_listen(vespercsi_netlink_t* netlink)
{
    vespercsi_netlink_command_t command = {
        .id = NL80211_CMD_VENDOR,
        .idby = VESPERCSI_NETLINK_CIB_NETDEV,
        .nlFlags = 0,
        .device = if_nametoindex(VESPERCSI_WIFI_INTERFACE),
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

int vespercsi_csi_capture_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *attrs[MAX_CMD + 1];
    struct nlmsghdr *nlh = nlmsg_hdr(msg);

    nlmsg_parse(nlh, 32, attrs, MAX_CMD, NULL);
    if (attrs[IWL_MVM_VENDOR_ATTR_CSI_HDR])
    {
        unsigned int dataLength = nla_len(attrs[IWL_MVM_VENDOR_ATTR_CSI_HDR]);
        if (dataLength != VESPERCSI_CSI_HEADER_LENGTH)
        {
            return NL_SKIP;
        }
        
        uint8_t header[dataLength];
        uint8_t *data = (uint8_t *)nla_data(attrs[IWL_MVM_VENDOR_ATTR_CSI_HDR]);
        memcpy(header, data, dataLength);

        if (attrs[IWL_MVM_VENDOR_ATTR_CSI_DATA])
        {
            dataLength = nla_len(attrs[IWL_MVM_VENDOR_ATTR_CSI_DATA]);
            uint8_t rawCsi[dataLength];
            uint8_t *dataCsi = (uint8_t *)nla_data(attrs[IWL_MVM_VENDOR_ATTR_CSI_DATA]);
            memcpy(rawCsi, dataCsi, dataLength);

            Csi *c = new Csi();
            c->loadFromMemory(header, dataCsi);

            if (
                (c->channelWidth == RATE_MCS_CHAN_WIDTH_20 && vespercsi_options.channel_width == 20) ||
                (c->channelWidth == RATE_MCS_CHAN_WIDTH_40 && vespercsi_options.channel_width == 40) ||
                (c->channelWidth == RATE_MCS_CHAN_WIDTH_80 && vespercsi_options.channel_width == 80) ||
                (c->channelWidth == RATE_MCS_CHAN_WIDTH_160 && vespercsi_options.channel_width == 160)
            
            )
            {
                if (
                    (c->format == RATE_MCS_LEGACY_OFDM_MSK && Arguments::arguments.format == "NOHT") ||
                    (c->format == RATE_MCS_HT_MSK && Arguments::arguments.format == "HT") ||
                    (c->format == RATE_MCS_VHT_MSK && Arguments::arguments.format == "VHT") ||
                    (c->format == RATE_MCS_HE_MSK && Arguments::arguments.format == "HESU") ||
                    (c->format == RATE_MCS_EHT_MSK && Arguments::arguments.format == "EHT")
                
                )
                {
                    printf("masm csi\n");
                    if (!Arguments::arguments.strict || (Arguments::arguments.strict && (c->rawHeaderData.rateNflag & RATE_LEGACY_RATE_MSK) == Arguments::arguments.mcs))
                    {
                        if (Arguments::arguments.verbose) {
                            printDetail(c);
                        }
                        if ( MainController::getInstance()->udpSocket ) {
                            c->sendUDP(MainController::getInstance()->udpSocket);
                        } else {
                            c->save();
                        }
                        if (Arguments::arguments.plot)
                        {
                            WiFiCsiController::csiQueueMutex.lock();
                            WiFiCsiController::csiQueue.push(c);
                            WiFiCsiController::csiQueueMutex.unlock();
                        }
                    }
                    
                }
            }
        }
    }

    WiFiCsiController *wcc = (WiFiCsiController*) arg;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (wcc->stopTime != 0 && wcc->stopTime < now)
    {
        return NL_OK;
    }

    return NL_SKIP;
}

int vespercsi_wifi_toggle_iwlwifi_csi(bool enable){

    const char *baseDir = "/sys/kernel/debug/iwlwifi";
    DIR *dir;
    struct dirent *entry;
    bool csi_enabled = false;
    char path[PATH_MAX];

    // 1. Try to open the base directory
    dir = opendir(baseDir);
    if (!dir) {
        LOG(FAIL, "/sys/kernel/debug/iwlwifi not found. Did you compile iwlwifi with debugfs enabled?");
        return VESPERCSI_FAIL;
    }

    // 2. Iterate through the directory contents
    while ((entry = readdir(dir)) != NULL) {
        // Skip the standard "." and ".." hidden directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the full path: /sys/kernel/debug/iwlwifi/<pci-id>/iwlmvm/csi_enabled
        snprintf(path, sizeof(path), "%s/%s/iwlmvm/csi_enabled", baseDir, entry->d_name);

        // 3. Check if the file exists using access()
        if (access(path, F_OK) == 0) {
            
            // 4. Open, write, and close
            FILE *f = fopen(path, "w");
            if (f) {
                fprintf(f, "%s\n", enable ? "1" : "0");
                fflush(f);
                fclose(f);
                
                LOG(INFO, "Successfully %s iwlwifi CSI measurement on %s", 
                    enable ? "enabled" : "disabled", entry->d_name);
                csi_enabled = true;
            } else {
                LOG(FAIL, "Found csi_enabled file but lacked write permissions: %s", path);
            }
        }
    }

    closedir(dir);

    // 5. Check if we actually found and wrote to at least one valid card
    if (!csi_enabled) {
        LOG(FAIL, "Failed to enable CSI measurement. Device might not support it or iwlmvm is not loaded.");
        return VESPERCSI_FAIL;
    }

    return VESPERCSI_OK;
}
