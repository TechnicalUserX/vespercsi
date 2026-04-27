#include <fcntl.h>
#include <vespercsi/plugin.h>
#include "ir_label.h"

vespercsi_plugin_context_t vespercsi_plugin_context;
ir_label_state_t state = {0};
action_policy_t action_policies[8] = {0};

const char* action_to_string(action_t action){
    switch(action){
        case ACTION_WALK:       return "WALK";
        case ACTION_RUN:        return "RUN";
        case ACTION_JUMP:       return "JUMP";
        case ACTION_STILL:      return "STILL";
        case ACTION_STANDUP:    return "STANDUP";
        case ACTION_SITDOWN:    return "SITDOWN";
        case ACTION_SITSTAND:   return "SITSTAND";
        case ACTION_FALL:       return "FALL";
        default:
        case ACTION_UNKNOWN:    return "UNKNOWN";
    }
}

static inline void set_pin(int pin, int value){
    LOG_VERBOSE(VERBOSITY_2, INFO, "ir_label: Setting pin %d to %d", pin, value);
    switch(value){
        case 1:
            gpiod_line_request_set_value(state.gpio.request, pin, GPIOD_LINE_VALUE_ACTIVE);
            break;
        case 0:
        default:
            gpiod_line_request_set_value(state.gpio.request, pin, GPIOD_LINE_VALUE_INACTIVE);
            break;
    }    
}

static inline void buzzer_sequence_input(void){
    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(500);
    set_pin(GPIO_LINE_BUZZER, 0);
}

static inline void buzzer_sequence_capture_start(void){
    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(1000);
    set_pin(GPIO_LINE_BUZZER, 0);
    usleep(1000);
    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(1000);
    set_pin(GPIO_LINE_BUZZER, 0);    
}

static inline void buzzer_sequence_capture_stop(void){
    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(10000);
    set_pin(GPIO_LINE_BUZZER, 0);
    usleep(10000);
    set_pin(GPIO_LINE_BUZZER, 1);    
    usleep(10000);
    set_pin(GPIO_LINE_BUZZER, 0);
    usleep(10000);
    set_pin(GPIO_LINE_BUZZER, 1);  
    usleep(10000);
}

static inline void buzzer_sequence_action_change(void){
    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(800);
    set_pin(GPIO_LINE_BUZZER, 0);
    usleep(800);
    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(800);
    set_pin(GPIO_LINE_BUZZER, 0);
    usleep(800);
    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(800);    
    set_pin(GPIO_LINE_BUZZER, 0);
}

static inline void buzzer_sequence_capture_periodic_start(void){

    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(1000);
    set_pin(GPIO_LINE_BUZZER, 0);
    usleep(1000);
    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(1000);
    set_pin(GPIO_LINE_BUZZER, 0);    
}

static inline void buzzer_sequence_capture_periodic_stop(){
    set_pin(GPIO_LINE_BUZZER, 1);
    usleep(10000);
    set_pin(GPIO_LINE_BUZZER, 0);
    usleep(10000);
    set_pin(GPIO_LINE_BUZZER, 1);    
    usleep(10000);
}

static inline void red_led_capture_start(){
    set_pin(GPIO_LINE_LED_RED, 1);
}

static inline void red_led_capture_stop(){
    set_pin(GPIO_LINE_LED_RED, 0);
}

static inline void red_led_capture_about_to_start(){
    set_pin(GPIO_LINE_LED_RED, 1);
    usleep(200000);
    set_pin(GPIO_LINE_LED_RED, 0);
    usleep(800000);
    set_pin(GPIO_LINE_LED_RED, 1);
    usleep(200000);
    set_pin(GPIO_LINE_LED_RED, 0);
    usleep(800000);
    set_pin(GPIO_LINE_LED_RED, 1);
    usleep(200000);
    set_pin(GPIO_LINE_LED_RED, 0);
    usleep(800000);    
}


static inline uint64_t wallclock_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}


label_t action_to_label(action_t action){

    switch(action){
        case ACTION_WALK:       return 1;
        case ACTION_RUN:        return 2;
        case ACTION_JUMP:       return 3;
        case ACTION_STILL:      return 4;
        case ACTION_STANDUP:    return 5;
        case ACTION_SITDOWN:    return 6;
        case ACTION_SITSTAND:   return 7;
        case ACTION_FALL:       return 8;
        case ACTION_UNKNOWN:    return 0;
    }
}

action_t command_to_action(command_t cmd){
    
    switch(cmd){
        case COMMAND_SET_ACTION_WALK:       return ACTION_WALK;
        case COMMAND_SET_ACTION_RUN:        return ACTION_RUN;
        case COMMAND_SET_ACTION_JUMP:       return ACTION_JUMP;
        case COMMAND_SET_ACTION_STILL:      return ACTION_STILL;
        case COMMAND_SET_ACTION_STANDUP:    return ACTION_STANDUP;
        case COMMAND_SET_ACTION_SITDOWN:    return ACTION_SITDOWN;
        case COMMAND_SET_ACTION_SITSTAND:   return ACTION_SITSTAND;
        case COMMAND_SET_ACTION_FALL:       return ACTION_FALL;
        default:                            return ACTION_UNKNOWN;
    }
}


void* periodic_runner(void* arg){

    uint64_t action_duration;
    uint64_t action_wait;

    if(action_policies[state.action.current].action_duration > 0){
        action_duration = action_policies[state.action.current].action_duration;
    }else{
        action_duration = ACTION_POLICY_PERIODIC_DEFAULT_DURATION;
    }

    if(action_policies[state.action.current].action_wait > 0){
        action_wait = action_policies[state.action.current].action_wait;
    }else{
        action_wait = ACTION_POLICY_PERIODIC_DEFAULT_WAIT;
    }

    if(state.action.current == ACTION_SITSTAND){
        state.action.effective = ACTION_STANDUP;
    }

    while(1){
        // 1 period

        if(state.capture.stop_periodic_signal){
            state.capture.stop_periodic_signal = false;
            pthread_exit(NULL);
        }

        uint64_t start_time;
        uint64_t current_time;


        start_time = wallclock_us();


        red_led_capture_about_to_start();
        red_led_capture_start();
        buzzer_sequence_capture_periodic_start();

        if(state.action.current == ACTION_SITSTAND){
            if(state.action.effective == ACTION_STANDUP){
                state.action.effective = ACTION_SITDOWN;
            }else{
                state.action.effective = ACTION_STANDUP;
            }
        }

        state.capture.enable = true;

        do{
            current_time = wallclock_us();
            usleep(100);
        }while(current_time - start_time < action_duration);


        
        state.capture.enable = false;

        red_led_capture_stop();
        buzzer_sequence_capture_periodic_stop();

        start_time = wallclock_us();

        do{
            current_time = wallclock_us();
            usleep(100);
        }while(current_time - start_time < action_wait);        

        state.capture.index++;

    }
}

void command_interpreter(command_t cmd){

    buzzer_sequence_input();

    switch(cmd){

        case COMMAND_START_CAPTURE:{
            if(state.capture.is_recording == false){

                if(action_policies[state.action.current].is_periodic){
                    // Starting periodic action
                    LOG(INFO, "ir_label: Starting periodic capture...");

                    red_led_capture_about_to_start();
                    buzzer_sequence_capture_start();
                    
                    state.capture.is_periodic = true;
                    state.capture.is_recording = true;

                    if(pthread_create(&state.periodic_runner_thread, NULL, periodic_runner, NULL) != 0){
                        LOG(FAIL, "ir_label: Cannot create periodic_runner thread");
                        return;
                    }                    

                }else{
                    // Starting continuous action
                    LOG(INFO, "ir_label: Starting continuous capture...");

                    red_led_capture_start();
                    buzzer_sequence_capture_start();

                    state.capture.is_recording = true;
                    state.capture.is_periodic = false;
                    state.capture.enable = true;

                }

            }
        }break;

        case COMMAND_STOP_CAPTURE:{
            if(state.capture.is_recording == true){

                if(state.capture.is_periodic){
                    // Periodic
                    LOG(INFO, "ir_label: Stopping periodic capture...");
                    state.capture.stop_periodic_signal = true;
                    pthread_join(state.periodic_runner_thread, NULL);

                }else{
                    // Continuous
                    LOG(INFO, "ir_label: Stopping continuous capture...");


                    state.capture.is_recording = false;
                    state.capture.enable = false;
                    state.capture.index++; // After every stop, we increase the index by one
                }

                red_led_capture_stop();
                buzzer_sequence_capture_stop();

            }
        }break;

        case COMMAND_SET_ACTION_WALK:
        case COMMAND_SET_ACTION_RUN:
        case COMMAND_SET_ACTION_JUMP:
        case COMMAND_SET_ACTION_STILL:
        case COMMAND_SET_ACTION_STANDUP:
        case COMMAND_SET_ACTION_SITDOWN:
        case COMMAND_SET_ACTION_SITSTAND:
        case COMMAND_SET_ACTION_FALL:{

            if(!state.capture.is_recording){

                state.command.current = cmd;

                if(state.command.current != state.command.previous){

                    state.action.current =  command_to_action(state.command.current);

                    LOG(INFO, "ir_label: Changing action to %s", action_to_string(state.action.current));

                    if(state.action.current == ACTION_SITSTAND){
                        state.action.effective = ACTION_STANDUP;
                    }else{
                         state.action.effective = state.action.current;
                    }

                    buzzer_sequence_action_change();
                }

                state.command.previous = state.command.current;
            }

        }break;

    }

}

// This thread changes the internal state of the plugin based on infrared remote input
void* infrared_runner(void* arg){

    struct pollfd pfd = { .fd = state.lirc.fd, .events = POLLIN };

    while(1){

        if(state.shutdown_signal){
            pthread_exit(NULL);
        }

        struct lirc_scancode sc = {0};

        int p = poll(&pfd, 1, 200);

        if(p == 0){
            continue;
        }else if(pfd.revents & POLLIN){
            // There is data in the infrared receiver
            int r = read(state.lirc.fd, &sc, sizeof(sc));

            if(r != sizeof(sc)){
                // Bad read
                continue;
            }

            // Good read, make something based on it
            command_t cmd = sc.scancode & 0xFF;
            LOG_VERBOSE(VERBOSITY_0, INFO, "ir_label: Received command: 0x%02X", cmd);
            command_interpreter(cmd);
        }

    }

}

void letgo_resources(void){

    if(state.gpio.request){
        gpiod_line_request_release(state.gpio.request);
    }

    if(state.gpio.chip){
        gpiod_chip_close(state.gpio.chip);
    }

    if(state.lirc.fd > 0){
        close(state.lirc.fd);
    }

    if (state.out_fd > 0){
        close(state.out_fd);
    }    
}

int vespercsi_plugin_function_init(vespercsi_plugin_context_t* context){
    vespercsi_plugin_context = *context;

    if(!context->config.set){
        LOG(FAIL, "ir_label: Config file needed");
        return VESPERCSI_FAIL;
    }

    // Configuration file set

    config_t cfg = {0};

    if (!config_read_file(&cfg, context->config.path)) {
        LOG(FAIL, "ir_label: config error: %s (line %d)\n",
            config_error_text(&cfg),
            config_error_line(&cfg));

            letgo_resources();
        config_destroy(&cfg);
        return VESPERCSI_FAIL;
    }

    int temp_int = 0;
    const char* temp_str = NULL;


    if(config_lookup_string(&cfg, "out", &temp_str)){

        state.out_fd = open(temp_str, O_CREAT | O_APPEND | O_WRONLY, 0644);

        if(state.out_fd < 0){
            LOG(FAIL, "ir_label: Cannot open file: %s: %s",temp_str, strerror(errno));
            return VESPERCSI_FAIL;
        }
        strncpy(state.out_path, temp_str, sizeof(state.out_path)-1);

    }else{
        LOG(FAIL, "ir_label: Output file path not set in configuration file");
        return VESPERCSI_FAIL;
    }

    // Walk
    if(config_lookup_bool(&cfg, "action.walk.periodic", &temp_int)){
        action_policies[ACTION_WALK].is_periodic = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.walk.duration", &temp_int)){
        action_policies[ACTION_WALK].action_duration = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.walk.wait", &temp_int)){
        action_policies[ACTION_WALK].action_wait = temp_int;
    }

    // Run
    if(config_lookup_bool(&cfg, "action.run.periodic", &temp_int)){
        action_policies[ACTION_RUN].is_periodic = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.run.duration", &temp_int)){
        action_policies[ACTION_RUN].action_duration = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.run.wait", &temp_int)){
        action_policies[ACTION_RUN].action_wait = temp_int;
    }        

    // Jump
    if(config_lookup_bool(&cfg, "action.jump.periodic", &temp_int)){
        action_policies[ACTION_JUMP].is_periodic = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.jump.duration", &temp_int)){
        action_policies[ACTION_JUMP].action_duration = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.jump.wait", &temp_int)){
        action_policies[ACTION_JUMP].action_wait = temp_int;
    }            

    // Still
    if(config_lookup_bool(&cfg, "action.still.periodic", &temp_int)){
        action_policies[ACTION_STILL].is_periodic = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.still.duration", &temp_int)){
        action_policies[ACTION_STILL].action_duration = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.still.wait", &temp_int)){
        action_policies[ACTION_STILL].action_wait = temp_int;
    }     

    // Stand up
    if(config_lookup_bool(&cfg, "action.standup.periodic", &temp_int)){
        action_policies[ACTION_STANDUP].is_periodic = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.standup.duration", &temp_int)){
        action_policies[ACTION_STANDUP].action_duration = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.standup.wait", &temp_int)){
        action_policies[ACTION_STANDUP].action_wait = temp_int;
    }     

    // Sit down
    if(config_lookup_bool(&cfg, "action.sitdown.periodic", &temp_int)){
        action_policies[ACTION_SITDOWN].is_periodic = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.sitdown.duration", &temp_int)){
        action_policies[ACTION_SITDOWN].action_duration = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.sitdown.wait", &temp_int)){
        action_policies[ACTION_SITDOWN].action_wait = temp_int;
    }    

    // Sit down and stand up
    if(config_lookup_bool(&cfg, "action.sitstand.periodic", &temp_int)){
        action_policies[ACTION_SITSTAND].is_periodic = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.sitstand.duration", &temp_int)){
        action_policies[ACTION_SITSTAND].action_duration = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.sitstand.wait", &temp_int)){
        action_policies[ACTION_SITSTAND].action_wait = temp_int;
    }    

    // Fall
    if(config_lookup_bool(&cfg, "action.fall.periodic", &temp_int)){
        action_policies[ACTION_FALL].is_periodic = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.fall.duration", &temp_int)){
        action_policies[ACTION_FALL].action_duration = temp_int;
    }
    if(config_lookup_bool(&cfg, "action.fall.wait", &temp_int)){
        action_policies[ACTION_FALL].action_wait = temp_int;
    }    

    config_destroy(&cfg);


    //==========================================================//
    //==================== Infrared Setup ======================//

    state.lirc.fd = open(LIRC_DEV, O_RDONLY);

    if(state.lirc.fd < 0){
        LOG(FAIL, "ir_label: Cannot open %s", LIRC_DEV);
        return VESPERCSI_FAIL;
    }

    unsigned int mode = LIRC_MODE_SCANCODE;
    if (ioctl(state.lirc.fd, LIRC_SET_REC_MODE, &mode) < 0) {
        LOG(FAIL, "ir_label: ioctl LIRC_SET_REC_MODE failed");
        letgo_resources();
        return VESPERCSI_FAIL;
    }    

    if (ioctl(state.lirc.fd, LIRC_GET_REC_MODE, &mode) < 0) {
        LOG(FAIL, "ir_label: ioctl LIRC_GET_REC_MODE failed");
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    if(mode != LIRC_MODE_SCANCODE){
        LOG(FAIL, "ir_label: mode is not set to LIRC_MODE_SCANCODE");
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    //==========================================================//
    //======================= GPIO Setup =======================//


    state.gpio.chip = gpiod_chip_open(GPIO_DEV);
    if (!state.gpio.chip) {
        LOG(FAIL, "ir_label: GPIO device %s cannot be opened", GPIO_DEV);
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    // 1. Create a generic settings profile for an Output line initialized to 0 (Inactive)
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if(!settings){
        LOG(FAIL, "ir_label: Cannot create GPIO line settings");
        letgo_resources();
        return VESPERCSI_FAIL;
    }
    
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    // 2. Map the settings to the specific offsets you intend to use
    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    if(!line_cfg){
        LOG(FAIL, "ir_label: Cannot create GPIO line config");
        gpiod_line_settings_free(settings);
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    unsigned int offsets[] = {
        GPIO_LINE_LED_RED, 
        GPIO_LINE_BUZZER
    };

    if(gpiod_line_config_add_line_settings(line_cfg, offsets, 2, settings) != 0){
        LOG(FAIL, "ir_label: Cannot add GPIO line settings to config");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    // 3. Set standard request metadata (like the consumer string)
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    if(!req_cfg){
        LOG(FAIL, "ir_label: Cannot create GPIO request config");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        letgo_resources();
        return VESPERCSI_FAIL;
    }
    
    gpiod_request_config_set_consumer(req_cfg, "vespercsi-plugin-ir-label");

    // 4. Submit the request to reserve all the lines at once
    state.gpio.request = gpiod_chip_request_lines(state.gpio.chip, req_cfg, line_cfg);

    // 5. Free the configuration boilerplate (it is no longer needed after the request)
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    if(!state.gpio.request){
        LOG(FAIL, "ir_label: Cannot request GPIO lines");
        letgo_resources();
        return VESPERCSI_FAIL;
    }
    
    if(pthread_create(&state.infrared_runner_thread, NULL, infrared_runner, NULL) != 0){
        LOG(FAIL, "ir_label: Cannot create infrared_runner thread");
        letgo_resources();
        return VESPERCSI_FAIL;
    }else{
        LOG_VERBOSE(VERBOSITY_0, INFO, "ir_label: Created infrared_runner thread");
    }

    LOG(INFO, "ir_label: Plugin initalized");
    return VESPERCSI_OK;
}

int vespercsi_plugin_function_callback(const vespercsi_csi_t* csi){

    if(state.capture.enable){

        ir_label_header_t ir_label_header;

        label_t current_label = 0;

        ir_label_header.label = action_to_label(state.action.effective);
        ir_label_header.index = state.capture.index;

        struct iovec iov[3];

        iov[0].iov_base = (void*)&ir_label_header;
        iov[0].iov_len = sizeof(ir_label_header);
        iov[1].iov_base = (void*)&csi->header;
        iov[1].iov_len  = VESPERCSI_CSI_HEADER_LENGTH;
        iov[2].iov_base = (void*)&csi->data.csi_data;
        iov[2].iov_len  = csi->header.csi_data_size;

        size_t io_write_size = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len; 

        ssize_t s = writev(state.out_fd, iov, 3);

        if(s == io_write_size){
            LOG_VERBOSE(VERBOSITY_2, INFO,"ir_label: CSI with size %ld written to file %s", io_write_size, state.out_path);
        }else{
            LOG_VERBOSE(VERBOSITY_2, WARN, "ir_label: CSI with size %ld data not written: %s", io_write_size, strerror(errno));
        }        

    }

    return VESPERCSI_OK;
}

int vespercsi_plugin_function_destroy(void){

    LOG_VERBOSE(VERBOSITY_1, INFO, "ir_label: Stopping infrared_runner thread");
    state.shutdown_signal = true;
    pthread_join(state.infrared_runner_thread, NULL);


    LOG(INFO, "ir_label: Plugin destroyed");
    return VESPERCSI_OK;
}
