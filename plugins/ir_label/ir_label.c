#include <libconfig.h>
#include <vespercsi/plugin.h>
#include "ir_label.h"

vespercsi_plugin_context_t vespercsi_plugin_context = {0};

ir_label_state_t state = {0};

int async_buzzer_pipe[2] = {0};

int state_init(void){
    state.action.current = ACTION_WALK;
    state.action.effective = ACTION_WALK;

    state.out.fd = -1;
    strlcpy(state.out.directory, ".", sizeof(state.out.directory));

    state.infrared_runner_thread = -1;

    state.capture_periodic_runner_thread = -1;
    state.capture.is_labeling = false;

    state.async_buzzer_runner_thread = -1;
    
    if(pipe(async_buzzer_pipe) != 0){
        return VESPERCSI_FAIL;
    }

    if(pthread_mutex_init(&state.out.fd_lock, NULL) != 0){
        return VESPERCSI_FAIL;
    }

    return VESPERCSI_OK;
}

#include <stdio.h>
#include <time.h>
#include <stdint.h>

void precise_sleep_posix(uint64_t usec) {
    struct timespec ts;
    
    // 1. Get current time
    clock_gettime(CLOCK_MONOTONIC, &ts);

    // 2. Add the requested microseconds to the current time
    uint64_t target_nsec = ts.tv_nsec + (usec * 1000);
    
    // Handle nanosecond overflow into seconds
    ts.tv_sec += target_nsec / 1000000000;
    ts.tv_nsec = target_nsec % 1000000000;

    // 3. Sleep until that absolute time
    // TIMER_ABSTIME ensures we wake up exactly at the target, regardless of signals
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
}

static inline uint64_t wallclock_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

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
        case ACTION_EMPTY:      return "EMPTY";
        default:
        case ACTION_NOLABEL:    return "UNKNOWN";
    }
}

const char* generate_new_record_file_name(void){
    static char buffer[PATH_MAX] = {0};
    memset(buffer, 0x0, sizeof(buffer));
    strcpy(buffer, "csi");
    uint64_t ts = 0;

    if(state.out.include.datetime || state.out.include.timestamp){
        ts = wallclock_us();
    }

    if(state.out.include.prefix){
        strlcat(buffer, "_", PATH_MAX);
        strlcat(buffer, state.out.prefix, PATH_MAX);
    }

    if(state.out.include.datetime){
        strlcat(buffer, "_", PATH_MAX);
        char dt_buffer[64] = {0};        
        time_t seconds = (time_t)(ts / (uint64_t)1000000);
        struct tm tm_info = {0};
        localtime_r(&seconds, &tm_info);
        strftime(dt_buffer, sizeof(dt_buffer), "%Y%m%d%H%M%S", &tm_info);
        strlcat(buffer, dt_buffer, PATH_MAX);
    }

    if(state.out.include.timestamp){
        strlcat(buffer, "_", PATH_MAX);
        char ts_buffer[32] = {0};
        snprintf(ts_buffer, sizeof(ts_buffer), "%" PRIu64, ts);
        strlcat(buffer, ts_buffer, PATH_MAX);
    }

    if(state.out.include.action){
        strlcat(buffer, "_", PATH_MAX);
        strlcat(buffer, action_to_string(state.action.current), PATH_MAX);
    }

    if(state.out.include.index){
        // This the current index of the current action, reset at each run
        strlcat(buffer, "_", PATH_MAX);
        char index_buffer[32] = {0};
        snprintf(index_buffer, 32, "%" PRIu64, state.capture.index);
        strlcat(buffer, index_buffer, PATH_MAX);
    }

    if(state.out.include.suffix){
        strlcat(buffer, "_", PATH_MAX);
        strlcat(buffer, state.out.suffix, PATH_MAX);
    }

    if(state.out.include.extension){
        strlcat(buffer, ".", PATH_MAX);
        strlcat(buffer, state.out.extension, PATH_MAX);
    }    

    return buffer;
}

const char* generate_new_record_path(void){

    static char record_path[PATH_MAX] = {0};
    memset(record_path, 0x0, sizeof(record_path));


    if(strlen(state.out.directory) > 0){
        strlcat(record_path, state.out.directory, PATH_MAX);
    }

    const char* new_record_file_name = generate_new_record_file_name();

    if(new_record_file_name == NULL){
        LOG(FAIL, "ir_label: Cannot create a record file name");
        return NULL;
    }

    strlcat(record_path, new_record_file_name, PATH_MAX);

    return record_path;
}

int open_out(void){

    const char* record_path = generate_new_record_path();
    
    if(record_path == NULL){
        return VESPERCSI_FAIL;
    }

    int fd = open(record_path, O_CREAT | O_APPEND | O_WRONLY, 0644);

    if(fd < 0){
        LOG_VERBOSE(VERBOSITY_0, FAIL, "Cannot open output file %s", record_path);
        return VESPERCSI_FAIL;
    }


    pthread_mutex_lock(&state.out.fd_lock);
    state.out.fd = fd;
    strlcpy(state.out.fd_path, record_path, PATH_MAX);
    pthread_mutex_unlock(&state.out.fd_lock);

    return VESPERCSI_OK;
}

int close_out(bool discard){

    pthread_mutex_lock(&state.out.fd_lock);

    if(state.out.fd >= 0){
        close(state.out.fd);
        if(discard){
            if(remove(state.out.fd_path) != 0){
                LOG(WARN, "ir_label: Cannot discard the file: %s", state.out.fd_path);
            }else{
                LOG(INFO, "ir_label: File discarded: %s", state.out.fd_path);
            }
        }else{
            if(state.out.include.index && state.capture.index_autoincrement){
                state.capture.index++;
            }
        }

        strlcpy(state.out.fd_path, "", PATH_MAX);
        state.out.fd = -1;
    }

    pthread_mutex_unlock(&state.out.fd_lock);

    return VESPERCSI_OK;
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
    SLEEP_MILISECONDS(100);
    set_pin(GPIO_LINE_BUZZER, 0);
    SLEEP_MILISECONDS(50);
}

static inline void buzzer_sequence_capture_start(void){
    set_pin(GPIO_LINE_BUZZER, 1);
    SLEEP_MILISECONDS(100);
    set_pin(GPIO_LINE_BUZZER, 0);
    SLEEP_MILISECONDS(50);
    set_pin(GPIO_LINE_BUZZER, 1);
    SLEEP_MILISECONDS(100);
    set_pin(GPIO_LINE_BUZZER, 0);
    SLEEP_MILISECONDS(50);
}

static inline void buzzer_sequence_capture_stop(void){
    set_pin(GPIO_LINE_BUZZER, 1);
    SLEEP_MILISECONDS(400);
    set_pin(GPIO_LINE_BUZZER, 0);
    SLEEP_MILISECONDS(100);
    set_pin(GPIO_LINE_BUZZER, 1);    
    SLEEP_MILISECONDS(150);
    set_pin(GPIO_LINE_BUZZER, 0);
    SLEEP_MILISECONDS(100);
}

static inline void buzzer_sequence_capture_wait(void){
    set_pin(GPIO_LINE_BUZZER, 1);
    SLEEP_MILISECONDS(200);
    set_pin(GPIO_LINE_BUZZER, 0);
    SLEEP_MILISECONDS(50);
}

// Synchronous
static inline void buzzer_sequence_action_change(void){
    set_pin(GPIO_LINE_BUZZER, 1);
    SLEEP_MILISECONDS(50);
    set_pin(GPIO_LINE_BUZZER, 0);
    SLEEP_MILISECONDS(50);
    set_pin(GPIO_LINE_BUZZER, 1);
    SLEEP_MILISECONDS(50);
    set_pin(GPIO_LINE_BUZZER, 0);
    SLEEP_MILISECONDS(50);

}

// Synchronous, 1 second
static inline void buzzer_sequence_hold_one_second(void){
    set_pin(GPIO_LINE_BUZZER, 1);
    SLEEP_MILISECONDS(500);
    set_pin(GPIO_LINE_BUZZER, 0);
    SLEEP_MILISECONDS(500);
}

static inline void red_led_capture_start(){
    set_pin(GPIO_LINE_LED_RED, 1);
}

static inline void red_led_capture_stop(){
    set_pin(GPIO_LINE_LED_RED, 0);
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
        case ACTION_EMPTY:      return 9;
        case ACTION_NOLABEL:    return 0;
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
        case COMMAND_SET_ACTION_EMPTY:      return ACTION_EMPTY;
        default:                            return ACTION_NOLABEL;
    }
}

void action_sitstand_swap(void){
    if(state.action.current == ACTION_SITSTAND){
        if(state.action.effective == ACTION_STANDUP){
            state.action.effective = ACTION_SITDOWN;
        }else{
            state.action.effective = ACTION_STANDUP;
        }
    }
}

int async_buzzer_enqueue(uint8_t buzzer_code){
    if(buzzer_code > 4){
        return VESPERCSI_FAIL;
    }

    int w = write(async_buzzer_pipe[1], &buzzer_code, 1);
    if(w == 1){
        return VESPERCSI_OK;    
    }else{
        return VESPERCSI_FAIL;
    }
}

void* async_buzzer_runner(void* arg){

    struct pollfd pfd = {.fd = async_buzzer_pipe[0], .events = POLLIN};

    while(1){

        if(state.async_buzzer.stop_signal){
            pthread_exit(NULL);
        }

        int r = poll(&pfd, 1, 200);

        if(r == 0){
            // Timeout
            continue;
        }else if(pfd.revents & POLLIN){
            uint8_t byte = 0;
            ssize_t r = read(async_buzzer_pipe[0], &byte, 1);
            if(r != 1){
                continue;
            }
            switch(byte){
                case ASYNC_BUZZER_CODE_CAPTURE_START:{
                    buzzer_sequence_capture_start();
                }break;

                case ASYNC_BUZZER_CODE_CAPTURE_STOP:{
                    buzzer_sequence_capture_stop();
                }break;

                case ASYNC_BUZZER_CODE_CAPTURE_WAIT:{
                    buzzer_sequence_capture_wait();
                }break;

                case ASYNC_BUZZER_CODE_ACTION_CHANGE:{
                    buzzer_sequence_action_change();
                }break;

            }
        }


    }

}

void* capture_periodic_runner(void* arg){

    uint64_t action_duration = 0;
    uint64_t action_wait = 0;

    if(state.action.policies[state.action.current].action_duration > 0){
        action_duration = state.action.policies[state.action.current].action_duration;
    }else{
        action_duration = ACTION_POLICY_PERIODIC_DEFAULT_DURATION;
    }

    if(state.action.policies[state.action.current].action_wait > 0){
        action_wait = state.action.policies[state.action.current].action_wait;
    }else{
        action_wait = ACTION_POLICY_PERIODIC_DEFAULT_WAIT;
    }

    if(state.action.current == ACTION_SITSTAND){
        state.action.effective = ACTION_STANDUP;
    }

    uint64_t counter = 0;

    bool counter_enabled = false;

    if(state.action.policies[state.action.current].action_count > 0){
        counter = state.action.policies[state.action.current].action_count;
        counter_enabled = true;
    }

    while(!counter_enabled || counter > 0){
        // 1 period

        if(state.capture.stop_periodic_signal){
            break;
        }

        uint64_t start_time = 0;
        uint64_t current_time = 0;

        action_sitstand_swap();

        if(!state.capture.enable){

            if(state.action.policies[state.action.current].action_hold){

                // Only enable early if hold_nolabel is true
                if(state.action.policies[state.action.current].action_hold_nolabel){
                    state.capture.is_labeling = false;
                    state.capture.enable = true;
                }

                uint64_t hold_seconds_final = state.action.policies[state.action.current].action_hold_seconds;
                if(hold_seconds_final == 0){
                    hold_seconds_final = 3;
                }
                for(uint64_t i = 0; i < hold_seconds_final; i++){
                    buzzer_sequence_hold_one_second();
                }
                

                // After hold sequence while enabled, labeling is switched to true, labeling is enabled
                // If no hold was issued, labeling is directly set to true and capture is enabled
                state.capture.is_labeling = true;
                state.capture.enable = true; // Ensure it is on for the actual action

            }else{
                state.capture.is_labeling = true;
                state.capture.enable = true;
            }

        }else{
            state.capture.is_labeling = true;
        }

        red_led_capture_start();
        async_buzzer_enqueue(ASYNC_BUZZER_CODE_CAPTURE_START);

        start_time = wallclock_us();
        do{
            if(state.capture.stop_periodic_signal){
                break;
            }
            SLEEP_MILISECONDS(1);
            current_time = wallclock_us();
        }while(current_time - start_time < action_duration);

        state.action.index[ACTION_NOLABEL]++;
        
        state.capture.is_labeling = false;

        red_led_capture_stop();
        async_buzzer_enqueue(ASYNC_BUZZER_CODE_CAPTURE_WAIT);

        start_time = wallclock_us();
        do{
            if(state.capture.stop_periodic_signal){
                break;
            }
            SLEEP_MILISECONDS(1);
            current_time = wallclock_us();
        }while(current_time - start_time < action_wait); 

        state.action.index[state.action.effective]++;
        counter--;
    }

    if (!state.capture.stop_periodic_signal) {
        stop_capture_periodic(true);
    }

    // If stop_periodic_signal IS true, we just exit so pthread_join() can succeed safely
    pthread_exit(NULL);
}

void start_capture_continuous(void){

    if(state.action.current == ACTION_SITSTAND){
        LOG(WARN, "ir_label: Action ACTION_SITSTAND is not allowed in continuous mode");
        return;
    }

    LOG(INFO, "ir_label: Starting continuous capture...");
    
    state.action.index[state.action.effective] = 0;

    if(state.action.policies[state.action.current].action_hold){
        
        uint64_t hold_seconds_final = state.action.policies[state.action.current].action_hold_seconds;
        
        if(hold_seconds_final == 0){
            hold_seconds_final = 3; 
        }
        
        for(uint64_t i = 0; i < hold_seconds_final; i++){
            buzzer_sequence_hold_one_second();
        }
    }

    red_led_capture_start();
    async_buzzer_enqueue(ASYNC_BUZZER_CODE_CAPTURE_START);

    state.capture.is_recording = true;
    state.capture.is_labeling = true;
    state.capture.enable = true;
}

void stop_capture_continuous(void){

    LOG(INFO, "ir_label: Stopping continuous capture...");

    state.capture.enable = false;
    state.capture.is_labeling = false;

    red_led_capture_stop();
    async_buzzer_enqueue(ASYNC_BUZZER_CODE_CAPTURE_STOP);

    state.action.index[state.action.effective] = 0;

    state.capture.is_recording = false;
}

void start_capture_periodic(void){
    LOG(INFO, "ir_label: Starting periodic capture...");

    if(state.action.current == ACTION_SITSTAND){
        state.action.index[ACTION_STANDUP] = 0;
        state.action.index[ACTION_SITDOWN] = 0;
    }else{
        state.action.index[state.action.effective] = 0;
    }

    state.action.index[ACTION_NOLABEL] = 0;

    state.capture.is_recording = true;

    if(pthread_create(&state.capture_periodic_runner_thread, NULL, capture_periodic_runner, NULL) != 0){
        LOG(FAIL, "ir_label: Cannot create capture_periodic_runner thread");
        return;
    }         
}

void stop_capture_periodic(bool self){
    LOG(INFO, "ir_label: Stopping periodic capture...");

    state.capture.enable = false;

    state.action.index[state.action.effective] = 0;
    state.capture.is_recording = false;

    red_led_capture_stop();
    async_buzzer_enqueue(ASYNC_BUZZER_CODE_CAPTURE_STOP);

    if(self){
        close_out(false);
        state.capture_periodic_runner_thread = -1;        
        pthread_detach(pthread_self());
        pthread_exit(NULL);
    }else{
        state.capture.stop_periodic_signal = true;
        pthread_join(state.capture_periodic_runner_thread, NULL);
        state.capture_periodic_runner_thread = -1;
        state.capture.stop_periodic_signal = false;
    }
}

void command_interpreter(command_t cmd){

    state.command.current = cmd;

    switch(cmd){

        case COMMAND_START_CAPTURE:{
            if(state.capture.is_recording == false){

                if(open_out() == VESPERCSI_FAIL){
                    LOG(WARN, "Cannot open a file for writing CSI data");
                    break;
                }

                if(state.action.policies[state.action.current].is_periodic){
                    start_capture_periodic();
                }else{
                    start_capture_continuous();
                }
            }
        }break;

        case COMMAND_DISCARD_CAPTURE:
        case COMMAND_STOP_CAPTURE:{
            if(state.capture.is_recording == true){

                if(state.action.policies[state.action.current].is_periodic){
                    stop_capture_periodic(false);
                }else{
                    stop_capture_continuous();
                }

                if(cmd == COMMAND_DISCARD_CAPTURE){
                    close_out(true);
                }else{
                    close_out(false);
                }
            }
        }break;

        case COMMAND_SET_ACTION_WALK:
        case COMMAND_SET_ACTION_RUN:
        case COMMAND_SET_ACTION_JUMP:
        case COMMAND_SET_ACTION_STILL:
        case COMMAND_SET_ACTION_STANDUP:
        case COMMAND_SET_ACTION_SITDOWN:
        case COMMAND_SET_ACTION_SITSTAND:
        case COMMAND_SET_ACTION_FALL:
        case COMMAND_SET_ACTION_EMPTY:{

            if(!state.capture.is_recording){

                if(state.command.current != state.command.previous){

                    state.action.current =  command_to_action(state.command.current);

                    LOG(INFO, "ir_label: Changing action to %s", action_to_string(state.action.current));

                    if(state.action.current == ACTION_SITSTAND){
                        state.action.effective = ACTION_STANDUP;
                    }else{
                         state.action.effective = state.action.current;
                    }

                        async_buzzer_enqueue(ASYNC_BUZZER_CODE_ACTION_CHANGE);

                }

            }

        }break;

        case COMMAND_CAPTURE_INDEX_INCREASE:{
            state.capture.index++;
            LOG(INFO, "ir_label: Capture index increased to %" PRIu64, state.capture.index);
        }break;

        case COMMAND_CAPTURE_INDEX_DECREASE:{
            if(state.capture.index > 0){
                state.capture.index--;
                LOG(INFO, "ir_label: Capture index decreased to %" PRIu64, state.capture.index);
            }
        }break;

        case COMMAND_CAPTURE_INDEX_TOGGLE_AUTOINCREMENT:{
            if(state.capture.index_autoincrement){
                state.capture.index_autoincrement = false;
                LOG(INFO, "ir_label: Capture index mode set to: manual");
            }else{
                state.capture.index_autoincrement = true;
                LOG(INFO, "ir_label: Capture index mode set to: autoincrement");
            }
        }break;

        default: break;
    }

    state.command.previous = state.command.current;
}

void* infrared_runner(void* arg){
    // This thread changes the internal state of the plugin based on infrared remote input

    struct pollfd pfd = { .fd = state.lirc.fd, .events = POLLIN };

    while(1){

        if(state.shutdown_signal){
            pthread_exit(NULL);
        }

        struct lirc_scancode sc = {0};

        int p = poll(&pfd, 1, 200);

        if(p == 0){ 
            // Timeout
            continue;
        }else if(pfd.revents & POLLIN){
            // There is data in the infrared receiver
            int r = read(state.lirc.fd, &sc, sizeof(sc));

            if(r != sizeof(sc)){
                // Bad read
                continue;
            }

            // Good read, make something based on it

            // Do not allow repeated presses
            if(sc.flags & LIRC_SCANCODE_FLAG_REPEAT){
                // Discard REPEAT press
                LOG_VERBOSE(VERBOSITY_2, DEBG, "ir_label: Discarded command due to repeated press");
                continue;
            }

            command_t cmd = sc.scancode & 0xFF;

            LOG_VERBOSE(VERBOSITY_1, DEBG, "ir_label: Received command: 0x%02X", cmd);
            
            command_interpreter(cmd);
        }

    }

}

void free_resources(void){

    if(state.gpio.request){
        gpiod_line_request_release(state.gpio.request);
        state.gpio.request = NULL;
    }

    if(state.gpio.chip){
        gpiod_chip_close(state.gpio.chip);
        state.gpio.chip = NULL;
    }

    if(state.lirc.fd > 0){
        close(state.lirc.fd);
        state.lirc.fd = -1;
    }

    pthread_mutex_lock(&state.out.fd_lock);
    if (state.out.fd >= 0){
        close(state.out.fd);
        state.out.fd = -1;
    }    
    pthread_mutex_unlock(&state.out.fd_lock);
}

int vespercsi_plugin_function_init(vespercsi_plugin_context_t* context){
    vespercsi_plugin_context = *context;

    if(state_init() == VESPERCSI_FAIL){
        LOG(FAIL, "ir_label: Cannot set initial state");
        free_resources();
        return VESPERCSI_FAIL;
    }

    if(!context->config.set){
        LOG(FAIL, "ir_label: Config file needed");
        free_resources();
        return VESPERCSI_FAIL;
    }

    // Configuration file set //

    if(!config_read_file(&state.config_file, context->config.path)){
        LOG(FAIL, "ir_label: config error: %s (line %d)\n", config_error_text(&state.config_file), config_error_line(&state.config_file));
        free_resources();
        config_destroy(&state.config_file);
        return VESPERCSI_FAIL;
    }

    int temp_int = 0;
    const char* temp_str = NULL;
    long long temp_int64;

    // out.* config

    if(config_lookup_string(&state.config_file, "out.directory", &temp_str)){
        strlcpy(state.out.directory, temp_str, PATH_MAX);    
    }

    if(strlen(state.out.directory) > 0){
        // Directory not present, I guess we should check the default value now
        if(access(state.out.directory, W_OK | X_OK) == 0){
            LOG_VERBOSE(VERBOSITY_1, DEBG, "ir_label: Permissions are sufficient: %s",state.out.directory);
        }else{
            LOG(FAIL, "ir_label: Cannot access %s, permission denied", state.out.directory);
            config_destroy(&state.config_file);
            free_resources();
            return VESPERCSI_FAIL;
        }

        struct stat directory_path_stat = {0};
        stat(state.out.directory, &directory_path_stat);

        if(S_ISDIR(directory_path_stat.st_mode)) {
            LOG_VERBOSE(VERBOSITY_1, DEBG, "ir_label: Directory exists: %s",state.out.directory);
        }else{
            LOG(FAIL, "ir_label: %s is not a directory", state.out.directory);
            config_destroy(&state.config_file);
            free_resources();            
            return VESPERCSI_FAIL;
        }        

        uint64_t len = strlen(state.out.directory);

        if(state.out.directory[len-1] != '/'){

            if(len < PATH_MAX - 1){
                state.out.directory[len] = '/';
                state.out.directory[len+1] = '\0';
            }else{
                LOG(FAIL, "ir_label: Directory name too large for buffer to include a file name at the end");
                config_destroy(&state.config_file);
                free_resources();
                return VESPERCSI_FAIL;
            }
        }

    }

    if(config_lookup_bool(&state.config_file, "out.timestamp", &temp_int)){
        state.out.include.timestamp = temp_int;
    }

    if(config_lookup_bool(&state.config_file, "out.datetime", &temp_int)){
        state.out.include.datetime = temp_int;
    }     
    
    if(config_lookup_bool(&state.config_file, "out.action", &temp_int)){
        state.out.include.action = temp_int;
    }    

    if(config_lookup_string(&state.config_file, "out.prefix", &temp_str)){
        strlcpy(state.out.prefix, temp_str, sizeof(state.out.prefix));
        state.out.include.prefix = true;
    }

    if(config_lookup_string(&state.config_file, "out.suffix", &temp_str)){
        strlcpy(state.out.suffix, temp_str, sizeof(state.out.suffix));
        state.out.include.suffix = true;
    }

    if(config_lookup_string(&state.config_file, "out.extension", &temp_str)){
        strlcpy(state.out.extension, temp_str, sizeof(state.out.extension));
        state.out.include.extension = true;
    }

    if(config_lookup_bool(&state.config_file, "out.index", &temp_int)){
        state.out.include.index = temp_int;
    } 

    // capture.* config

    if(config_lookup_bool(&state.config_file, "capture.index_autoincrement", &temp_int)){
        state.capture.index_autoincrement = temp_int;
    }

    if(config_lookup_int64(&state.config_file, "capture.index", &temp_int64)){
        state.capture.index = temp_int64;
    }    

    // action.* config //

    // Walk
    if(config_lookup_bool(&state.config_file, "action.walk.periodic", &temp_int)){
        state.action.policies[ACTION_WALK].is_periodic = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.walk.duration", &temp_int64)){
        state.action.policies[ACTION_WALK].action_duration = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.walk.wait", &temp_int64)){
        state.action.policies[ACTION_WALK].action_wait = temp_int64;
    }
    if(config_lookup_bool(&state.config_file, "action.walk.hold", &temp_int)){
        state.action.policies[ACTION_WALK].action_hold = temp_int;
    }
    if(config_lookup_bool(&state.config_file, "action.walk.hold_nolabel", &temp_int)){
        state.action.policies[ACTION_WALK].action_hold_nolabel = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.walk.hold_seconds", &temp_int64)){
        state.action.policies[ACTION_WALK].action_hold_seconds = temp_int64;
    }          
    if(config_lookup_int64(&state.config_file, "action.walk.count", &temp_int64)){
        state.action.policies[ACTION_WALK].action_count = temp_int64;
    }


    // Run
    if(config_lookup_bool(&state.config_file, "action.run.periodic", &temp_int)){
        state.action.policies[ACTION_RUN].is_periodic = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.run.duration", &temp_int64)){
        state.action.policies[ACTION_RUN].action_duration = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.run.wait", &temp_int64)){
        state.action.policies[ACTION_RUN].action_wait = temp_int64;
    }        
    if(config_lookup_bool(&state.config_file, "action.run.hold", &temp_int)){
        state.action.policies[ACTION_RUN].action_hold = temp_int;
    }
    if(config_lookup_bool(&state.config_file, "action.run.hold_nolabel", &temp_int)){
        state.action.policies[ACTION_RUN].action_hold_nolabel = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.run.hold_seconds", &temp_int64)){
        state.action.policies[ACTION_RUN].action_hold_seconds = temp_int64;
    }        
    if(config_lookup_int64(&state.config_file, "action.run.count", &temp_int64)){
        state.action.policies[ACTION_RUN].action_count = temp_int64;
    }


    // Jump
    if(config_lookup_bool(&state.config_file, "action.jump.periodic", &temp_int)){
        state.action.policies[ACTION_JUMP].is_periodic = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.jump.duration", &temp_int64)){
        state.action.policies[ACTION_JUMP].action_duration = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.jump.wait", &temp_int64)){
        state.action.policies[ACTION_JUMP].action_wait = temp_int64;
    }            
    if(config_lookup_bool(&state.config_file, "action.jump.hold", &temp_int)){
        state.action.policies[ACTION_JUMP].action_hold = temp_int;
    }
    if(config_lookup_bool(&state.config_file, "action.jump.hold_nolabel", &temp_int)){
        state.action.policies[ACTION_JUMP].action_hold_nolabel = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.jump.hold_seconds", &temp_int64)){
        state.action.policies[ACTION_JUMP].action_hold_seconds = temp_int64;
    }    
    if(config_lookup_int64(&state.config_file, "action.jump.count", &temp_int64)){
        state.action.policies[ACTION_JUMP].action_count = temp_int64;
    }

    // Still
    if(config_lookup_bool(&state.config_file, "action.still.periodic", &temp_int)){
        state.action.policies[ACTION_STILL].is_periodic = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.still.duration", &temp_int64)){
        state.action.policies[ACTION_STILL].action_duration = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.still.wait", &temp_int64)){
        state.action.policies[ACTION_STILL].action_wait = temp_int64;
    }     
    if(config_lookup_bool(&state.config_file, "action.still.hold", &temp_int)){
        state.action.policies[ACTION_STILL].action_hold = temp_int;
    }
    if(config_lookup_bool(&state.config_file, "action.still.hold_nolabel", &temp_int)){
        state.action.policies[ACTION_STILL].action_hold_nolabel = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.still.hold_seconds", &temp_int64)){
        state.action.policies[ACTION_STILL].action_hold_seconds = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.still.count", &temp_int64)){
        state.action.policies[ACTION_STILL].action_count = temp_int64;
    }

    // Stand up
    if(config_lookup_bool(&state.config_file, "action.standup.periodic", &temp_int)){
        state.action.policies[ACTION_STANDUP].is_periodic = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.standup.duration", &temp_int64)){
        state.action.policies[ACTION_STANDUP].action_duration = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.standup.wait", &temp_int64)){
        state.action.policies[ACTION_STANDUP].action_wait = temp_int64;
    }     
    if(config_lookup_bool(&state.config_file, "action.standup.hold", &temp_int)){
        state.action.policies[ACTION_STANDUP].action_hold = temp_int;
    }
    if(config_lookup_bool(&state.config_file, "action.standup.hold_nolabel", &temp_int)){
        state.action.policies[ACTION_STANDUP].action_hold_nolabel = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.standup.hold_seconds", &temp_int64)){
        state.action.policies[ACTION_STANDUP].action_hold_seconds = temp_int64;
    }    
    if(config_lookup_int64(&state.config_file, "action.standup.count", &temp_int64)){
        state.action.policies[ACTION_STANDUP].action_count = temp_int64;
    }


    // Sit down
    if(config_lookup_bool(&state.config_file, "action.sitdown.periodic", &temp_int)){
        state.action.policies[ACTION_SITDOWN].is_periodic = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.sitdown.duration", &temp_int64)){
        state.action.policies[ACTION_SITDOWN].action_duration = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.sitdown.wait", &temp_int64)){
        state.action.policies[ACTION_SITDOWN].action_wait = temp_int64;
    }    
    if(config_lookup_bool(&state.config_file, "action.sitdown.hold", &temp_int)){
        state.action.policies[ACTION_SITDOWN].action_hold = temp_int;
    }
    if(config_lookup_bool(&state.config_file, "action.sitdown.hold_nolabel", &temp_int)){
        state.action.policies[ACTION_SITDOWN].action_hold_nolabel = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.sitdown.hold_seconds", &temp_int64)){
        state.action.policies[ACTION_SITDOWN].action_hold_seconds = temp_int64;
    }    
    if(config_lookup_int64(&state.config_file, "action.sitdown.count", &temp_int64)){
        state.action.policies[ACTION_SITDOWN].action_count = temp_int64;
    }

    // Sit down and stand up
    if(config_lookup_bool(&state.config_file, "action.sitstand.periodic", &temp_int)){
        state.action.policies[ACTION_SITSTAND].is_periodic = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.sitstand.duration", &temp_int64)){
        state.action.policies[ACTION_SITSTAND].action_duration = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.sitstand.wait", &temp_int64)){
        state.action.policies[ACTION_SITSTAND].action_wait = temp_int64;
    }    
    if(config_lookup_bool(&state.config_file, "action.sitstand.hold", &temp_int)){
        state.action.policies[ACTION_SITSTAND].action_hold = temp_int;
    }
    if(config_lookup_bool(&state.config_file, "action.sitstand.hold_nolabel", &temp_int)){
        state.action.policies[ACTION_SITSTAND].action_hold_nolabel = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.sitstand.hold_seconds", &temp_int64)){
        state.action.policies[ACTION_SITSTAND].action_hold_seconds = temp_int64;
    }    
    if(config_lookup_int64(&state.config_file, "action.sitstand.count", &temp_int64)){
        state.action.policies[ACTION_SITSTAND].action_count = temp_int64;
    }

    // Fall
    if(config_lookup_bool(&state.config_file, "action.fall.periodic", &temp_int)){
        state.action.policies[ACTION_FALL].is_periodic = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.fall.duration", &temp_int64)){
        state.action.policies[ACTION_FALL].action_duration = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.fall.wait", &temp_int64)){
        state.action.policies[ACTION_FALL].action_wait = temp_int64;
    }
    if(config_lookup_bool(&state.config_file, "action.fall.hold", &temp_int)){
        state.action.policies[ACTION_FALL].action_hold = temp_int;
    }
    if(config_lookup_bool(&state.config_file, "action.fall.hold_nolabel", &temp_int)){
        state.action.policies[ACTION_FALL].action_hold_nolabel = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.fall.hold_seconds", &temp_int64)){
        state.action.policies[ACTION_FALL].action_hold_seconds = temp_int64;
    }    
    if(config_lookup_int64(&state.config_file, "action.fall.count", &temp_int64)){
        state.action.policies[ACTION_FALL].action_count = temp_int64;
    }

    // Empty
    if(config_lookup_bool(&state.config_file, "action.empty.periodic", &temp_int)){
        state.action.policies[ACTION_EMPTY].is_periodic = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.empty.duration", &temp_int64)){
        state.action.policies[ACTION_EMPTY].action_duration = temp_int64;
    }
    if(config_lookup_int64(&state.config_file, "action.empty.wait", &temp_int64)){
        state.action.policies[ACTION_EMPTY].action_wait = temp_int64;
    }
    if(config_lookup_bool(&state.config_file, "action.empty.hold", &temp_int)){
        state.action.policies[ACTION_EMPTY].action_hold = temp_int;
    }
    if(config_lookup_bool(&state.config_file, "action.empty.hold_nolabel", &temp_int)){
        state.action.policies[ACTION_EMPTY].action_hold_nolabel = temp_int;
    }
    if(config_lookup_int64(&state.config_file, "action.empty.hold_seconds", &temp_int64)){
        state.action.policies[ACTION_EMPTY].action_hold_seconds = temp_int64;
    }    
    if(config_lookup_int64(&state.config_file, "action.empty.count", &temp_int64)){
        state.action.policies[ACTION_EMPTY].action_count = temp_int64;
    }



    config_destroy(&state.config_file);

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
        free_resources();
        return VESPERCSI_FAIL;
    }

    if (ioctl(state.lirc.fd, LIRC_GET_REC_MODE, &mode) < 0) {
        LOG(FAIL, "ir_label: ioctl LIRC_GET_REC_MODE failed");
        free_resources();
        return VESPERCSI_FAIL;
    }

    if(mode != LIRC_MODE_SCANCODE){
        LOG(FAIL, "ir_label: mode is not set to LIRC_MODE_SCANCODE");
        free_resources();
        return VESPERCSI_FAIL;
    }

    //==========================================================//
    //======================= GPIO Setup =======================//

    state.gpio.chip = gpiod_chip_open(GPIO_DEV);
    if (!state.gpio.chip) {
        LOG(FAIL, "ir_label: GPIO device %s cannot be opened", GPIO_DEV);
        free_resources();
        return VESPERCSI_FAIL;
    }

    // 1. Create a generic settings profile for an Output line initialized to 0 (Inactive)
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if(!settings){
        LOG(FAIL, "ir_label: Cannot create GPIO line settings");
        free_resources();
        return VESPERCSI_FAIL;
    }
    
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    // 2. Map the settings to the specific offsets you intend to use
    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    if(!line_cfg){
        LOG(FAIL, "ir_label: Cannot create GPIO line config");
        gpiod_line_settings_free(settings);
        free_resources();
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
        free_resources();
        return VESPERCSI_FAIL;
    }

    // 3. Set standard request metadata (like the consumer string)
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    if(!req_cfg){
        LOG(FAIL, "ir_label: Cannot create GPIO request config");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        free_resources();
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
        free_resources();
        return VESPERCSI_FAIL;
    }

    if(pthread_create(&state.infrared_runner_thread, NULL, infrared_runner, NULL) != 0){
        LOG(FAIL, "ir_label: Cannot create infrared_runner thread");
        free_resources();
        return VESPERCSI_FAIL;
    }else{
        LOG_VERBOSE(VERBOSITY_0, INFO, "ir_label: Created infrared_runner thread");
    }


    if(pthread_create(&state.async_buzzer_runner_thread, NULL, async_buzzer_runner, NULL) != 0){
        LOG(FAIL, "ir_label: Cannot create async_buzzer_runner thread");
        free_resources();
        return VESPERCSI_FAIL;
    }else{
        LOG_VERBOSE(VERBOSITY_0, INFO, "ir_label: Created async_buzzer_runner thread");
    }

    LOG(INFO, "ir_label: Plugin initalized");
    return VESPERCSI_OK;
}

int vespercsi_plugin_function_callback(const vespercsi_csi_t* csi){

    if(state.capture.enable){
        // Critical Area //

        int lock_attempt = pthread_mutex_trylock(&state.out.fd_lock);
        
        if(lock_attempt == EBUSY){
            return VESPERCSI_OK;
        }else if(lock_attempt == EINVAL){
            return VESPERCSI_FAIL;
        }

        ir_label_header_t ir_label_header = {0};

        if(state.capture.is_labeling){
            ir_label_header.action_label = action_to_label(state.action.effective);
            ir_label_header.action_group_index =  state.action.index[state.action.effective];
        }else{
            ir_label_header.action_label = ACTION_NOLABEL;
            ir_label_header.action_group_index =  state.action.index[ACTION_NOLABEL];
        }


        if(state.out.fd >= 0){

            struct iovec iov[3] = {0};

            iov[0].iov_base = (void*)&ir_label_header;
            iov[0].iov_len = sizeof(ir_label_header);
            iov[1].iov_base = (void*)&csi->header;
            iov[1].iov_len  = VESPERCSI_CSI_HEADER_LENGTH;
            iov[2].iov_base = (void*)&csi->data.csi_data;
            iov[2].iov_len  = csi->header.csi_data_size;

            size_t total_to_write = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;
            size_t remaining = total_to_write;

            // Variables to track which vectors to send next
            int current_iov_index = 0;
            int current_iov_count = 3;

            while (remaining > 0 && current_iov_index < 3) {
                // Write starting from the first unwritten (or partially written) vector
                ssize_t written = writev(state.out.fd, &iov[current_iov_index], current_iov_count);

                if (written < 0) {
                    if (errno == EINTR) {
                        // System call was interrupted by a signal before any data was written; try again
                        continue;
                    }
                    LOG_VERBOSE(VERBOSITY_2, WARN, "ir_label: Fatal write error: %s", strerror(errno));
                    break; 
                }

                if (written == 0) {
                    // EOF or disk full scenario, stop attempting to write
                    LOG_VERBOSE(VERBOSITY_2, WARN, "ir_label: writev returned 0 (No space left?)");
                    break;
                }

                remaining -= written;

                // Advance the iovec structures based on how many bytes were actually written
                size_t bytes_accounted_for = (size_t)written;
                
                    while (bytes_accounted_for > 0 && current_iov_index < 3) {
                        if (iov[current_iov_index].iov_len <= bytes_accounted_for) {
                            // This entire vector was successfully written
                            bytes_accounted_for -= iov[current_iov_index].iov_len;
                            current_iov_index++;  // Move to the next vector
                            current_iov_count--;  // We have one less vector to process
                        }else{
                            // This vector was only partially written. 
                            // Adjust its base pointer and length for the next writev call.
                            iov[current_iov_index].iov_base = (uint8_t*)iov[current_iov_index].iov_base + bytes_accounted_for;
                            iov[current_iov_index].iov_len -= bytes_accounted_for;
                            bytes_accounted_for = 0; // All written bytes are accounted for
                        }
                    }
            }

            if (remaining > 0) {
                LOG_VERBOSE(VERBOSITY_2, WARN, "ir_label: CSI partial write failure. %zu bytes lost out of %zu", remaining, total_to_write);
            } else {
                LOG_VERBOSE(VERBOSITY_2, INFO,"ir_label: CSI with size %zu written to file: %s", total_to_write, state.out.fd_path);
            }

        //         
        }

        pthread_mutex_unlock(&state.out.fd_lock);
        // Capture Enabled
    }

    return VESPERCSI_OK;
}

int vespercsi_plugin_function_destroy(void){

    LOG_VERBOSE(VERBOSITY_1, INFO, "ir_label: Stopping infrared_runner thread");

    if(state.async_buzzer_runner_thread != -1){
        state.async_buzzer.stop_signal = true;
        pthread_join(state.async_buzzer_runner_thread, NULL);
    }

    if(state.capture_periodic_runner_thread != -1){
        state.capture.stop_periodic_signal = true;
        pthread_join(state.capture_periodic_runner_thread, NULL);
    }


    if(state.infrared_runner_thread != -1){
        state.shutdown_signal = true;
        pthread_join(state.infrared_runner_thread, NULL);
    }
    
    close(async_buzzer_pipe[0]);
    close(async_buzzer_pipe[1]);

    pthread_mutex_destroy(&state.out.fd_lock);
    free_resources();
    LOG(INFO, "ir_label: Plugin destroyed");
    return VESPERCSI_OK;
}
