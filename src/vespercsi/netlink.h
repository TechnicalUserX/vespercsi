#ifndef VESPERCSI_NETLINK_H
#define VESPERCSI_NETLINK_H

#include <vespercsi/vespercsi.h>
#include <nl80211.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/socket.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/attr.h>
#include <netlink/msg.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/nl80211.h>
#include <sys/ioctl.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define ETH_ALEN 6

typedef struct vespercsi_netlink_t{
    struct nl_sock*  netlink_socket;
    int netlink_socket_fd;
    int              nl80211_id;
}vespercsi_netlink_t;

// (Command Identified By)
typedef enum vespercsi_netlink_cib_t{
    VESPERCSI_NETLINK_CIB_NONE,
    VESPERCSI_NETLINK_CIB_PHY,
    VESPERCSI_NETLINK_CIB_NETDEV,
    VESPERCSI_NETLINK_CIB_WDEV,
}vespercsi_netlink_cib_t;

typedef struct vespercsi_netlink_command_t{
    uint8_t id;
    vespercsi_netlink_cib_t idby;
    int nlFlags;
    signed long long device;
    int (*handler)(struct vespercsi_netlink_t *netlink, struct nl_msg *msg, void *arg);
    int (*valid_handler)(struct nl_msg *msg, void *arg);
    void *args;
}vespercsi_netlink_command_t;

int vespercsi_netlink_init(vespercsi_netlink_t* netlink);

int vespercsi_netlink_destroy(vespercsi_netlink_t* netlink);

int vespercsi_netlink_execute(vespercsi_netlink_t* netlink, vespercsi_netlink_command_t* command);

int vespercsi_netlink_nlvalid_handler(struct nl_msg *msg, void *arg);

int vespercsi_netlink_error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg);

int vespercsi_netlink_finish_handler(struct nl_msg *msg, void *arg);

int vespercsi_netlink_ack_handler(struct nl_msg *msg, void *arg);

#endif
