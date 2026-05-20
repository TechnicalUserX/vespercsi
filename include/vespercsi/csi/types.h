#ifndef VESPERCSI_CSI_TYPES_H
#define VESPERCSI_CSI_TYPES_H

#include <vespercsi/csi/defs.h>
#include <stdint.h>

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

typedef struct __attribute__((__packed__)) vespercsi_csi_data_t{
    uint8_t csi_data[VESPERCSI_CSI_DATA_SIZE_MAX];
}vespercsi_csi_data_t;

typedef struct vespercsi_csi_t{
    vespercsi_csi_header_t header;
    vespercsi_csi_data_t data;
}vespercsi_csi_t;

#endif
