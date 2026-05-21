#include <vespercsi/plugin/sdk.h>
#include <vespercsi/csi/types.h>
#include <vespercsi/csi/defs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

vespercsi_plugin_context_t vespercsi_plugin_context;

// --- Configuration ---
#define PLOT_WIDTH 80
#define PLOT_HEIGHT 20
#define WINDOW_SIZE 30
#define RENDER_INTERVAL_US 100000 // 100ms (10 FPS)

// Max flattened elements per packet (244 subcarriers * 2 TX * 2 RX = 976)
#define MAX_ELEMENTS (VESPERCSI_CSI_SUPPORTED_SUBCARRIERS_MAX * VESPERCSI_CSI_SUPPORTED_TX_MAX * VESPERCSI_CSI_SUPPORTED_RX_MAX)

// --- Threading & Shared State ---
pthread_t render_thread;
pthread_mutex_t data_lock = PTHREAD_MUTEX_INITIALIZER;
int plugin_running = 1;

// Sliding window for CSI amplitude
double amp_window[MAX_ELEMENTS][WINDOW_SIZE];
int window_idx = 0;
int window_count = 0;

// Graph history buffer
double graph_history[PLOT_WIDTH] = {0};

// --- Terminal Rendering Function ---
void* render_loop(void* arg) {
    char screen_buffer[(PLOT_WIDTH + 15) * PLOT_HEIGHT + 200]; 
    
    while (plugin_running) {
        pthread_mutex_lock(&data_lock);
        
        // Find dynamic max for scaling the Y-axis
        double max_val = 1.0; 
        for (int i = 0; i < PLOT_WIDTH; i++) {
            if (graph_history[i] > max_val) {
                max_val = graph_history[i];
            }
        }

        // Reset cursor to top-left without clearing the screen
        sprintf(screen_buffer, "\033[H\033[96m--- VesperCSI Live STD Activity Plot ---\033[0m\n\n");
        char* ptr = screen_buffer + strlen(screen_buffer);

        // Draw graph top to bottom
        for (int y = PLOT_HEIGHT; y > 0; y--) {
            double threshold = (max_val / PLOT_HEIGHT) * y;
            
            ptr += sprintf(ptr, "%6.1f | ", threshold);
            
            for (int x = 0; x < PLOT_WIDTH; x++) {
                if (graph_history[x] >= threshold) {
                    ptr += sprintf(ptr, "\033[92m█\033[0m"); 
                } else {
                    ptr += sprintf(ptr, " ");
                }
            }
            ptr += sprintf(ptr, "\n");
        }
        
        pthread_mutex_unlock(&data_lock);
        
        printf("%s", screen_buffer);
        fflush(stdout);
        
        usleep(RENDER_INTERVAL_US);
    }
    return NULL;
}

// --- VesperCSI API Implementation ---

int vespercsi_plugin_api_init(vespercsi_plugin_context_t* context) {
    // 1. Immediately clear the screen and set cursor to top-left
    printf("\033[2J\033[H");
    fflush(stdout);

    vespercsi_plugin_context = *context;

    if(vespercsi_plugin_context.sdk_version != VESPERCSI_PLUGIN_SDK_VERSION){
        LOG(FAIL, "livestd: Plugin SDK version mismatch");
        return VESPERCSI_FAIL;
    }

    memset(amp_window, 0, sizeof(amp_window));
    
    // Start background render thread
    if (pthread_create(&render_thread, NULL, render_loop, NULL) != 0) {
        LOG(FAIL, "livestd: Failed to create render thread");
        return VESPERCSI_FAIL;
    }

    // Using INFO instead of OK based on level.h
    LOG(INFO, "livestd: Live plotting plugin initialized successfully.");
    return VESPERCSI_OK;
}

int vespercsi_plugin_api_callback(const vespercsi_csi_t* csi) {
    if (!csi) return VESPERCSI_FAIL;

    int num_sub = csi->header.num_subcarriers;
    int num_rx = csi->header.num_rx;
    int num_tx = csi->header.num_tx;
    
    int total_elements = num_sub * num_rx * num_tx;
    if (total_elements > MAX_ELEMENTS) {
        total_elements = MAX_ELEMENTS;
    }

    pthread_mutex_lock(&data_lock);

    // Cast flat byte array to 16-bit integer array to extract real/imag parts
    int16_t *raw_csi = (int16_t *)csi->data.csi_data;
    
    for (int i = 0; i < total_elements; i++) {
        double real = (double)raw_csi[2 * i];
        double imag = (double)raw_csi[(2 * i) + 1];
        
        double amp = sqrt((real * real) + (imag * imag));
        amp_window[i][window_idx] = amp;
    }

    window_idx = (window_idx + 1) % WINDOW_SIZE;
    if (window_count < WINDOW_SIZE) window_count++;

    // Compute Mean STD across all extracted elements
    double total_std = 0.0;
    if (window_count > 1) {
        for (int i = 0; i < total_elements; i++) {
            double sum = 0.0, mean = 0.0, variance_sum = 0.0;
            
            for (int t = 0; t < window_count; t++) sum += amp_window[i][t];
            mean = sum / window_count;
            
            for (int t = 0; t < window_count; t++) {
                double diff = amp_window[i][t] - mean;
                variance_sum += (diff * diff);
            }
            
            total_std += sqrt(variance_sum / window_count);
        }
    }
    
    double current_score = (total_elements > 0) ? (total_std / total_elements) : 0.0;

    // Shift graph history left and insert new score on the right
    for (int i = 0; i < PLOT_WIDTH - 1; i++) {
        graph_history[i] = graph_history[i + 1];
    }
    graph_history[PLOT_WIDTH - 1] = current_score;

    pthread_mutex_unlock(&data_lock);

    return VESPERCSI_OK;
}

int vespercsi_plugin_api_destroy(void) {
    plugin_running = 0;
    pthread_join(render_thread, NULL);
    
    // Clear screen once more on exit so the terminal is usable afterward
    printf("\033[2J\033[H");
    fflush(stdout);
    
    // Using INFO instead of OK based on level.h
    LOG(INFO, "livestd: Plugin destroyed gracefully.");
    return VESPERCSI_OK;
}
