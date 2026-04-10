#ifndef VESPERCSI_CONFIG_H
#define VESPERCSI_CONFIG_H

#include <vespercsi/vespercsi.h>
#include <libconfig.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <net/if.h>

typedef struct vespercsi_config_t{

    struct{
        struct{
            bool core_set;
            int core;

            bool priority_set;
            int priority;

        }capture;

        struct{
            bool core_set;
            int core;

            bool priority_set;
            int priority;
        }consume;
    }runners;

    struct{
        bool set;
        char path[512];
    }plugin;

}vespercsi_config_t;

extern vespercsi_config_t vespercsi_config;

void vespercsi_config_init(vespercsi_config_t* config);

#endif
