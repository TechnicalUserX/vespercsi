#ifndef VESPERCSI_CSI_H
#define VESPERCSI_CSI_H

#include <vespercsi/vespercsi.h>
#include <vespercsi/listpool.h>
#include <vespercsi/csi/types.h>
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
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <time.h>
#include <string.h>
#include <sys/uio.h>

#define IWL_MVM_VENDOR_ATTR_CSI_HDR 0x4d
#define IWL_MVM_VENDOR_ATTR_CSI_DATA 0x4e
#define MAX_CMD 0x4f

LISTPOOL_TEMPLATE_DECLARATION(vespercsi_csi_ringbuffer, VESPERCSI_CSI_RINGBUFFER, vespercsi_csi_t, VESPERCSI_CSI_SUPPORTED_RATE_MAX * VESPERCSI_CSI_SUPPORTED_TIME_FRAME_MAX)

typedef struct vespercsi_csi_controller_t{
    vespercsi_netlink_t netlink;
    pthread_t runner_capture_thread;
    pthread_t runner_consume_thread;
    int vespercsi_csi_out_file;
    vespercsi_csi_ringbuffer_listpool_t vespercsi_csi_ringbuffer_1;
    vespercsi_csi_ringbuffer_listpool_t vespercsi_csi_ringbuffer_2;    
}vespercsi_csi_controller_t;

extern vespercsi_csi_controller_t vespercsi_csi_controller;

void vespercsi_csi_controller_init(vespercsi_csi_controller_t* controller);

void vespercsi_csi_controller_destroy(vespercsi_csi_controller_t* controller);

void vespercsi_csi_capture_start(void);

void vespercsi_csi_capture_stop(void);

void vespercsi_csi_consume_start(void);

void vespercsi_csi_consume_stop(void);

void vespercsi_csi_runner_set_realtime_priority(int priority);

void vespercsi_csi_runner_set_cpu_core(int core);

void* vespercsi_csi_capture_runner(void* arg);

void* vespercsi_csi_consume_runner(void* arg);

int vespercsi_csi_listen(vespercsi_csi_controller_t* controller);

int vespercsi_csi_listen_handler(struct vespercsi_netlink_t *state, struct nl_msg *msg, void *arg);

int vespercsi_csi_capture_handler(struct nl_msg *msg, void *arg);

int vespercsi_csi_fix_bugs(vespercsi_csi_metadata_t* metadata, vespercsi_csi_data_t* data);

void vespercsi_csi_close_file(void);

static inline uint64_t vespercsi_csi_wallclock_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

#endif
