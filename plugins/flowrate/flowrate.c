#include <vespercsi/plugin/sdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <termios.h>
#include <sys/ioctl.h>

vespercsi_plugin_context_t vespercsi_plugin_context;

// Sliding window configuration
#define NUM_BUCKETS 5 // 5 buckets of 200ms = 1 second
static atomic_int current_bucket_count = ATOMIC_VAR_INIT(0);
static int history[NUM_BUCKETS] = {0};
static int bucket_index = 0;

// Threading and State
static pthread_t render_thread;
static atomic_bool keep_running = ATOMIC_VAR_INIT(true);
static int last_rate = -1;

// Terminal State
static struct termios orig_termios;
static int term_cols = 80;
static int term_rows = 24;

// 8x7 ASCII Font mimicking a 7-Segment Display
#define DIGIT_WIDTH 8
#define DIGIT_HEIGHT 7
#define DIGIT_SPACING 2 

static const char* digits[10][DIGIT_HEIGHT] = {
    { // 0
        " ###### ",
        "##    ##",
        "##    ##",
        "        ",
        "##    ##",
        "##    ##",
        " ###### "
    },
    { // 1
        "      ##",
        "      ##",
        "      ##",
        "        ",
        "      ##",
        "      ##",
        "      ##"
    },
    { // 2
        " ###### ",
        "      ##",
        "      ##",
        " ###### ",
        "##      ",
        "##      ",
        " ###### "
    },
    { // 3
        " ###### ",
        "      ##",
        "      ##",
        " ###### ",
        "      ##",
        "      ##",
        " ###### "
    },
    { // 4
        "##    ##",
        "##    ##",
        "##    ##",
        " ###### ",
        "      ##",
        "      ##",
        "      ##"
    },
    { // 5
        " ###### ",
        "##      ",
        "##      ",
        " ###### ",
        "      ##",
        "      ##",
        " ###### "
    },
    { // 6
        " ###### ",
        "##      ",
        "##      ",
        " ###### ",
        "##    ##",
        "##    ##",
        " ###### "
    },
    { // 7
        " ###### ",
        "      ##",
        "      ##",
        "        ",
        "      ##",
        "      ##",
        "      ##"
    },
    { // 8
        " ###### ",
        "##    ##",
        "##    ##",
        " ###### ",
        "##    ##",
        "##    ##",
        " ###### "
    },
    { // 9
        " ###### ",
        "##    ##",
        "##    ##",
        " ###### ",
        "      ##",
        "      ##",
        " ###### "
    }
};

// Delta-rendering logic to prevent terminal flicker
void draw_rate(int rate) {
    char current_str[16];
    char last_str[16];
    snprintf(current_str, sizeof(current_str), "%d", rate);
    snprintf(last_str, sizeof(last_str), "%d", last_rate);
    
    int cur_len = strlen(current_str);
    int last_len = (last_rate == -1) ? 0 : strlen(last_str);
    
    int char_stride = DIGIT_WIDTH + DIGIT_SPACING; // 10 chars per digit footprint
    
    // If the number of digits changes (e.g., 1000 -> 999), clear the old footprint
    if (cur_len != last_len && last_rate != -1) {
        int old_width = last_len * char_stride - DIGIT_SPACING;
        int old_x = (term_cols - old_width) / 2;
        int start_y = (term_rows - DIGIT_HEIGHT) / 2;
        
        for (int r = 0; r < DIGIT_HEIGHT; r++) {
            printf("\033[%d;%dH", start_y + r, old_x);
            for (int c = 0; c < old_width; c++) putchar(' ');
        }
    }
    
    int total_width = cur_len * char_stride - DIGIT_SPACING;
    int start_x = (term_cols - total_width) / 2;
    int start_y = (term_rows - DIGIT_HEIGHT) / 2;
    
    // Set color to Cyan and bold
    printf("\033[1;36m"); 
    
    for (int i = 0; i < cur_len; i++) {
        // Redraw only if the string length changed OR the specific digit changed
        if (cur_len != last_len || current_str[i] != last_str[i]) {
            int digit = current_str[i] - '0';
            for (int r = 0; r < DIGIT_HEIGHT; r++) {
                printf("\033[%d;%dH%s", start_y + r, start_x + (i * char_stride), digits[digit][r]);
            }
        }
    }
    
    // Draw "PPS" static label below the numbers
    printf("\033[0;37m"); // Reset to white
    printf("\033[%d;%dH CSI Packets / Sec ", start_y + DIGIT_HEIGHT + 2, (term_cols - 19) / 2);
    
    fflush(stdout);
    last_rate = rate;
}

void* render_loop(void* arg) {
    while(atomic_load(&keep_running)) {
        // Slide window by 200ms
        usleep(200000); 
        
        // Atomically fetch the counts for the last 200ms and reset the counter
        int current_count = atomic_exchange(&current_bucket_count, 0);
        
        history[bucket_index] = current_count;
        bucket_index = (bucket_index + 1) % NUM_BUCKETS;

        int rate = 0;
        for(int i = 0; i < NUM_BUCKETS; i++) {
            rate += history[i];
        }

        draw_rate(rate);
    }
    return NULL;
}

int vespercsi_plugin_api_init(vespercsi_plugin_context_t* context){
    vespercsi_plugin_context = *context;

    if(vespercsi_plugin_context.sdk_version != VESPERCSI_PLUGIN_SDK_VERSION){
        LOG(FAIL, "flowrate: Plugin SDK version mismatch");
        return VESPERCSI_FAIL;
    }

    // Get terminal dimensions
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        term_rows = w.ws_row;
        term_cols = w.ws_col;
    }

    // Save canonical terminal settings and apply raw mode
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Enter alternate screen buffer to preserve user's scrollback history
    printf("\033[?1049h"); 
    // Hide cursor
    printf("\033[?25l");   
    // Clear screen
    printf("\033[2J");     
    fflush(stdout);

    // Initialize state
    atomic_store(&keep_running, true);
    atomic_store(&current_bucket_count, 0);
    last_rate = -1;
    
    // Start sliding window render thread
    pthread_create(&render_thread, NULL, render_loop, NULL);

    return VESPERCSI_OK;
}

int vespercsi_plugin_api_callback(const vespercsi_csi_t* csi){
    // Lock-free increment guarantees maximum callback throughput
    atomic_fetch_add(&current_bucket_count, 1);
    return VESPERCSI_OK;
}

int vespercsi_plugin_api_destroy(void){
    // Signal thread to stop and wait
    atomic_store(&keep_running, false);
    pthread_join(render_thread, NULL);

    // Restore terminal state
    printf("\033[2J");       // Clear the screen one last time
    printf("\033[?25h");     // Show cursor
    printf("\033[?1049l");   // Exit alternate screen buffer
    fflush(stdout);

    // Restore canonical mode
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

    return VESPERCSI_OK;
}
