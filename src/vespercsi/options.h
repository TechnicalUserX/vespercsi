#ifndef VESPERCSI_OPTIONS_H
#define VESPERCSI_OPTIONS_H

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ETH_ALEN 6

typedef struct vespercsi_options_t{
    uint16_t frequency;
    char bandwidth[128];
    uint8_t mcs;
    uint16_t channel_width;
    uint8_t spatial_streams;
    uint8_t tx_power;
    uint32_t antenna;
    char format[16];
    uint8_t mac[ETH_ALEN];
    bool verbose;
    bool log_file;
    char log_file_path[512];
    bool quiet;
}vespercsi_options_t;

extern vespercsi_options_t vespercsi_options;

void vespercsi_options_parse(int argc, char* argv[]);

#endif
