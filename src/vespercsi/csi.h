#ifndef VESPERCSI_CSI_H
#define VESPERCSI_CSI_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <complex.h>
#include <vespercsi/netlink.h>
#include <dirent.h>
#include <linux/limits.h>
#include <unistd.h>
#include <rs.h>
#include <errno.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/attr.h>
#include <net/if.h>
#include <nl80211.h>

#define VESPERCSI_CSI_HEADER_LENGTH 272

#define IWL_MVM_VENDOR_ATTR_CSI_HDR 0x4d
#define IWL_MVM_VENDOR_ATTR_CSI_DATA 0x4e
#define MAX_CMD 0x4f

extern uint32_t VESPERCSI_CSI_NO_NHT_20_PILOT_INDICES[];
extern uint32_t VESPERCSI_CSI_HT_VHT_20_PILOT_INDICES[];
extern uint32_t VESPERCSI_CSI_HT_VHT_40_PILOT_INDICES[];
extern uint32_t VESPERCSI_CSI_VHT_80_PILOT_INDICES[];
extern uint32_t VESPERCSI_CSI_VHT_160_PILOT_INDICES[];
extern uint32_t VESPERCSI_CSI_HE_20_PILOT_INDICES[];
extern uint32_t VESPERCSI_CSI_HE_40_PILOT_INDICES[];
extern uint32_t VESPERCSI_CSI_HE_80_PILOT_INDICES[];
extern uint32_t VESPERCSI_CSI_HE_160_PILOT_INDICES[];

typedef struct __attribute__((__packed__)) vespercsi_csi_header_t{
    uint32_t csiDataSize;
    uint32_t space4;
    uint32_t ftmClock;
    uint64_t timestamp;
    uint8_t space20[26];
    uint8_t numRx;
    uint8_t numTx;
    uint8_t space48[4];
    uint32_t numSubCarriers;
    uint8_t space54[4];
    uint32_t rssi1;
    uint32_t rssi2;
    uint8_t srcMac[6];
    uint8_t space75[18];
    uint32_t rateNflag;
    uint32_t space96[44];
}vespercsi_csi_header_t;

typedef struct vespercsi_csi_t{
    vespercsi_csi_header_t header;
    uint32_t num_rx;
    uint32_t num_tx;
    uint32_t num_subcarriers;
    uint32_t format;
    uint32_t channel_width;
    // std::vector<std::complex<double>> csi;
    // std::vector<std::complex<double>> csiBackup;
    // std::vector<double> magnitude;
    // std::vector<double> phase;
    uint8_t *raw_csi_data;
    
}vespercsi_csi_t;


typedef struct vespercsi_csi_controller_t{
    vespercsi_netlink_t netlink;

}vespercsi_csi_controller_t;

int vespercsi_wifi_toggle_iwlwifi_csi(bool enable);

int vespercsi_csi_listen_handler(struct vespercsi_netlink_t *state, struct nl_msg *msg, void *arg);

int vespercsi_csi_capture_handler(struct nl_msg *msg, void *arg);

#endif
