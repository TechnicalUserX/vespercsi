#ifndef VESPERCSI_WIFI_H
#define VESPERCSI_WIFI_H

#include <vespercsi/vespercsi.h>
#include <vespercsi/netlink.h>
#include <linux/netlink.h>
#include <netlink/msg.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <signal.h>


#ifdef CONFIG_VESPERCSI_WIFI_INTERFACE_MAX_SIZE
    #define VESPERCSI_WIFI_INTERFACE_MAX_SIZE CONFIG_VESPERCSI_WIFI_INTERFACE_MAX_SIZE
#else
    #define VESPERCSI_WIFI_INTERFACE_MAX_SIZE 10
#endif

#ifdef CONFIG_VESPERCSI_WIFI_PHY_MAX_SIZE
    #define VESPERCSI_WIFI_PHY_MAX_SIZE CONFIG_VESPERCSI_WIFI_PHY_MAX_SIZE
#else
    #define VESPERCSI_WIFI_PHY_MAX_SIZE 10
#endif

#ifdef CONFIG_VESPERCSI_WIFI_INTERFACE_NAME
    #define VESPERCSI_WIFI_INTERFACE_NAME CONFIG_VESPERCSI_WIFI_INTERFACE_NAME
#else
    #define VESPERCSI_WIFI_INTERFACE_NAME "vespercsimon"
#endif

#define VESPERCSI_WIFI_PHY_NAME_STR_SIZE 32
#define VESPERCSI_WIFI_INTERFACE_NAME_STR_SIZE 64
#define VESPERCSI_WIFI_MAC_STR_SIZE 18

extern char vespercsi_wifi_original_interface[IFNAMSIZ];

typedef struct vespercsi_wifi_interface_t{
    char name[VESPERCSI_WIFI_INTERFACE_NAME_STR_SIZE];
    int index;
    uint64_t wdev;
    char mac[VESPERCSI_WIFI_MAC_STR_SIZE];
    char ssid[256];
    int type;
    int wiphy;
    uint32_t freq;
    int channel;
    int channel_width;
    int center_freq_1;
    int center_freq_2;
    int channel_type;
    double txdBm;
}vespercsi_wifi_interface_t;

typedef struct vespercsi_wifi_phy_t{
    uint64_t index;
    char name[VESPERCSI_WIFI_PHY_NAME_STR_SIZE];
}vespercsi_wifi_phy_t;

typedef struct vespercsi_wifi_channel_mode_t{
    const char* name;
    unsigned int width;
    int freq_1_diff;
    int channel_type; /* for older kernel */
}vespercsi_wifi_channel_mode_t;

typedef struct vespercsi_wifi_controller_t{
    vespercsi_netlink_t netlink;
    uint16_t interfaces_size;
    vespercsi_wifi_interface_t interfaces[VESPERCSI_WIFI_INTERFACE_MAX_SIZE];
    uint16_t phys_size;
    vespercsi_wifi_phy_t phys[VESPERCSI_WIFI_PHY_MAX_SIZE];
}vespercsi_wifi_controller_t;

extern vespercsi_wifi_controller_t vespercsi_wifi_controller;

// API

void vespercsi_wifi_controller_init(vespercsi_wifi_controller_t* controller);

void vespercsi_wifi_controller_destroy(vespercsi_wifi_controller_t* controller);

void vespercsi_wifi_get_phys(vespercsi_wifi_controller_t* controller);

void vespercsi_wifi_get_single_interface(vespercsi_wifi_controller_t* controller, vespercsi_wifi_interface_t* interface, const char *ifname);

void vespercsi_wifi_get_interfaces(vespercsi_wifi_controller_t* controller);

void vespercsi_wifi_remove_interface(vespercsi_wifi_controller_t* controller, const char *name, int index);

void vespercsi_wifi_set_interface_frequency(vespercsi_wifi_controller_t* controller, const char* ifname, uint16_t freq, const char *bw);

void vespercsi_wifi_set_tx_power(vespercsi_wifi_controller_t* controller, const char* ifname);

int vespercsi_wifi_set_interface_updown(const char *ifname, bool up);

void vespercsi_wifi_add_device(vespercsi_wifi_controller_t* controller, const char *name, uint64_t phy_id, const enum nl80211_iftype type);

// Handlers

int vespercsi_wifi_get_interfaces_handler(struct nl_msg *msg, void *arg);

int vespercsi_wifi_get_single_interface_handler(struct nl_msg *msg, void *arg);

int vespercsi_wifi_set_interface_frequency_handler(vespercsi_netlink_t *netlink, struct nl_msg *msg, void *arg);

int vespercsi_wifi_get_phys_handler(struct nl_msg *msg, void *arg);

int vespercsi_wifi_add_device_handler(vespercsi_netlink_t *state, struct nl_msg *msg, void *arg);

// Utility

void vespercsi_wifi_mac_string(char* dest, const uint8_t *src);

int vespercsi_wifi_frequency_to_channel(int freq);

vespercsi_wifi_channel_mode_t vespercsi_wifi_get_channel_mode(const char *width);

int vespercsi_wifi_get_cf1(const struct vespercsi_wifi_channel_mode_t *chanmode, unsigned long freq);

uint16_t vespercsi_wifi_channel_mode_to_width(vespercsi_wifi_channel_mode_t* chanMode);

bool vespercsi_wifi_check_phy_mac(const char* phy_name, const char* target_mac);

int vespercsi_wifi_toggle_iwlwifi(bool enable);

#endif
