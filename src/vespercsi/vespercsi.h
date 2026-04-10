#ifndef VESPERCSI_H
#define VESPERCSI_H

#define VESPERCSI_OK        0
#define VESPERCSI_FAIL      1

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif


#include <stdio.h>

static inline void vespercsi_banner(void) {
    printf(
        "\n"
        "  __   __                       ___ ___ ___ \n"
        "  \\ \\ / /__ ____ __  ___ _ _  / __/ __|_ _|\n"
        "   \\ V / -_|_-< '_ \\/ -_) '_|| (__\\__ \\| | \n"
        "    \\_/\\___/__/ .__/\\___|_|   \\___|___/___|\n"
        "              |_|                           \n"
        "\n"
        "  Wi-Fi Channel State Information Capture Tool\n"
        "\n"
    );

    fflush(stdout);
}

#endif
