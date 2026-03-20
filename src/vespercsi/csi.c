#include <vespercsi/options.h>
#include <vespercsi/csi.h>
#include <vespercsi/log.h>
#include <vespercsi/wifi.h>
#include <vespercsi/config.h>
#include <vespercsi/plugin.h>

vespercsi_csi_controller_t vespercsi_csi_controller;

vespercsi_csi_ringbuffer_listpool_t* vespercsi_csi_ringbuffer_capture = NULL;
vespercsi_csi_ringbuffer_listpool_t* vespercsi_csi_ringbuffer_consume = NULL;
atomic_flag vespercsi_csi_ringbuffer_lock = ATOMIC_FLAG_INIT;

LISTPOOL_TEMPLATE_DEFINITION(vespercsi_csi_ringbuffer, VESPERCSI_CSI_RINGBUFFER, vespercsi_csi_t, VESPERCSI_CSI_SUPPORTED_RATE_MAX * VESPERCSI_CSI_SUPPORTED_TIME_FRAME_MAX)

atomic_int vespercsi_capture_dirty_flag = ATOMIC_VAR_INIT(0);

void vespercsi_csi_controller_init(vespercsi_csi_controller_t* controller){

    if(vespercsi_netlink_init(&vespercsi_csi_controller.netlink) == VESPERCSI_FAIL){
       fprintf(stdout, "VesperCSI: Cannot initalize CSI Controller netlink");
       exit(EXIT_FAILURE); 
    }

    vespercsi_csi_ringbuffer_listpool_init(&controller->vespercsi_csi_ringbuffer_1);
    vespercsi_csi_ringbuffer_listpool_init(&controller->vespercsi_csi_ringbuffer_2);

    vespercsi_csi_ringbuffer_capture = &controller->vespercsi_csi_ringbuffer_1;
    vespercsi_csi_ringbuffer_consume = &controller->vespercsi_csi_ringbuffer_2;

    if(vespercsi_options.out_file_set){
        controller->vespercsi_csi_out_file = open(vespercsi_options.out_file_path, O_CREAT | O_APPEND | O_WRONLY, 0644);

        if (controller->vespercsi_csi_out_file == -1) {
            fprintf(stdout, "VesperCSI: CSI output file cannot be opened at %s\n", vespercsi_options.out_file_path);
            exit(EXIT_FAILURE);
        }
    }
}

void vespercsi_csi_capture_start(void){
    LOG(INFO, "Capture Runner started");
    pthread_create(&vespercsi_csi_controller.runner_capture_thread, NULL, vespercsi_csi_capture_runner, NULL);
}

void vespercsi_csi_consume_start(void){
    LOG(INFO, "Consume Runner started");
    pthread_create(&vespercsi_csi_controller.runner_consume_thread, NULL, vespercsi_csi_consume_runner, NULL);
}

void vespercsi_csi_ringbuffer_swap_handler(void){
    vespercsi_csi_ringbuffer_listpool_t* temp = vespercsi_csi_ringbuffer_capture;
    vespercsi_csi_ringbuffer_capture = vespercsi_csi_ringbuffer_consume;
    vespercsi_csi_ringbuffer_consume = temp;
}

void vespercsi_csi_runner_set_realtime_priority(int priority){
    struct sched_param param;
    param.sched_priority = priority; // Highest RT priority
    
    // SCHED_FIFO means the thread runs until it voluntarily yields or blocks
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        LOG(FAIL,"Failed to set RT priority");
    }
}

void vespercsi_csi_runner_set_cpu_core(int core){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset); 
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void* vespercsi_csi_capture_runner(void* arg){

    if(vespercsi_config.runners.capture.core_set){
        vespercsi_csi_runner_set_cpu_core(vespercsi_config.runners.capture.core);
    }

    if(vespercsi_config.runners.capture.priority_set){
        if(vespercsi_config.runners.capture.core_set == false){
            LOG_VERBOSE(WARN, "Core not set for consume runner, potential system instability");
        }        
        vespercsi_csi_runner_set_realtime_priority(vespercsi_config.runners.capture.priority);
    }

    vespercsi_csi_listen(&vespercsi_csi_controller);

    return NULL;
}

void* vespercsi_csi_consume_runner(void* arg){
    
    if(vespercsi_config.runners.consume.core_set){
        vespercsi_csi_runner_set_cpu_core(vespercsi_config.runners.consume.core);
    }

    if(vespercsi_config.runners.consume.priority_set){
        if(vespercsi_config.runners.consume.core_set == false){
            LOG_VERBOSE(WARN, "Core not set for consume runner, potential system instability");
        }
        vespercsi_csi_runner_set_realtime_priority(vespercsi_config.runners.consume.priority);
    }

    static uint64_t counter = 0;

    while(1){
        vespercsi_csi_t csi = {0};
        
        vespercsi_csi_ringbuffer_listpool_status_t status = vespercsi_csi_ringbuffer_listpool_pop_back(vespercsi_csi_ringbuffer_consume, &csi);

        if(status == VESPERCSI_CSI_RINGBUFFER_LISTPOOL_STATUS_SUCCESS){
            counter++;
            // Process your CSI packet instantly
            LOG_VERBOSE(DEBG, "Consume CSI %lu", counter);
            
            // Plugin Callback
            if(vespercsi_plugin.loaded){
                vespercsi_plugin.callback(&csi);
            }

            if(vespercsi_options.out_file_set){
                uint8_t io_write_buffer[VESPERCSI_CSI_SIZE_MAX] = {0};

                memcpy(io_write_buffer, &csi.header, VESPERCSI_CSI_HEADER_LENGTH);
                memcpy(io_write_buffer + VESPERCSI_CSI_HEADER_LENGTH, csi.data.csi_data, csi.header.csi_data_size);

                size_t io_write_size = VESPERCSI_CSI_HEADER_LENGTH + csi.header.csi_data_size;
                ssize_t s = write(vespercsi_csi_controller.vespercsi_csi_out_file, io_write_buffer, io_write_size);                

                if(s == VESPERCSI_CSI_HEADER_LENGTH + csi.header.csi_data_size){
                    LOG_VERBOSE(INFO,"CSI with size %ld written to file %s", io_write_size, vespercsi_options.out_file_path);
                }else{
                    LOG_VERBOSE(WARN, "CSI with size %ld data not written: %s", io_write_size, strerror(errno));
                }
            }

        }else{
            // BUFFER IS EMPTY. 
            // 1. PEEK: Do a cheap, relaxed read to see if the flag is 1.
            // This does NOT lock the memory bus.
            if (atomic_load_explicit(&vespercsi_capture_dirty_flag, memory_order_relaxed) == 1) {
                // 2. CLEAR: Safely reset the flag to 0. 
                // memory_order_acquire pairs with the capture thread's release.
                atomic_exchange_explicit(&vespercsi_capture_dirty_flag, 0, memory_order_acquire);

                // 3. SWAP: Now that the flag is clear, perform the heavy buffer swap.
                while(atomic_flag_test_and_set_explicit(&vespercsi_csi_ringbuffer_lock, memory_order_acquire));
                vespercsi_csi_ringbuffer_swap_handler();
                atomic_flag_clear_explicit(&vespercsi_csi_ringbuffer_lock, memory_order_release);
            
            }else{
                // STILL TRULY EMPTY. Flag is 0.
                __asm__ __volatile__("yield" ::: "memory");
            }
        }
    }
    return NULL;
}

int vespercsi_csi_listen(vespercsi_csi_controller_t* controller){
    vespercsi_netlink_command_t command = {
        .id = NL80211_CMD_VENDOR,
        .idby = VESPERCSI_NETLINK_CIB_NETDEV,
        .nlFlags = 0,
        .device = if_nametoindex(VESPERCSI_WIFI_MONITOR_INTERFACE),
        .handler = vespercsi_csi_listen_handler,
        .valid_handler = vespercsi_csi_capture_handler,
    };

    return vespercsi_netlink_execute(&controller->netlink, &command);
}

int vespercsi_csi_listen_handler(struct vespercsi_netlink_t *state, struct nl_msg *msg, void *arg){
    NLA_PUT_U32(msg, NL80211_ATTR_VENDOR_ID, 0x001735);
    NLA_PUT_U32(msg, NL80211_ATTR_VENDOR_SUBCMD, 0x24);
    return 0;
nla_put_failure:
    return -ENOBUFS;
}

int vespercsi_csi_capture_handler(struct nl_msg *msg, void *arg){

    static uint64_t counter = 0;

    static vespercsi_csi_metadata_t current_csi_metadata = {0};
    static vespercsi_csi_data_t current_csi_data = {0};

    struct nlattr *attrs[MAX_CMD + 1];
    struct nlmsghdr *nlh = nlmsg_hdr(msg);

    nlmsg_parse(nlh, 32, attrs, MAX_CMD, NULL);

    if (attrs[IWL_MVM_VENDOR_ATTR_CSI_HDR]){

        unsigned int header_length = nla_len(attrs[IWL_MVM_VENDOR_ATTR_CSI_HDR]);

        if(header_length != VESPERCSI_CSI_HEADER_LENGTH){
            return NL_SKIP;
        }

        uint8_t *header = (uint8_t *)nla_data(attrs[IWL_MVM_VENDOR_ATTR_CSI_HDR]);

        memcpy(&current_csi_metadata.header, header, header_length);
        
        if(vespercsi_options.src_mac_set && memcmp(current_csi_metadata.header.src_mac, vespercsi_options.src_mac, 6) != 0){
            return NL_SKIP;
        }

        if (attrs[IWL_MVM_VENDOR_ATTR_CSI_DATA]){

            unsigned int data_length = nla_len(attrs[IWL_MVM_VENDOR_ATTR_CSI_DATA]);

            uint8_t* data = (uint8_t *)nla_data(attrs[IWL_MVM_VENDOR_ATTR_CSI_DATA]);
            
            memcpy(current_csi_data.csi_data, data, data_length);

            current_csi_metadata.num_rx = current_csi_metadata.header.num_rx;
            current_csi_metadata.num_tx = current_csi_metadata.header.num_tx;
            current_csi_metadata.num_subcarriers = current_csi_metadata.header.num_subcarriers;
            current_csi_metadata.format = current_csi_metadata.header.rateNflag & RATE_MCS_MOD_TYPE_MSK;
            current_csi_metadata.channel_width = current_csi_metadata.header.rateNflag & RATE_MCS_CHAN_WIDTH_MSK;

            if(vespercsi_csi_fix_bugs(&current_csi_metadata, &current_csi_data) == VESPERCSI_FAIL){
                return NL_SKIP;
            }

            if(
                (current_csi_metadata.channel_width == RATE_MCS_CHAN_WIDTH_20 && vespercsi_options.channel_width == 20) ||
                (current_csi_metadata.channel_width== RATE_MCS_CHAN_WIDTH_40 && vespercsi_options.channel_width == 40) ||
                (current_csi_metadata.channel_width == RATE_MCS_CHAN_WIDTH_80 && vespercsi_options.channel_width == 80) ||
                (current_csi_metadata.channel_width== RATE_MCS_CHAN_WIDTH_160 && vespercsi_options.channel_width == 160)
            
            ){
                if(
                    (current_csi_metadata.format == RATE_MCS_LEGACY_OFDM_MSK && strcmp(vespercsi_options.format ,"NOHT") == 0) ||
                    (current_csi_metadata.format == RATE_MCS_HT_MSK && strcmp(vespercsi_options.format ,"HT") == 0) ||
                    (current_csi_metadata.format == RATE_MCS_VHT_MSK && strcmp(vespercsi_options.format ,"VHT") == 0) ||
                    (current_csi_metadata.format == RATE_MCS_HE_MSK && strcmp(vespercsi_options.format ,"HESU") == 0) ||
                    (current_csi_metadata.format == RATE_MCS_EHT_MSK && strcmp(vespercsi_options.format ,"EHT") == 0)
                ){
                    // CSI Captured
                    counter++;

                    current_csi_metadata.header.timestamp = vespercsi_csi_wallclock_us();

                    vespercsi_csi_t csi = {
                        current_csi_metadata.header,
                        current_csi_data
                    };

                    while (atomic_flag_test_and_set_explicit(&vespercsi_csi_ringbuffer_lock, memory_order_acquire));
                    vespercsi_csi_ringbuffer_listpool_status_t status = vespercsi_csi_ringbuffer_listpool_push_front(vespercsi_csi_ringbuffer_capture, &csi);
                    atomic_flag_clear_explicit(&vespercsi_csi_ringbuffer_lock, memory_order_release);

                    switch(status){
                        case VESPERCSI_CSI_RINGBUFFER_LISTPOOL_STATUS_SUCCESS:{
                            LOG_VERBOSE(DEBG, "Pushed CSI %lu", counter);
                        }break;

                        case VESPERCSI_CSI_RINGBUFFER_LISTPOOL_STATUS_NO_MEMORY:{
                            LOG_VERBOSE(DEBG, "No memory to push CSI");
                        }break;

                        case VESPERCSI_CSI_RINGBUFFER_LISTPOOL_STATUS_NULL_VALUE:{
                            LOG_VERBOSE(DEBG, "Null value whiile pushing CSI");
                        }break;

                        case VESPERCSI_CSI_RINGBUFFER_LISTPOOL_STATUS_EMPTY:{
                            LOG_VERBOSE(DEBG, "Empty buffer?");
                        }break;                        
                        
                        default:break;
                    }

                    char mac_string[18] = {0};
                    vespercsi_wifi_mac_string(mac_string, current_csi_metadata.header.src_mac);

                    atomic_store_explicit(&vespercsi_capture_dirty_flag, 1, memory_order_release);
                    LOG_VERBOSE(INFO, "CSI Capture");
                }

            }

        }
    }

    // We should probably do timeout checking here for termination

    /*
    WiFiCsiController *wcc = (WiFiCsiController*) arg;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (wcc->stopTime != 0 && wcc->stopTime < now)
    {
        return NL_OK;
    }
    */
    return NL_SKIP;
}

int vespercsi_csi_fix_bugs(vespercsi_csi_metadata_t* metadata, vespercsi_csi_data_t* data){
    if (metadata->channel_width != RATE_MCS_CHAN_WIDTH_160){
        return VESPERCSI_OK;
    }

    if (metadata->format != RATE_MCS_VHT_MSK && metadata->format != RATE_MCS_HE_MSK){
        return VESPERCSI_OK;
    }

    uint16_t new_subcarrier_size = 0;

    if (metadata->format == RATE_MCS_VHT_MSK && metadata->header.num_subcarriers == 484){
        return VESPERCSI_OK;
    }else if(metadata->format == RATE_MCS_VHT_MSK){
        new_subcarrier_size = 484;
    }

    if (metadata->format == RATE_MCS_HE_MSK && metadata->header.num_subcarriers == 1992){
        return VESPERCSI_OK;
    }else if(metadata->format == RATE_MCS_HE_MSK){
        new_subcarrier_size = 1992;
    }

    for (uint32_t i = 0; i < metadata->header.csi_data_size; i = i + 4){
        if (metadata->format == RATE_MCS_VHT_MSK){
            if (i > (241 * 4) && i < (256 * 4)) // Fix the firmware bug
                continue;
        }
    }

    uint32_t new_total_size = new_subcarrier_size * 4 *metadata->num_rx * metadata->num_tx;

    if(new_total_size > VESPERCSI_CSI_DATA_SIZE_MAX){
        return VESPERCSI_FAIL;
    }

    uint8_t fixed_csi_data[VESPERCSI_CSI_DATA_SIZE_MAX] = {0};

    uint32_t newIndex = 0;
    uint32_t oldIndex = 0;

    for (uint32_t rx = 0; rx < metadata->num_rx; rx++)
    {
        for (uint32_t tx = 0; tx < metadata->num_tx; tx++)
        {
            for (uint32_t n = 0; n < metadata->num_subcarriers; n++)
            {
                if (metadata->format == RATE_MCS_VHT_MSK)
                {
                    if (n > 241 && n < 256)
                    {
                        oldIndex += 4;
                        continue;
                    }
                }
                if (metadata->format == RATE_MCS_HE_MSK)
                {
                    if (n > 995 && n < 1024)
                    {
                        oldIndex += 4;
                        continue;
                    }
                }
                memcpy(&fixed_csi_data[newIndex], &data->csi_data[oldIndex], 4);
                oldIndex += 4;
                newIndex += 4;
            }
        }
    }

    metadata->num_subcarriers = new_subcarrier_size;
    metadata->header.num_subcarriers = metadata->num_subcarriers;
    metadata->header.csi_data_size = new_total_size;
    memcpy(data->csi_data, fixed_csi_data, new_total_size);
    return VESPERCSI_OK;
}
