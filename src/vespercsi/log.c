#include <vespercsi/options.h>
#include <vespercsi/log.h>

void vespercsi_log(vespercsi_log_level_t log_level, const char* format, ...){

    if(vespercsi_options.quiet){
        return;
    }

    static FILE* log_file = NULL;

    if(log_file == NULL){
        if(vespercsi_options.log_file){
            log_file = fopen(vespercsi_options.log_file_path, "a");

            if(log_file == NULL){
                LOG(FAIL, "Cannot open log file at %s", vespercsi_options.log_file_path);
                exit(EXIT_FAILURE);
            }
        }else{
            log_file = stdout;
        }
    }

    va_list vl;
    va_start(vl, format);

    char log_buffer[1024] = {0};
    char variable_buffer[512] = {0};

    vsprintf(variable_buffer, format, vl);

    if(log_file != stdout){
        switch(log_level){        
            case VESPERCSI_LOG_LEVEL_FAIL:
                sprintf(log_buffer, "[FAIL] %s", variable_buffer);
            break; 
        
            case VESPERCSI_LOG_LEVEL_INFO:
                sprintf(log_buffer, "[INFO] %s", variable_buffer);
                break;
            
            case VESPERCSI_LOG_LEVEL_PASS:
                sprintf(log_buffer, "[PASS] %s", variable_buffer);
                break;

            case VESPERCSI_LOG_LEVEL_WARN:
                sprintf(log_buffer, "[WARN] %s", variable_buffer);
                break;

            case VESPERCSI_LOG_LEVEL_DEBG:
                sprintf(log_buffer, "[DEBG] %s", variable_buffer);
                break;
        }
    }else{
        switch(log_level){        
            case VESPERCSI_LOG_LEVEL_FAIL:
                sprintf(log_buffer, "[" VESPERCSI_LOG_COLOR_RED "FAIL" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer);
            break; 
        
            case VESPERCSI_LOG_LEVEL_INFO:
                sprintf(log_buffer, "[" VESPERCSI_LOG_COLOR_BLUE "INFO" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer);
                break;
            
            case VESPERCSI_LOG_LEVEL_PASS:
                sprintf(log_buffer, "[" VESPERCSI_LOG_COLOR_GREEN "PASS" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer);
                break;

            case VESPERCSI_LOG_LEVEL_WARN:
                sprintf(log_buffer, "[" VESPERCSI_LOG_COLOR_YELLOW "WARN" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer);
                break;

            case VESPERCSI_LOG_LEVEL_DEBG:
                sprintf(log_buffer, "[" VESPERCSI_LOG_COLOR_MAGENTA "DEBG" VESPERCSI_LOG_COLOR_CLEAR "] %s", variable_buffer);
                break;
        }
    }

    fprintf(log_file, "%s\r\n", log_buffer);
    fflush(log_file);
    
    va_end(vl);
    return;
}
