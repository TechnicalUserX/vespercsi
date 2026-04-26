#include <vespercsi/plugin.h>
#include "ir_label.h"

vespercsi_plugin_context_t vespercsi_plugin_context;
ir_label_state_t state;



command_t command_decoder(uint64_t code){
    switch(code){
        case ACTION_WALK_VALUE:     return COMMAND_SET_ACTION_WALK;
        case ACTION_RUN_VALUE:      return COMMAND_SET_ACTION_RUN;
        case ACTION_JUMP_VALUE:     return COMMAND_SET_ACTION_JUMP;
        case ACTION_STILL_VALUE:    return COMMAND_SET_ACTION_STILL;
        case ACTION_SITDOWN_VALUE:  return COMMAND_SET_ACTION_SITDOWN;
        case ACTION_STANDUP_VALUE:  return COMMAND_SET_ACTION_STANDUP;
        case ACTION_FALL_VALUE:     return COMMAND_SET_ACTION_FALL;
        
    }
}


void command_interpreter(command_t cmd){


}

// This thread changes the internal state of the plugin based on infrared remote input
void* infrared_runner(void* arg){

    struct lirc_scancode sc;
    struct pollfd pfd = { .fd = state.lirc.fd, .events = POLLIN };

    while(1){

        int p = poll(&pfd, 1, 200);

        if(p == 0){
            if(state.shutdown_signal){
                pthread_exit(NULL);
            }
        }else if(pfd.revents & POLLIN){
            // There is data in the infrared receiver
            int r = read(state.lirc.fd, &sc, sizeof(sc));

            if(r != sizeof(sc)){
                // Bad read
                continue;
            }

            // Good read
            uint8_t command_byte = sc.scancode & 0xFF;
            command_decoder(command_byte);

        }

    }

}

void letgo_resources(void){

    if(state.gpio.line_led_red){
        gpiod_line_release(state.gpio.line_led_red);
    }

    if(state.gpio.line_led_rgb[0]){
        gpiod_line_release(state.gpio.line_led_rgb[0]);
    }

    if(state.gpio.line_led_rgb[1]){
        gpiod_line_release(state.gpio.line_led_rgb[1]);
    }

    if(state.gpio.line_led_rgb[2]){
        gpiod_line_release(state.gpio.line_led_rgb[2]);
    }

    if(state.gpio.chip){
        gpiod_chip_close(state.gpio.chip);
    }

    if(state.lirc.fd > 0){
        close(state.lirc.fd);
    }
}

int vespercsi_plugin_function_init(vespercsi_plugin_context_t* context){
    vespercsi_plugin_context = *context;


    //==========================================================//
    //==================== Infrared Setup ======================//

    state.lirc.fd = open(LIRC_DEV, O_RDONLY);

    if(state.lirc.fd < 0){
        LOG(WARN, "ir_label: Cannot open %s", LIRC_DEV);
        return VESPERCSI_FAIL;
    }

    unsigned int mode = LIRC_MODE_SCANCODE;
    if (ioctl(state.lirc.fd, LIRC_SET_REC_MODE, &mode) < 0) {
        LOG(WARN, "ir_label: ioctl LIRC_SET_REC_MODE failed");
        letgo_resources();
        return VESPERCSI_FAIL;
    }    

    if (ioctl(state.lirc.fd, LIRC_GET_REC_MODE, &mode) < 0) {
        LOG(WARN, "ir_label: ioctl LIRC_GET_REC_MODE failed");
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    if(mode != LIRC_MODE_SCANCODE){
        LOG(WARN, "ir_label: mode is not set to LIRC_MODE_SCANCODE");
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    //==========================================================//
    //======================= GPIO Setup =======================//

    state.gpio.chip = gpiod_chip_open(GPIO_DEV);
    if (!state.gpio.chip) {
        LOG(WARN, "ir_label: GPIO device %s cannot be opened", GPIO_DEV);
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    state.gpio.line_led_red = gpiod_chip_get_line( state.gpio.chip, GPIO_LINE_LED_RED);
    if(!state.gpio.line_led_red){
        LOG(WARN, "ir_label: GPIO line for red led cannot be retrieved");
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    state.gpio.line_led_rgb[0] = gpiod_chip_get_line( state.gpio.chip, GPIO_LINE_LED_RGB_RED);
    if(!state.gpio.line_led_rgb[0]){
        LOG(WARN, "ir_label: GPIO line for rgb led (red) cannot be retrieved");
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    state.gpio.line_led_rgb[1] = gpiod_chip_get_line( state.gpio.chip, GPIO_LINE_LED_RGB_GREEN);
    if(!state.gpio.line_led_rgb[1]){
        LOG(WARN, "ir_label: GPIO line for rgb led (green) cannot be retrieved");
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    state.gpio.line_led_rgb[2] = gpiod_chip_get_line( state.gpio.chip, GPIO_LINE_LED_RGB_BLUE);
    if(!state.gpio.line_led_rgb[2]){
        LOG(WARN, "ir_label: GPIO line for rgb led (blue) cannot be retrieved");
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    state.gpio.line_buzzer = gpiod_chip_get_line( state.gpio.chip, GPIO_LINE_BUZZER);
    if(!state.gpio.line_buzzer){
        LOG(WARN, "ir_label: GPIO line for buzzer cannot be retrieved");
        letgo_resources();
        return VESPERCSI_FAIL;
    }

    if (gpiod_line_request_output(state.gpio.line_led_red, "vespercsi-plugin-ir-label", 0) < 0) {
        LOG(WARN, "ir_label: GPIO line for red led cannot be set to output mode");
        letgo_resources();
        return VESPERCSI_FAIL;
    }


    
    LOG(INFO, "ir_label: Plugin initalized");
    return VESPERCSI_OK;
}

int vespercsi_plugin_function_callback(const vespercsi_csi_t* csi){
    struct lirc_scancode sc;
    return VESPERCSI_OK;
}

int vespercsi_plugin_function_destroy(void){

    state.shutdown_signal = true;
    pthread_join(state.infrared_runner_thread, NULL);


    LOG(INFO, "ir_label: Plugin destroyed");
    return VESPERCSI_OK;
}
