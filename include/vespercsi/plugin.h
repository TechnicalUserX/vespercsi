#ifndef VESPERCSI_PLUGIN_H
#define VESPERCSI_PLUGIN_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ================================================================
// VesperCSI Plugin SDK
// 
// To create a plugin:
// 1. Include this header
// 2. Implement vespercsi_plugin_callback(), vespercsi_plugin_init() and vespercsi_plugin_destroy()
// 4. Compile as a shared library:
//    gcc -shared -fPIC -o my_plugin.so my_plugin.c
// ================================================================

#define VESPERCSI_PLUGIN_SDK_VERSION 1

// ================================================================
// CSI Data Structures
// These are the only VesperCSI internals the plugin needs to know
// ================================================================

#define VESPERCSI_CSI_HEADER_LENGTH         272
#define VESPERCSI_CSI_SUPPORTED_TX_MAX      2
#define VESPERCSI_CSI_SUPPORTED_RX_MAX      2
#define VESPERCSI_CSI_SUPPORTED_SUBCARRIERS_MAX 244
#define VESPERCSI_CSI_DATA_SIZE_MAX         (16 * VESPERCSI_CSI_SUPPORTED_SUBCARRIERS_MAX * VESPERCSI_CSI_SUPPORTED_TX_MAX * VESPERCSI_CSI_SUPPORTED_RX_MAX)
#define VESPERCSI_CSI_SIZE_MAX              (VESPERCSI_CSI_HEADER_LENGTH + VESPERCSI_CSI_DATA_SIZE_MAX)

typedef struct __attribute__((__packed__)) vespercsi_csi_header_t {
    uint32_t csi_data_size;
    uint32_t space4;
    uint32_t ftm_clock;
    uint64_t timestamp;         // microseconds since Unix epoch (set by VesperCSI)
    uint8_t  space20[26];
    uint8_t  num_rx;
    uint8_t  num_tx;
    uint8_t  space48[4];
    uint32_t num_subcarriers;
    uint8_t  space54[4];
    uint32_t rssi1;
    uint32_t rssi2;
    uint8_t  src_mac[6];        // source MAC address
    uint8_t  space75[18];
    uint32_t rateNflag;
    uint32_t space96[44];
} vespercsi_csi_header_t;

typedef struct __attribute__((aligned(4096))) vespercsi_plugin_csi_data_t {
    uint8_t csi_data[VESPERCSI_CSI_DATA_SIZE_MAX];
} vespercsi_csi_data_t;

typedef struct vespercsi_csi_t {
    vespercsi_csi_header_t header;
    vespercsi_csi_data_t   data;
} vespercsi_csi_t;

// ================================================================
// Rate flags — useful for interpreting rateNflag in the header
// ================================================================

#define VESPERCSI_RATE_MCS_MOD_TYPE_POS     8
#define VESPERCSI_RATE_MCS_MOD_TYPE_MSK     (0x7 << VESPERCSI_RATE_MCS_MOD_TYPE_POS)
#define VESPERCSI_RATE_MCS_LEGACY_OFDM_MSK  (1 << VESPERCSI_RATE_MCS_MOD_TYPE_POS)
#define VESPERCSI_RATE_MCS_HT_MSK           (2 << VESPERCSI_RATE_MCS_MOD_TYPE_POS)
#define VESPERCSI_RATE_MCS_VHT_MSK          (3 << VESPERCSI_RATE_MCS_MOD_TYPE_POS)
#define VESPERCSI_RATE_MCS_HE_MSK           (4 << VESPERCSI_RATE_MCS_MOD_TYPE_POS)
#define VESPERCSI_RATE_MCS_EHT_MSK          (5 << VESPERCSI_RATE_MCS_MOD_TYPE_POS)

#define VESPERCSI_RATE_MCS_CHAN_WIDTH_POS    11
#define VESPERCSI_RATE_MCS_CHAN_WIDTH_MSK    (0x7 << VESPERCSI_RATE_MCS_CHAN_WIDTH_POS)
#define VESPERCSI_RATE_MCS_CHAN_WIDTH_20     (0 << VESPERCSI_RATE_MCS_CHAN_WIDTH_POS)
#define VESPERCSI_RATE_MCS_CHAN_WIDTH_40     (1 << VESPERCSI_RATE_MCS_CHAN_WIDTH_POS)
#define VESPERCSI_RATE_MCS_CHAN_WIDTH_80     (2 << VESPERCSI_RATE_MCS_CHAN_WIDTH_POS)
#define VESPERCSI_RATE_MCS_CHAN_WIDTH_160    (3 << VESPERCSI_RATE_MCS_CHAN_WIDTH_POS)



// ================================================================
// Plugin Internals
// ================================================================

typedef enum vespercsi_log_level_t {
    VESPERCSI_LOG_LEVEL_FAIL = 0,
    VESPERCSI_LOG_LEVEL_WARN,
    VESPERCSI_LOG_LEVEL_INFO,
    VESPERCSI_LOG_LEVEL_PASS,
    VESPERCSI_LOG_LEVEL_DEBG
}vespercsi_log_level_t;

#define FAIL VESPERCSI_LOG_LEVEL_FAIL
#define WARN VESPERCSI_LOG_LEVEL_WARN
#define INFO VESPERCSI_LOG_LEVEL_INFO
#define PASS VESPERCSI_LOG_LEVEL_PASS
#define DEBG VESPERCSI_LOG_LEVEL_DEBG

#define VERBOSITY_0 0
#define VERBOSITY_1 1
#define VERBOSITY_2 2

typedef void (*vespercsi_plugin_function_log_t)(vespercsi_log_level_t level, const char* format, ...);

typedef struct vespercsi_plugin_log_t{
    vespercsi_plugin_function_log_t function;
    bool verbose;
    uint8_t verbosity;
}vespercsi_plugin_log_t;

typedef struct vespercsi_plugin_context_t{
    uint64_t sdk_version;
     vespercsi_plugin_log_t log;
}vespercsi_plugin_context_t;

extern vespercsi_plugin_context_t vespercsi_plugin_context;

#define LOG(level, format, ...) {vespercsi_plugin_context.log.function(level, format, ##__VA_ARGS__);}

#define LOG_VERBOSE(verbosity_index, level, format, ...) if(vespercsi_plugin_context.log.verbose && verbosity_index <= vespercsi_plugin_context.log.verbosity ){vespercsi_plugin_context.log.function(level, format, ##__VA_ARGS__);}

// ================================================================
// Plugin API — symbols your shared library must/can export
// ================================================================

void vespercsi_plugin_function_init(vespercsi_plugin_context_t* context);

void vespercsi_plugin_function_callback(const vespercsi_csi_t* csi);

void vespercsi_plugin_function_destroy(void);

#endif
