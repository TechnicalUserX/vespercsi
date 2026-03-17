#include <vespercsi/options.h>

// Global Options Struct
vespercsi_options_t vespercsi_options = {0};

void vespercsi_options_parse(int argc, char* argv[]){

    static const struct option long_options[] = {
        { "help",           no_argument,        NULL,   'h'},    
        { "interface",      required_argument,  NULL,   'i'},
        { "frequency",      required_argument,  NULL,   'f'},
        { "format",         required_argument,  NULL,   't'},
        { "channel-width",  no_argument,        NULL,   'w'},
        { "verbose",        required_argument,  NULL,   'v'},
        { "version",        no_argument,        NULL,   1000},
        { "log-file",       required_argument,  NULL,   'l'},
        { "quiet",          required_argument,  NULL,   'q'},
        { NULL,             0,                  NULL,   0   }   
    };

    optind = 0;
    int opt = 0;
    
    while((opt = getopt_long(argc, argv, ":vhi:f:t:w:lq",long_options,NULL)) != -1){

        switch(opt){
    
            case 'h':{
                // Print usage help
            }break;


            case 'v':{
                vespercsi_options.verbose = true;
                vespercsi_options.quiet = false;
            }break;

            case 'q':{
                vespercsi_options.verbose = false;
                vespercsi_options.quiet = true;
            }break;            


            case 'l':{
                vespercsi_options.log_file = true;
                strncpy(vespercsi_options.log_file_path, optarg, 512);
            }break;
            
            case 'r':{ // Negative values are not accepted

                char rts_interval_string[21] = {0};
                strncpy(rts_interval_string,optarg,20);
            }

    
        }
    
    }
    
    return;
}
