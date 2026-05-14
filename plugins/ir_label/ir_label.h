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
#include <time.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/stat.h>

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

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_DISCARD_CAPTURE
    #define COMMAND_DISCARD_CAPTURE CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_DISCARD_CAPTURE
#else
    #define COMMAND_DISCARD_CAPTURE 0x02
#endif


// Action Values

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_WALK
    #define COMMAND_SET_ACTION_WALK CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_WALK
#else
    #define COMMAND_SET_ACTION_WALK 0x03
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_RUN
    #define COMMAND_SET_ACTION_RUN CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_RUN
#else
    #define COMMAND_SET_ACTION_RUN 0x04
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_JUMP
    #define COMMAND_SET_ACTION_JUMP CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_JUMP
#else
    #define COMMAND_SET_ACTION_JUMP 0x05
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_STILL
    #define COMMAND_SET_ACTION_STILL CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_STILL
#else
    #define COMMAND_SET_ACTION_STILL 0x06
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_STANDUP
    #define COMMAND_SET_ACTION_STANDUP CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_STANDUP
#else
    #define COMMAND_SET_ACTION_STANDUP 0x07
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_SITDOWN
    #define COMMAND_SET_ACTION_SITDOWN CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_SITDOWN
#else
    #define COMMAND_SET_ACTION_SITDOWN 0x08
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_SITSTAND
    #define COMMAND_SET_ACTION_SITSTAND CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_SITSTAND
#else
    #define COMMAND_SET_ACTION_SITSTAND 0x09
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_FALL
    #define COMMAND_SET_ACTION_FALL CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_SET_ACTION_FALL
#else
    #define COMMAND_SET_ACTION_FALL 0x10
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_CAPTURE_INDEX_INCREASE
    #define COMMAND_CAPTURE_INDEX_INCREASE CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_CAPTURE_INDEX_INCREASE
#else
    #define COMMAND_CAPTURE_INDEX_INCREASE 0x11
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_CAPTURE_INDEX_DECREASE
    #define COMMAND_CAPTURE_INDEX_DECREASE CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_CAPTURE_INDEX_DECREASE
#else
    #define COMMAND_CAPTURE_INDEX_DECREASE 0x12
#endif

#ifdef CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_CAPTURE_INDEX_TOGGLE_AUTOINCREMENT
    #define COMMAND_CAPTURE_INDEX_TOGGLE_AUTOINCREMENT CONFIG_VESPERCSI_PLUGIN_IR_LABEL_COMMAND_CAPTURE_INDEX_TOGGLE_AUTOINCREMENT
#else
    #define COMMAND_CAPTURE_INDEX_TOGGLE_AUTOINCREMENT 0x13
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

#define SLEEP_MILISECONDS(ms) usleep(1000*(ms))

/*
    Buzzer Sequences

    Any/Invalid Input: B-100 G-50

    Capture Hold: B-500 G-500 B-500 G-500 B-500 G-500

    Capture Start: (Also periodic indicator): B-100 G-50 B-100 G-50 
    Capture Wait: B-100 G-50

    Capture Stop: B-400 G-100 B-150 G-100

    Action Change: B-50 G-50 B-50 G-50


    Each period must include it's own index during the capture start-stop session
    A single file record contains either a continuous or a periodic action, after start
    let it be WALK or JUMP, it is periodic or continuous, but this does not affect writing operation in general.
    A single file cannot contain multiple action types, only one, for example WALK.

    WALK operation is chosen, if continuous, then the index does not change during the action, but if it is periodic,
    index is incremented by one after each period is complete. This is for internal usage.

*/

#define    ASYNC_BUZZER_CODE_CAPTURE_START 1
#define    ASYNC_BUZZER_CODE_CAPTURE_STOP 2
#define    ASYNC_BUZZER_CODE_CAPTURE_WAIT 3
#define    ASYNC_BUZZER_CODE_ACTION_CHANGE 4

typedef uint16_t label_t;

typedef uint8_t command_t;

typedef enum action_t{
    ACTION_NOLABEL = 0,
    ACTION_WALK,
    ACTION_RUN,
    ACTION_JUMP,
    ACTION_STILL,
    ACTION_SITDOWN,
    ACTION_STANDUP,
    ACTION_SITSTAND,
    ACTION_FALL,
}action_t;

extern int async_buzzer_pipe[2];

typedef struct action_policy_t{
    bool is_periodic; // If not periodic, then it is continuous
    // If periodic:
    uint64_t action_duration; // Action duration, valid time interval for recording
    uint64_t action_wait; // Time to wait between each recordings
    uint64_t action_count; // If zero, it means no bound
    bool action_hold; // Amount of time to wait before beginning 
    bool action_hold_nolabel;
    uint64_t action_hold_seconds;
}action_policy_t;

typedef struct ir_label_header_t{
    uint16_t action_group_index;
    uint16_t action_label;
}ir_label_header_t;

typedef struct ir_label_state_t{

    config_t config_file;

    struct{

        int fd;
        char fd_path[PATH_MAX];
        pthread_mutex_t fd_lock;

        char directory[PATH_MAX];
        char prefix[NAME_MAX];
        char suffix[NAME_MAX];
        char extension[NAME_MAX];

        struct{
            bool timestamp;
            bool datetime;
            bool action;
            bool prefix;
            bool suffix;
            bool extension;
            bool index;
        }include;


    }out;

    struct{
        atomic_bool is_recording;
        atomic_bool is_labeling;
        atomic_bool enable;
        sig_atomic_t stop_periodic_signal;
        uint64_t index;
        bool index_autoincrement;
    }capture;

    sig_atomic_t shutdown_signal;

    struct{
        sig_atomic_t stop_signal;
    }async_buzzer;

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
        action_policy_t policies[9];
        uint64_t index[9];
    }action;

    pthread_t infrared_runner_thread;
    pthread_t capture_periodic_runner_thread;
    pthread_t async_buzzer_runner_thread;

}ir_label_state_t;

void stop_capture_periodic(bool self);
