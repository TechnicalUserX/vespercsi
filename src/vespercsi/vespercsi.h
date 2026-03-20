#ifndef VESPERCSI_H
#define VESPERCSI_H

#define VESPERCSI_OK        0
#define VESPERCSI_FAIL      1

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <stdio.h>

static inline void vespercsi_banner(void) {
    fprintf(stdout,"\n");
    fprintf(stdout,"  __   __                       ___ ___ ___ \n");
    fprintf(stdout,"  \\ \\ / /__ ____ __  ___ _ _  / __/ __|_ _|\n");
    fprintf(stdout,"   \\ V / -_|_-< '_ \\/ -_) '_|| (__\\__ \\| | \n");
    fprintf(stdout,"    \\_/\\___/__/ .__/\\___|_|   \\___|___/___|\n");
    fprintf(stdout,"              |_|                           \n");
    fprintf(stdout,"\n");
    fprintf(stdout,"  Wi-Fi Channel State Information Capture Tool\n");
    fprintf(stdout,"\n");
    fflush(stdout);
}

#endif