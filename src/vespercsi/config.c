#include <vespercsi/config.h>
#include <vespercsi/options.h>

vespercsi_config_t vespercsi_config;

void vespercsi_config_init(vespercsi_config_t* config){

    if(!vespercsi_options.config_file_set){
        return;
    }

    config_t cfg = {0};

    if (!config_read_file(&cfg, vespercsi_options.config_file_path)) {
        fprintf(stdout, "VesperCSI: config error: %s (line %d)\n",
            config_error_text(&cfg),
            config_error_line(&cfg));
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }

    int temp_int;
    const char* temp_str;

    if(config_lookup_int(&cfg, "runners.capture.core", &temp_int)){
        if(temp_int < 0 || temp_int > 31){
            fprintf(stdout, "VesperCSI: config error: invalid cpu core, valid options: 0-31");
            exit(EXIT_FAILURE);
        }
        vespercsi_config.runners.capture.core_set = true;
        vespercsi_config.runners.capture.core = temp_int;
    }

    if(config_lookup_int(&cfg, "runners.consume.core", &temp_int)){
        if(temp_int < 0 || temp_int > 31){
            fprintf(stdout, "VesperCSI: config error: invalid cpu core, valid options: 0-31");
            exit(EXIT_FAILURE);
        }
        vespercsi_config.runners.consume.core_set = true;
        vespercsi_config.runners.consume.core = temp_int;
    }

    if(config_lookup_int(&cfg, "runners.capture.priority", &temp_int)){
        if(temp_int < 0 || temp_int > 99){
            fprintf(stdout, "VesperCSI: config error: invalid priority, valid options: 0-99");
            exit(EXIT_FAILURE);
        }
        vespercsi_config.runners.capture.priority_set = true;
        vespercsi_config.runners.capture.priority = temp_int;
    }

    if(config_lookup_int(&cfg, "runners.consume.priority", &temp_int)){
        if(temp_int < 0 || temp_int > 99){
            fprintf(stdout, "VesperCSI: config error: invalid priority, valid options: 0-99");
            exit(EXIT_FAILURE);
        }
        vespercsi_config.runners.consume.priority_set = true;
        vespercsi_config.runners.consume.priority = temp_int;
    }


    if(config_lookup_string(&cfg, "plugin.path", &temp_str)){
        vespercsi_config.plugin.path_set = true;
        strncpy(vespercsi_config.plugin.path, temp_str, sizeof(vespercsi_config.plugin.path)-1);
    }

    config_destroy(&cfg);
}