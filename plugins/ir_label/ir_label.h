#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <linux/lirc.h>
#include <sys/ioctl.h>
#include <gpiod.h>
#include <pthread.h>
#include <stdatomic.h>
#include <libconfig.h>
#include <sys/uio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#ifdef  CONFIG_VESPERCSI_PLUGIN_IR_LABEL_LIRC_DEV
    #define LIRC_DEV CONFIG_VESPERCSI_PLUGIN_IR_LABEL_LIRC_DEV
#else
    #define LIRC_DEV "/dev/lirc0"
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_GPIO_DEV
    #define GPIO_DEV CONFIG_VESPERCSI_PLUGIN_IR_LABEL_GPIO_DEV
#else
    #define GPIO_DEV "/dev/gpiochip0"
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_GPIO_LINE_LED_RED
    #define GPIO_LINE_LED_RED CONFIG_VESPERCSI_PLUGIN_IR_LABEL_GPIO_LINE_LED_RED
#else
    #define GPIO_LINE_LED_RED 27
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_GPIO_LINE_BUZZER
    #define GPIO_LINE_BUZZER CONFIG_VESPERCSI_PLUGIN_IR_LABEL_GPIO_LINE_BUZZER
#else
    #define GPIO_LINE_BUZZER 22
#endif

// Capture Control

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_START_CAPTURE
    #define COMMAND_START_CAPTURE CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_START_CAPTURE
#else
    #define COMMAND_START_CAPTURE 0x00
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_STOP_CAPTURE
    #define COMMAND_STOP_CAPTURE CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_STOP_CAPTURE
#else
    #define COMMAND_STOP_CAPTURE 0x01
#endif


// Action Values

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_WALK
    #define COMMAND_SET_ACTION_WALK CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_WALK
#else
    #define COMMAND_SET_ACTION_WALK 0x02
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_RUN
    #define COMMAND_SET_ACTION_RUN CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_RUN
#else
    #define COMMAND_SET_ACTION_RUN 0x03
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_JUMP
    #define COMMAND_SET_ACTION_JUMP CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_JUMP
#else
    #define COMMAND_SET_ACTION_JUMP 0x04
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_STILL
    #define COMMAND_SET_ACTION_STILL CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_STILL
#else
    #define COMMAND_SET_ACTION_STILL 0x05
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_SITDOWN
    #define COMMAND_SET_ACTION_SITDOWN CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_SITDOWN
#else
    #define COMMAND_SET_ACTION_SITDOWN 0x06
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_STANDUP
    #define COMMAND_SET_ACTION_STANDUP CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_STANDUP
#else
    #define COMMAND_SET_ACTION_STANDUP 0x07
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_SITSTAND
    #define COMMAND_SET_ACTION_SITSTAND CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_SITSTAND
#else
    #define COMMAND_SET_ACTION_SITSTAND 0x08
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_FALL
    #define COMMAND_SET_ACTION_FALL CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_FALL
#else
    #define COMMAND_SET_ACTION_FALL 0x09
#endif

// Default Action Policy

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_ACTION_POLICY_PERIODIC_DEFAULT_DURATION
    #define ACTION_POLICY_PERIODIC_DEFAULT_DURATION CONFIG_VESPERCSI_PLUGIN_IR_LABEL_ACTION_POLICY_PERIODIC_DEFAULT_DURATION
#else
    #define ACTION_POLICY_PERIODIC_DEFAULT_DURATION 10000000
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_ACTION_POLICY_PERIODIC_DEFAULT_WAIT
    #define ACTION_POLICY_PERIODIC_DEFAULT_WAIT CONFIG_VESPERCSI_PLUGIN_IR_LABEL_ACTION_POLICY_PERIODIC_DEFAULT_WAIT
#else
    #define ACTION_POLICY_PERIODIC_DEFAULT_WAIT 5000000
#endif



typedef uint16_t label_t;

typedef uint8_t command_t;

typedef enum action_t{
    ACTION_WALK = 0,
    ACTION_RUN,
    ACTION_JUMP,
    ACTION_STILL,
    ACTION_SITDOWN,
    ACTION_STANDUP,
    ACTION_SITSTAND,
    ACTION_FALL,
    ACTION_UNKNOWN
}action_t;

typedef struct action_policy_t{
    bool is_periodic; // If not periodic, then it is continuous
    // If periodic:
    uint64_t action_duration; // Action duration, valid time interval for recording
    uint64_t action_wait; // Time to wait between each recordings
}action_policy_t;

extern action_policy_t action_policies[8];

typedef struct ir_label_header_t{
    uint64_t index;
    uint16_t label;
}ir_label_header_t;

typedef struct ir_label_state_t{

    int out_fd;
    char out_path[512];

    struct{
        _Atomic(uint64_t) index;
        atomic_bool is_recording;
        atomic_bool is_periodic;
        atomic_bool enable;
        sig_atomic_t stop_periodic_signal;
    }capture;

    sig_atomic_t shutdown_signal;

    struct{
        int fd;
    }lirc;

    struct{
        struct gpiod_chip* chip;
        struct gpiod_line_request* request; // Replaces all individual line pointers
    }gpio;

    struct{
        uint8_t previous;
        uint8_t current;
    }command;

    struct{
        _Atomic(action_t) current;
        _Atomic(action_t) effective;
    }action;

    pthread_t infrared_runner_thread;
    pthread_t periodic_runner_thread;

}ir_label_state_t;

