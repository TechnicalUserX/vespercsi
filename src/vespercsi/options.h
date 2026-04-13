#ifndef VESPERCSI_OPTIONS_H
#define VESPERCSI_OPTIONS_H

#include <vespercsi/vespercsi.h>
#include <vespercsi/version.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <rs.h>

#define ETH_ALEN 6

typedef struct vespercsi_options_t{
    uint16_t frequency;
    char bandwidth[128];
    uint8_t mcs;
    uint16_t channel_width;
    uint8_t spatial_streams;
    uint32_t antenna;
    char format[16];

    // Prints More Information
    bool verbose;
    uint8_t verbosity;

    bool interface_mac_set;
    uint8_t interface_mac[ETH_ALEN];
    
    // Logging File
    bool log_file_set;
    char log_file_path[512];

    // CSI File
    bool out_file_set;
    char out_file_path[512];

    bool quiet;

    // Filter Incoming Packet By Source MAC
    bool src_mac_set;
    uint8_t src_mac[ETH_ALEN];

    // Configuration File for Extra Functionality
    bool config_file_set;
    char config_file_path[512];

    bool unrestricted;

    bool netlink_buffer_instant_death;

    bool daemon;
}vespercsi_options_t;

extern vespercsi_options_t vespercsi_options;

void vespercsi_options_parse(int argc, char* argv[]);

void vespercsi_options_check_mandatory();

void vespercsi_options_print_usage();

#endif
