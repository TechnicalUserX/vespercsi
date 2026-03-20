#ifndef VESPERCSI_H
#define VESPERCSI_H

#include <stdint.h>

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

typedef struct __attribute__((aligned(4096))) vespercsi_csi_data_t{
    uint8_t csi_data[VESPERCSI_CSI_DATA_SIZE_MAX];
}vespercsi_csi_data_t;

typedef struct vespercsi_csi_t{
    vespercsi_csi_header_t header;
    vespercsi_csi_data_t data;
}vespercsi_csi_t;

#endif