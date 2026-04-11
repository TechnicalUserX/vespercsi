#include <vespercsi/wifi.h>
#include <vespercsi/netlink.h>
#include <vespercsi/options.h>
#include <vespercsi/log.h>

vespercsi_wifi_controller_t vespercsi_wifi_controller = {0};

// API

void vespercsi_wifi_controller_init(vespercsi_wifi_controller_t* controller){

    if(vespercsi_netlink_init(&controller->netlink) == VESPERCSI_FAIL){
       printf("VesperCSI: Cannot initalize Wi-Fi Controller netlink\n");
       exit(EXIT_FAILURE); 
    }
    
}

void vespercsi_wifi_controller_destroy(vespercsi_wifi_controller_t* controller){
    if(!controller){
        return;
    }

    vespercsi_netlink_destroy(&controller->netlink);

}

void vespercsi_wifi_get_phys(vespercsi_wifi_controller_t* controller){
    vespercsi_netlink_command_t command = {
        .id = NL80211_CMD_GET_WIPHY,
        .idby = VESPERCSI_NETLINK_CIB_NONE,
        .nlFlags = NLM_F_DUMP,
        .handler = NULL,
        .valid_handler = vespercsi_wifi_get_phys_handler,
    };

    vespercsi_netlink_execute(&controller->netlink, &command);
}

void vespercsi_wifi_get_interfaces(vespercsi_wifi_controller_t* controller){

    vespercsi_netlink_command_t command = {
        .id = NL80211_CMD_GET_INTERFACE,
        .idby = VESPERCSI_NETLINK_CIB_NONE,
        .nlFlags = NLM_F_DUMP,
        .handler = NULL,
        .valid_handler = vespercsi_wifi_get_interfaces_handler,
    };

    vespercsi_netlink_execute(&controller->netlink, &command);
}

void vespercsi_wifi_get_single_interface(vespercsi_wifi_controller_t* controller, vespercsi_wifi_interface_t* interface, const char *ifname){

    vespercsi_netlink_command_t command = {
        .id = NL80211_CMD_GET_INTERFACE,
        .idby = VESPERCSI_NETLINK_CIB_NETDEV,
        .nlFlags = 0,
        .device = if_nametoindex(ifname),
        .handler = NULL,
        .valid_handler = vespercsi_wifi_get_single_interface_handler,
        .args = (void*)interface
    };

    vespercsi_netlink_execute(&controller->netlink, &command);
}

void vespercsi_wifi_remove_interface(vespercsi_wifi_controller_t* controller, const char *name, int index){
    vespercsi_netlink_command_t command = {
        .id = NL80211_CMD_DEL_INTERFACE,
        .idby = VESPERCSI_NETLINK_CIB_NETDEV,
        .nlFlags = 0,
        .device = index ? index : if_nametoindex(name),
        .handler = NULL,
        .valid_handler = NULL,
    };

    vespercsi_netlink_execute(&controller->netlink, &command);
}

void vespercsi_wifi_set_interface_frequency(vespercsi_wifi_controller_t* controller, const char* ifname, uint16_t freq, const char *bw){
    
    const void *settings[] = {&freq, bw};

    vespercsi_netlink_command_t command = {
        .id = NL80211_CMD_SET_WIPHY,
        .idby = VESPERCSI_NETLINK_CIB_NETDEV,
        .nlFlags = 0,
        .device = if_nametoindex(ifname),
        .handler = vespercsi_wifi_set_interface_frequency_handler,
        .valid_handler = NULL,
        .args = settings,
    };

    vespercsi_netlink_execute(&controller->netlink, &command);
}


int vespercsi_wifi_set_interface_updown(const char *ifname, bool up){
    // Create a socket file descriptor
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Failed to create socket");
        return 1;
    }

    // Get the interface index
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("Failed to get interface index");
        close(sockfd);
        return 1;
    }

    if (up)
    {
        ifr.ifr_flags |= IFF_UP;
    }
    else
    {
        ifr.ifr_flags &= ~IFF_UP;
    }

    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0)
    {
        perror("Failed to bring down interface");
        close(sockfd);
        return 1;
    }

    // Close the socket
    close(sockfd);

    LOG_VERBOSE(VERBOSITY_0, INFO, "Interface %s has been brought %s", ifname, (up ? "up" : "down"));

    return 0;
}

void vespercsi_wifi_add_device(vespercsi_wifi_controller_t* controller, const char *name, uint64_t phy_id, const enum nl80211_iftype type)
{

    const void *settings[] = {name, &type};

    vespercsi_netlink_command_t command = {
        .id = NL80211_CMD_NEW_INTERFACE,
        .idby = VESPERCSI_NETLINK_CIB_PHY,
        .nlFlags = 0,
        .device = phy_id,
        .handler = vespercsi_wifi_add_device_handler,
        .valid_handler = NULL,
        .args = settings,
    };
    
    vespercsi_netlink_execute(&controller->netlink, &command);

}

// Handlers

int vespercsi_wifi_get_interfaces_handler(struct nl_msg *msg, void *arg){

    if(vespercsi_wifi_controller.interfaces_size == VESPERCSI_WIFI_INTERFACE_MAX_SIZE){
        return NL_OK;
    }

    struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];

    vespercsi_wifi_interface_t new_interface = {0};

    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

    if (tb_msg[NL80211_ATTR_IFNAME])
    {
        strncpy(new_interface.name,  nla_get_string(tb_msg[NL80211_ATTR_IFNAME]), ARRAY_SIZE(new_interface.name) - 1);
    }
    else
    {
        strncpy(new_interface.name, "Unnamed/non-netdev interface", ARRAY_SIZE(new_interface.name) - 1);
    }

    if (tb_msg[NL80211_ATTR_IFINDEX])
    {
        new_interface.index = nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
    }
    if (tb_msg[NL80211_ATTR_WDEV])
    {
        new_interface.wdev = (uint64_t)nla_get_u64(tb_msg[NL80211_ATTR_WDEV]);
    }
    if (tb_msg[NL80211_ATTR_MAC])
    {
        vespercsi_wifi_mac_string(new_interface.mac, (const unsigned char *)nla_data(tb_msg[NL80211_ATTR_MAC]));
    }
    if (tb_msg[NL80211_ATTR_SSID])
    {
        strncpy(new_interface.ssid, (const char *)nla_data(tb_msg[NL80211_ATTR_SSID]), ARRAY_SIZE(new_interface.ssid)-1);
    }
    if (tb_msg[NL80211_ATTR_IFTYPE])
    {
        new_interface.type = nla_get_u32(tb_msg[NL80211_ATTR_IFTYPE]);
    }

    if (tb_msg[NL80211_ATTR_WIPHY])
    {
        new_interface.wiphy = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY]);
    }
    if (tb_msg[NL80211_ATTR_WIPHY_FREQ])
    {
        new_interface.freq = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_FREQ]);
        new_interface.channel = vespercsi_wifi_frequency_to_channel(new_interface.freq);
        if (tb_msg[NL80211_ATTR_CHANNEL_WIDTH])
        {
            new_interface.channel_width = nla_get_u32(tb_msg[NL80211_ATTR_CHANNEL_WIDTH]);
            if (tb_msg[NL80211_ATTR_CENTER_FREQ1])
            {
                new_interface.center_freq_1 = nla_get_u32(tb_msg[NL80211_ATTR_CENTER_FREQ1]);
            }
            if (tb_msg[NL80211_ATTR_CENTER_FREQ2])
            {
                new_interface.center_freq_2 = nla_get_u32(tb_msg[NL80211_ATTR_CENTER_FREQ2]);
            }
        }
        else if (tb_msg[NL80211_ATTR_WIPHY_CHANNEL_TYPE])
        {
            new_interface.channel_type = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_CHANNEL_TYPE]);
        }
    }

    if (tb_msg[NL80211_ATTR_WIPHY_TX_POWER_LEVEL])
    {
        int32_t txp = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_TX_POWER_LEVEL]);
        new_interface.txdBm = txp % 100;
        new_interface.txdBm += (double)txp / 100;
    }

    if (tb_msg[NL80211_ATTR_IFINDEX])
    {
        vespercsi_wifi_controller.interfaces[vespercsi_wifi_controller.interfaces_size] = new_interface;
        vespercsi_wifi_controller.interfaces_size++;
    }

    return NL_OK;
}

int vespercsi_wifi_get_single_interface_handler(struct nl_msg *msg, void *arg){

    vespercsi_wifi_interface_t* current_interface = (vespercsi_wifi_interface_t*)arg;
    
    struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];

    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
              genlmsg_attrlen(gnlh, 0), NULL);

    if (tb_msg[NL80211_ATTR_IFNAME]){
        strncpy(current_interface->name,  nla_get_string(tb_msg[NL80211_ATTR_IFNAME]), ARRAY_SIZE(current_interface->name) - 1);
    }else{
        strncpy(current_interface->name, "Unnamed/non-netdev interface", ARRAY_SIZE(current_interface->name) - 1);
        return NL_OK;
    }

    if(tb_msg[NL80211_ATTR_IFINDEX]){
        current_interface->index = nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
    }else{
        current_interface->index  = 0;
    }

    if(tb_msg[NL80211_ATTR_WDEV]){
       current_interface->wdev = (uint64_t)nla_get_u64(tb_msg[NL80211_ATTR_WDEV]);
    }else{
        current_interface->wdev = 0;
    }

    if(tb_msg[NL80211_ATTR_MAC]){
        vespercsi_wifi_mac_string(current_interface->mac, (const unsigned char *)nla_data(tb_msg[NL80211_ATTR_MAC]));

    }else{
        strcpy(current_interface->mac, "");
    }

    if(tb_msg[NL80211_ATTR_SSID]){
        strncpy(current_interface->ssid, (const char *)nla_data(tb_msg[NL80211_ATTR_SSID]), ARRAY_SIZE(current_interface->ssid)-1);

    }else{
        strcpy(current_interface->ssid, "");
    }

    if(tb_msg[NL80211_ATTR_IFTYPE]){
        current_interface->type = nla_get_u32(tb_msg[NL80211_ATTR_IFTYPE]);
    }else{
        current_interface->type = 0;
    }

    if(tb_msg[NL80211_ATTR_WIPHY]){
        current_interface->wiphy = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY]);
    }else{
        current_interface->wiphy = 0;
    }

    if(tb_msg[NL80211_ATTR_WIPHY_FREQ]){
        current_interface->freq = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_FREQ]);
        current_interface->channel = vespercsi_wifi_frequency_to_channel(current_interface->freq);
    }else{
        current_interface->freq = 0;
        current_interface->channel = 0;
    }

    if(tb_msg[NL80211_ATTR_WIPHY_TX_POWER_LEVEL]){
        int32_t txp = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_TX_POWER_LEVEL]);
        current_interface->txdBm = txp % 100;
        current_interface->txdBm += (double)txp / 100;
    }else{
        current_interface->txdBm = 0;
    }

    return NL_OK;
}

int vespercsi_wifi_set_interface_frequency_handler(vespercsi_netlink_t *state, struct nl_msg *msg, void *arg){
    
    void **settings = (void **)arg;
    uint16_t settingsFreq = *(uint16_t *)(settings[0]);
    char *settingsWidth = (char *)(settings[1]);


    LOG_VERBOSE(VERBOSITY_0, INFO, "Setting frequency %hu - Channel Width: %s", settingsFreq, settingsWidth);

    uint32_t freq = settingsFreq;
    uint32_t freq_offset = 0;

    unsigned int control_freq = freq;
    unsigned int control_freq_offset = freq_offset;
    unsigned int center_freq1 = freq;
    unsigned int center_freq1_offset = freq_offset;
    enum nl80211_chan_width width = control_freq < 1000 ? NL80211_CHAN_WIDTH_16 : NL80211_CHAN_WIDTH_20_NOHT;

    vespercsi_wifi_channel_mode_t selectedChanMode = vespercsi_wifi_get_channel_mode(settingsWidth);

    center_freq1 = vespercsi_wifi_get_cf1(&selectedChanMode, freq);

    /* For non-S1G frequency */
    if (center_freq1 > 1000)
        center_freq1_offset = 0;

    width = (enum nl80211_chan_width)selectedChanMode.width;

    NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, control_freq);
    NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ_OFFSET, control_freq_offset);
    NLA_PUT_U32(msg, NL80211_ATTR_CHANNEL_WIDTH, width);

    switch (width)
    {
    case NL80211_CHAN_WIDTH_20_NOHT:
        NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE, NL80211_CHAN_NO_HT);
        break;
    case NL80211_CHAN_WIDTH_20:
        NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE, NL80211_CHAN_HT20);
        break;
    case NL80211_CHAN_WIDTH_40:
        if (control_freq > center_freq1)
        {
            NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE, NL80211_CHAN_HT40MINUS);
        }
        else
        {
            NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE, NL80211_CHAN_HT40PLUS);
        }
        break;
    default:
        break;
    }

    if (center_freq1)
    {
        NLA_PUT_U32(msg, NL80211_ATTR_CENTER_FREQ1, center_freq1);
    }

    if (center_freq1_offset)
    {
        NLA_PUT_U32(msg, NL80211_ATTR_CENTER_FREQ1_OFFSET, center_freq1_offset);
    }

    return 0;

nla_put_failure:
    return -ENOBUFS;
}

int vespercsi_wifi_get_phys_handler(struct nl_msg *msg, void *arg){

    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
              genlmsg_attrlen(gnlh, 0), NULL);


    if (tb_msg[NL80211_ATTR_WIPHY] && tb_msg[NL80211_ATTR_WIPHY_NAME]) {
        if (vespercsi_wifi_controller.phys_size < VESPERCSI_WIFI_PHY_MAX_SIZE) {
            vespercsi_wifi_controller.phys[vespercsi_wifi_controller.phys_size].index =
                nla_get_u32(tb_msg[NL80211_ATTR_WIPHY]);
            strncpy(vespercsi_wifi_controller.phys[vespercsi_wifi_controller.phys_size].name,
                    nla_get_string(tb_msg[NL80211_ATTR_WIPHY_NAME]), 31);
            vespercsi_wifi_controller.phys_size++;
        }
    }


    return NL_OK;
}

int vespercsi_wifi_add_device_handler(vespercsi_netlink_t *state, struct nl_msg *msg, void *arg){

    void **settings = (void **)arg;
    const char *name = (const char *)settings[0];
    enum nl80211_iftype *type = (enum nl80211_iftype *)(settings[1]);

    NLA_PUT_STRING(msg, NL80211_ATTR_IFNAME, name);
    NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, *type);
    NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, vespercsi_options.interface_mac);
    return 0;
nla_put_failure:
    return -ENOBUFS;
}

// Utility

void vespercsi_wifi_mac_string(char* dest, const uint8_t *src){
    if(dest == NULL || src == NULL){
        return;
    }

    int i, l;
    char mac_addr[20] = {0};

    l = 0;
    for (i = 0; i < ETH_ALEN; i++)
    {
        if (i == 0)
        {
            sprintf(mac_addr + l, "%02X", src[i]);
            l += 2;
        }
        else
        {
            sprintf(mac_addr + l, ":%02X", src[i]);
            l += 3;
        }
    }

    strncpy(dest, mac_addr, VESPERCSI_WIFI_MAC_STR_SIZE - 1);
}

int vespercsi_wifi_frequency_to_channel(int freq){
    if (freq < 1000)
        return 0;
    /* see 802.11-2007 17.3.8.3.2 and Annex J */
    if (freq == 2484)
        return 14;
    /* see 802.11ax D6.1 27.3.23.2 and Annex E */
    else if (freq == 5935)
        return 2;
    else if (freq < 2484)
        return (freq - 2407) / 5;
    else if (freq >= 4910 && freq <= 4980)
        return (freq - 4000) / 5;
    else if (freq < 5950)
        return (freq - 5000) / 5;
    else if (freq <= 45000) /* DMG band lower limit */
        /* see 802.11ax D6.1 27.3.23.2 */
        return (freq - 5950) / 5;
    else if (freq >= 58320 && freq <= 70200)
        return (freq - 56160) / 2160;
    else
        return 0;    
}

vespercsi_wifi_channel_mode_t vespercsi_wifi_get_channel_mode(const char *width){

    static const vespercsi_wifi_channel_mode_t chanMode[] = {
        {.name = "20",
         .width = NL80211_CHAN_WIDTH_20,
         .freq_1_diff = 0,
         .channel_type = NL80211_CHAN_HT20},
        {.name = "40",
         .width = NL80211_CHAN_WIDTH_40,
         .freq_1_diff = 10,
         .channel_type = NL80211_CHAN_HT40PLUS},
        {.name = "HT40-",
         .width = NL80211_CHAN_WIDTH_40,
         .freq_1_diff = -10,
         .channel_type = NL80211_CHAN_HT40MINUS},
        {.name = "NOHT",
         .width = NL80211_CHAN_WIDTH_20_NOHT,
         .freq_1_diff = 0,
         .channel_type = NL80211_CHAN_NO_HT},
        {.name = "5MHz",
         .width = NL80211_CHAN_WIDTH_5,
         .freq_1_diff = 0,
         .channel_type = -1},
        {.name = "10MHz",
         .width = NL80211_CHAN_WIDTH_10,
         .freq_1_diff = 0,
         .channel_type = -1},
        {.name = "80",
         .width = NL80211_CHAN_WIDTH_80,
         .freq_1_diff = 0,
         .channel_type = -1},
        {.name = "160",
         .width = NL80211_CHAN_WIDTH_160,
         .freq_1_diff = 0,
         .channel_type = -1},
        {.name = "320MHz",
         .width = NL80211_CHAN_WIDTH_320,
         .freq_1_diff = 0,
         .channel_type = -1},
        {.name = "1MHz",
         .width = NL80211_CHAN_WIDTH_1,
         .freq_1_diff = 0,
         .channel_type = -1},
        {.name = "2MHz",
         .width = NL80211_CHAN_WIDTH_2,
         .freq_1_diff = 0,
         .channel_type = -1},
        {.name = "4MHz",
         .width = NL80211_CHAN_WIDTH_4,
         .freq_1_diff = 0,
         .channel_type = -1},
        {.name = "8MHz",
         .width = NL80211_CHAN_WIDTH_8,
         .freq_1_diff = 0,
         .channel_type = -1},
        {.name = "16MHz",
         .width = NL80211_CHAN_WIDTH_16,
         .freq_1_diff = 0,
         .channel_type = -1},
    };

    vespercsi_wifi_channel_mode_t selectedChanMode = {0};

    for (unsigned int i = 0; i < ARRAY_SIZE(chanMode); i++)
    {
        if (strcasecmp(chanMode[i].name, width) == 0)
        {
            selectedChanMode = chanMode[i];
            break;
        }
    }

    return selectedChanMode;
}

int vespercsi_wifi_get_cf1(const struct vespercsi_wifi_channel_mode_t *chanmode, unsigned long freq){
    unsigned int cf1 = freq, j;
    unsigned int bw80[] = {5180, 5260, 5500, 5580, 5660, 5745,
                           5955, 6035, 6115, 6195, 6275, 6355,
                           6435, 6515, 6595, 6675, 6755, 6835,
                           6195, 6995};
    unsigned int bw160[] = {5180, 5500, 5955, 6115, 6275, 6435,
                            6595, 6755, 6915};
    /* based on 11be D2 E.1 Country information and operating classes */
    unsigned int bw320[] = {5955, 6115, 6275, 6435, 6595, 6755};

    switch (chanmode->width)
    {
    case NL80211_CHAN_WIDTH_80:
        /* setup center_freq1 */
        for (j = 0; j < ARRAY_SIZE(bw80); j++)
        {
            if (freq >= bw80[j] && freq < bw80[j] + 80)
                break;
        }

        if (j == ARRAY_SIZE(bw80))
            break;

        cf1 = bw80[j] + 30;
        break;
    case NL80211_CHAN_WIDTH_160:
        /* setup center_freq1 */
        for (j = 0; j < ARRAY_SIZE(bw160); j++)
        {
            if (freq >= bw160[j] && freq < bw160[j] + 160)
                break;
        }

        if (j == ARRAY_SIZE(bw160))
            break;

        cf1 = bw160[j] + 70;
        break;
    case NL80211_CHAN_WIDTH_320:
        /* setup center_freq1 */
        for (j = 0; j < ARRAY_SIZE(bw320); j++)
        {
            if (freq >= bw320[j] && freq < bw320[j] + 160)
                break;
        }

        if (j == ARRAY_SIZE(bw320))
            break;

        cf1 = bw320[j] + 150;
        break;
    default:
        cf1 = freq + chanmode->freq_1_diff;
        break;
    }

    return cf1;
}

uint16_t vespercsi_wifi_channel_mode_to_width(vespercsi_wifi_channel_mode_t* chanMode){
    switch (chanMode->width)
    {
    case NL80211_CHAN_WIDTH_20:
        return 20;
    case NL80211_CHAN_WIDTH_40:
        return 40;
    case NL80211_CHAN_WIDTH_80:
        return 80;
    case NL80211_CHAN_WIDTH_160:
        return 160;
    case NL80211_CHAN_WIDTH_320:
        return 320;

    default:
        break;
    }
    return 0;
}

bool vespercsi_wifi_check_phy_mac(const char* phy_name, const char* target_mac){
    if (phy_name == NULL || target_mac == NULL) {
        return false;
    }

    char path[256];
    // Construct the absolute path to the sysfs macaddress file
    snprintf(path, sizeof(path), "/sys/class/ieee80211/%s/macaddress", phy_name);

    // Open the file descriptor directly (POSIX style)
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false; // File doesn't exist or we lack permissions
    }

    char buffer[32] = {0};
    // Read the MAC address. 17 chars + 1 newline = 18 bytes expected.
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    
    // Immediately close the file descriptor to prevent leaks
    close(fd);

    if (bytes_read <= 0) {
        return false;
    }

    // sysfs files almost always end with a newline character. 
    // We must strip it before comparing strings.
    buffer[strcspn(buffer, "\n")] = '\0';

    // Use case-insensitive comparison in case sysfs uses lowercase 
    // and your target_mac is uppercase
    if (strcasecmp(buffer, target_mac) == 0) {
        return true;
    }

    return false;
}

int vespercsi_wifi_toggle_iwlwifi(bool enable){

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
