#ifndef VESPERCSI_CSI_H
#define VESPERCSI_CSI_H

#include "vespercsi/listpool.h"
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

#define VESPERCSI_CSI_SUPPORTED_TX_MAX 2
#define VESPERCSI_CSI_SUPPORTED_RX_MAX 2
#define VESPERCSI_CSI_SUPPORTED_SUBCARRIERS_MAX 244

// The max time difference between newest and oldest csi data in seconds
#define VESPERCSI_CSI_SUPPORTED_TIME_FRAME_MAX 10

// The number of CSI data arriving in a second
#define VESPERCSI_CSI_SUPPORTED_RATE_MAX 1000

#define VESPERCSI_CSI_HEADER_LENGTH 272

#define VESPERCSI_CSI_DATA_BYTE_SIZE(subcarriers, tx, rx) (16 * subcarriers * tx * rx)

#define VESPERCSI_CSI_DATA_SIZE_MAX VESPERCSI_CSI_DATA_BYTE_SIZE(VESPERCSI_CSI_SUPPORTED_SUBCARRIERS_MAX, VESPERCSI_CSI_SUPPORTED_TX_MAX, VESPERCSI_CSI_SUPPORTED_RX_MAX)

#define VESPERCSI_CSI_SIZE_MAX (VESPERCSI_CSI_HEADER_LENGTH + VESPERCSI_CSI_DATA_SIZE_MAX)

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
    uint32_t csi_data_size;
    uint32_t space4;
    uint32_t ftm_clock;
    uint64_t timestamp;
    uint8_t space20[26];
    uint8_t num_rx;
    uint8_t num_tx;
    uint8_t space48[4];
    uint32_t num_subcarriers;
    uint8_t space54[4];
    uint32_t rssi1;
    uint32_t rssi2;
    uint8_t src_mac[6];
    uint8_t space75[18];
    uint32_t rateNflag;
    uint32_t space96[44];
}vespercsi_csi_header_t;

typedef struct __attribute__((__packed__)) vespercsi_csi_metadata_t{
    vespercsi_csi_header_t header;
    uint32_t num_rx;
    uint32_t num_tx;
    uint32_t num_subcarriers;
    uint32_t format;
    uint32_t channel_width;
}vespercsi_csi_metadata_t;

typedef struct __attribute__((aligned(4096))) vespercsi_csi_data_t{
    uint8_t csi_data[VESPERCSI_CSI_DATA_SIZE_MAX];
}vespercsi_csi_data_t;

typedef struct vespercsi_csi_t{
    vespercsi_csi_header_t header;
    vespercsi_csi_data_t data;
}vespercsi_csi_t;

typedef struct vespercsi_csi_controller_t{
    vespercsi_netlink_t netlink;
}vespercsi_csi_controller_t;

extern vespercsi_csi_controller_t vespercsi_csi_controller;

LISTPOOL_TEMPLATE_DECLARATION(vespercsi_csi_ringbuffer, VESPERCSI_CSI_RINGBUFFER, vespercsi_csi_t, VESPERCSI_CSI_SUPPORTED_RATE_MAX * VESPERCSI_CSI_SUPPORTED_TIME_FRAME_MAX)

// ======== CSI LISTPOOL RING BUFFERS ======== //

extern vespercsi_csi_ringbuffer_listpool_t vespercsi_csi_ringbuffer_1;
extern vespercsi_csi_ringbuffer_listpool_t vespercsi_csi_ringbuffer_2;

// ======== CSI LISTPOOL ATOMIC POINTERS ======== //

extern vespercsi_csi_ringbuffer_listpool_t* _Atomic vespercsi_csi_ringbuffer_capture;
extern vespercsi_csi_ringbuffer_listpool_t* _Atomic vespercsi_csi_ringbuffer_consume;


int vespercsi_csi_listen_handler(struct vespercsi_netlink_t *state, struct nl_msg *msg, void *arg);

int vespercsi_csi_capture_handler(struct nl_msg *msg, void *arg);

int vespercsi_csi_fix_bugs(vespercsi_csi_metadata_t* metadata, vespercsi_csi_data_t* data);

#endif
