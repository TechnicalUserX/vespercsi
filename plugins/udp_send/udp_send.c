#include <vespercsi/plugin.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <libconfig.h>

vespercsi_plugin_context_t vespercsi_plugin_context;

// Plugin state
static int sockfd = -1;
static struct sockaddr_in dest_addr;
static bool plugin_ready = false;

void vespercsi_plugin_function_init(vespercsi_plugin_context_t* context) {
    vespercsi_plugin_context = *context;
    
    config_t cfg;
    config_init(&cfg);

    // Set fallback defaults
    const char *ip = "127.0.0.1";
    int port = 5555;

    // Read the libconfig file
    if (!config_read_file(&cfg, "/etc/vespercsi/plugins/udp_send.conf")) {
        LOG(WARN, "udp_send: Could not read /etc/vespercsi/plugins/udp_send.conf. Using default %s:%d. Reason: %s at line %d", 
            ip, port, config_error_text(&cfg), config_error_line(&cfg));
    } else {
        // Read values if they exist in the file
        config_lookup_string(&cfg, "ip", &ip);
        config_lookup_int(&cfg, "port", &port);
        LOG(INFO, "udp_send: Loaded config - IP: %s, Port: %d", ip, port);
    }

    // Setup UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        LOG(FAIL, "udp_send: Failed to create UDP socket");
        config_destroy(&cfg);
        return;
    }

    // Setup destination address struct
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &dest_addr.sin_addr) <= 0) {
        LOG(FAIL, "udp_send: Invalid IP address parsed (%s)", ip);
        close(sockfd);
        sockfd = -1;
        config_destroy(&cfg);
        return;
    }

    // Free config memory now that the IP is safely loaded into the sockaddr struct
    config_destroy(&cfg);

    plugin_ready = true;
    LOG(INFO, "udp_send: Plugin initialized successfully");
}

void vespercsi_plugin_function_callback(const vespercsi_csi_t* csi) {
    if (!plugin_ready || sockfd < 0) {
        return;
    }

    // Use Scatter/Gather IO to avoid slow memory copies while bypassing the 4096-byte padding
    struct iovec iov[2];
    
    // 1. Point to the header
    iov[0].iov_base = (void*)&csi->header;
    iov[0].iov_len = sizeof(vespercsi_csi_header_t); 

    // 2. Point to the data, reading only the valid data length specified in the header
    iov[1].iov_base = (void*)&csi->data.csi_data;
    iov[1].iov_len = csi->header.csi_data_size;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = iov;
    msg.msg_iovlen = 2; // We are sending 2 concatenated blocks

    // Send the packet
    ssize_t sent_bytes = sendmsg(sockfd, &msg, 0);
    
    if (sent_bytes < 0) {
        LOG(WARN, "udp_send: Failed to send UDP packet");
    } else {
        LOG_VERBOSE(VERBOSITY_1, DEBG, "udp_send: Sent %zd bytes over UDP", sent_bytes);
    }
}

void vespercsi_plugin_function_destroy(void) {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    plugin_ready = false;
    LOG(INFO, "udp_send: Plugin destroyed");
}
