#include <vespercsi/netlink.h>
#include <vespercsi/log.h>

int vespercsi_netlink_init(vespercsi_netlink_t* netlink){

    netlink->netlink_socket = nl_socket_alloc();

    if(!netlink->netlink_socket){
        LOG(FAIL, "Cannot allocate a netlink socket");
        return VESPERCSI_FAIL;
    }

    if(genl_connect(netlink->netlink_socket)){
        LOG(FAIL, "Cannot connect to netlink socket");
        return VESPERCSI_FAIL;
    }

    nl_socket_set_buffer_size(netlink->netlink_socket, 16000, 16000);

    int err = 1;
    setsockopt(nl_socket_get_fd(netlink->netlink_socket), SOL_NETLINK, NETLINK_EXT_ACK, &err, sizeof(err));

    netlink->nl80211_id = genl_ctrl_resolve(netlink->netlink_socket, "nl80211");

    if(netlink->nl80211_id  < 0){
        LOG(FAIL, "Cannot resolve nl80211 id");
        return VESPERCSI_FAIL;
    }

    return VESPERCSI_OK;
}

int vespercsi_netlink_execute(vespercsi_netlink_t* netlink, vespercsi_netlink_command_t* command){

    int err = 0;
    //char err_msg[128] = {0};

    struct nl_msg* msg = nlmsg_alloc();

    if(!msg){
        LOG(FAIL, "Failed to allocate netlink message");
        return VESPERCSI_FAIL;
    }

    struct nl_cb *cb = nl_cb_alloc(NL_CB_DEFAULT);
    struct nl_cb *s_cb = nl_cb_alloc(NL_CB_DEFAULT);

    if (!cb || !s_cb){
        LOG(FAIL, "Failed to allocate netlink callbacks");
        nl_cb_put(cb);
        nl_cb_put(s_cb);
        nlmsg_free(msg);
        return VESPERCSI_FAIL;
    }

    genlmsg_put(msg, 0, 0, netlink->nl80211_id , 0, command->nlFlags, command->id, 0);

    switch (command->idby)
    {
    case VESPERCSI_NETLINK_CIB_PHY:
        NLA_PUT_U32(msg, NL80211_ATTR_WIPHY, command->device);
        break;
    case VESPERCSI_NETLINK_CIB_NETDEV:
        NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, command->device);
        break;
    case VESPERCSI_NETLINK_CIB_WDEV:
        NLA_PUT_U64(msg, NL80211_ATTR_WDEV, command->device);
        break;
    default:
        break;
    }

    if (command->handler){
        err = command->handler(netlink, msg, command->args);
    }

    if(err){
        nl_cb_put(cb);
        nl_cb_put(s_cb);
        nlmsg_free(msg);           
        return VESPERCSI_FAIL;
    }

    nl_socket_set_cb(netlink->netlink_socket, s_cb);

    err = nl_send_auto_complete(netlink->netlink_socket, msg);

    if(err < 0){
        return VESPERCSI_FAIL;
    }

    err = 1;

    nl_cb_err(cb, NL_CB_CUSTOM, vespercsi_netlink_error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, vespercsi_netlink_finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, vespercsi_netlink_ack_handler, &err);
    nl_cb_set(cb, NL_CB_MSG_IN, NL_CB_CUSTOM, command->valid_handler ? command->valid_handler : vespercsi_netlink_nlvalid_handler, command->args);
    
    while(err > 0){
        nl_recvmsgs(netlink->netlink_socket, cb);
    }

    nl_cb_put(cb);
    nl_cb_put(s_cb);
    nlmsg_free(msg);        

    if(err == 0){
        return VESPERCSI_OK;
    }

    LOG(FAIL, "nl_recvmsgs() failure: %s", strerror(-err));
    return VESPERCSI_FAIL;
nla_put_failure:
    LOG(FAIL, "NLA_PUT_32() failure");
    return VESPERCSI_FAIL;
}

int vespercsi_netlink_nlvalid_handler(struct nl_msg *msg, void *arg){
    return NL_OK;
}

int vespercsi_netlink_error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg){
    struct nlmsghdr *nlh = (struct nlmsghdr *)err - 1;
    int len = nlh->nlmsg_len;
    struct nlattr *attrs;
    struct nlattr *tb[NLMSGERR_ATTR_MAX + 1];
    int *ret = (int *)arg;
    int ack_len = sizeof(*nlh) + sizeof(int) + sizeof(*nlh);
    
    if (err->error > 0)
    {
        /*
         * This is illegal, per netlink(7), but not impossible (think
         * "vendor commands"). Callers really expect negative error
         * codes, so make that happen.
         */

        exit(EXIT_FAILURE);
        // throw std::ios_base::failure("ERROR: received positive netlink error code " + std::to_string(err->error) + "\n");
        //*ret = -EPROTO;
    }
    else
    {
        *ret = err->error;
    }

    if (!(nlh->nlmsg_flags & NLM_F_ACK_TLVS)){
        return NL_STOP;
    }

    if (!(nlh->nlmsg_flags & NLM_F_CAPPED)){

        ack_len += err->msg.nlmsg_len - sizeof(*nlh);
    }

    if (len <= ack_len){
        return NL_STOP;
    }

    attrs = (struct nlattr *)((unsigned char *)nlh + ack_len);
    len -= ack_len;

    
    nla_parse(tb, NLMSGERR_ATTR_MAX, attrs, len, NULL);
    if (tb[NLMSGERR_ATTR_MSG])
    {
        LOG(FAIL, "Kernel says: %s", (char *)nla_data(tb[NLMSGERR_ATTR_MSG]));
        len = strnlen((char *)nla_data(tb[NLMSGERR_ATTR_MSG]),
                      nla_len(tb[NLMSGERR_ATTR_MSG]));
        exit(EXIT_FAILURE);
        //throw std::ios_base::failure("kernel reports: " + std::string((char *)nla_data(tb[NLMSGERR_ATTR_MSG])) + "\n");
    }else{
        printf("no?\n");
    }

    return NL_STOP;
}

int vespercsi_netlink_finish_handler(struct nl_msg *msg, void *arg){
    int *ret = (int *)arg;
    *ret = 0;
    return NL_SKIP;
}

int vespercsi_netlink_ack_handler(struct nl_msg *msg, void *arg){
    int *ret = (int *)arg;
    *ret = 0;
    return NL_STOP;
}
