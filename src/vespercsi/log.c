#include <vespercsi/options.h>
#include <vespercsi/log.h>

#include <vespercsi/options.h>
#include <vespercsi/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

FILE* vespercsi_log_file = NULL;

void vespercsi_log_init(void) {
    if (vespercsi_options.log_file_set) {
        vespercsi_log_file = fopen(vespercsi_options.log_file_path, "a");
        if (!vespercsi_log_file) {
            fprintf(stderr, "VesperCSI: Logger could not open file at %s\n",
                    vespercsi_options.log_file_path);
            exit(EXIT_FAILURE);
        }
    }
}

void vespercsi_log(vespercsi_log_level_t log_level, const char* format, ...) {
    va_list vl;
    char log_buffer[1024] = {0};
    char variable_buffer[512] = {0};
    
    // Safely format the user's string
    va_start(vl, format);
    vsnprintf(variable_buffer, sizeof(variable_buffer), format, vl);
    va_end(vl);

    // 1. Write to the file (uncolored)
    if (vespercsi_log_file) {
        switch (log_level) {        
            case VESPERCSI_LOG_LEVEL_FAIL: snprintf(log_buffer, sizeof(log_buffer), "[FAIL] %s", variable_buffer); break; 
            case VESPERCSI_LOG_LEVEL_INFO: snprintf(log_buffer, sizeof(log_buffer), "[INFO] %s", variable_buffer); break;
            case VESPERCSI_LOG_LEVEL_PASS: snprintf(log_buffer, sizeof(log_buffer), "[PASS] %s", variable_buffer); break;
            case VESPERCSI_LOG_LEVEL_WARN: snprintf(log_buffer, sizeof(log_buffer), "[WARN] %s", variable_buffer); break;
            case VESPERCSI_LOG_LEVEL_DEBG: snprintf(log_buffer, sizeof(log_buffer), "[DEBG] %s", variable_buffer); break;
            default:                       snprintf(log_buffer, sizeof(log_buffer), "[UNKN] %s", variable_buffer); break;
        }
        fprintf(vespercsi_log_file, "%s\n", log_buffer);
        fflush(vespercsi_log_file);
    }
    
    // 2. Write to standard output (colored)
    if (!vespercsi_options.quiet) {
        switch (log_level) {        
            case VESPERCSI_LOG_LEVEL_FAIL: snprintf(log_buffer, sizeof(log_buffer), "[" VESPERCSI_LOG_COLOR_RED "FAIL" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer); break; 
            case VESPERCSI_LOG_LEVEL_INFO: snprintf(log_buffer, sizeof(log_buffer), "[" VESPERCSI_LOG_COLOR_BLUE "INFO" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer); break;
            case VESPERCSI_LOG_LEVEL_PASS: snprintf(log_buffer, sizeof(log_buffer), "[" VESPERCSI_LOG_COLOR_GREEN "PASS" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer); break;
            case VESPERCSI_LOG_LEVEL_WARN: snprintf(log_buffer, sizeof(log_buffer), "[" VESPERCSI_LOG_COLOR_YELLOW "WARN" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer); break;
            case VESPERCSI_LOG_LEVEL_DEBG: snprintf(log_buffer, sizeof(log_buffer), "[" VESPERCSI_LOG_COLOR_MAGENTA "DEBG" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer); break;
            default:                       snprintf(log_buffer, sizeof(log_buffer), "[UNKN] %s", variable_buffer); break;
        }
        fprintf(stdout, "%s\n", log_buffer);
        fflush(stdout);        
    }
}