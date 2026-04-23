#include <vespercsi/wifi.h>
#include <vespercsi/options.h>
#include <vespercsi/log.h>
#include <vespercsi/plugin.h>


// Global Options Struct
vespercsi_options_t vespercsi_options = {
    .interface_mac_set = false,
    .interface_mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
    .antenna = RATE_MCS_ANT_A_MSK,
    .frequency = 2412,
    .format = "HT",
    .channel_width = 20,
    .num_rx = 0,
    .num_tx = 0,
    .verbose = false,
    .verbosity = 0,
    .log_file_set = false,
    .log_file_path = "",
    .out_file_set = false,
    .out_file_path = "",
    .quiet = false,
    .bandwidth = "20",
    .src_mac_set = false,
    .src_mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
    .unrestricted = false,
    .netlink_buffer_instant_death = false,
    .ring_buffer_instant_death = false
};

void vespercsi_options_parse(int argc, char* argv[]){

    static const struct option long_options[] = {
        { "help",           no_argument,        NULL,   'h'},    
        { "interface-mac",  required_argument,  NULL,   'i'},
        { "frequency",      required_argument,  NULL,   'f'},
        { "format",         required_argument,  NULL,   'F'},
        { "channel-width",  required_argument,  NULL,   'w'},
        { "num-rx",         required_argument,  NULL,   'r'},
        { "num-tx",         required_argument,  NULL,   't'},
        { "plugin",         required_argument,  NULL,   'p'},
        { "plugin-config",  required_argument,  NULL,   'P'},
        { "verbose",        optional_argument,  NULL,   'v'},
        { "version",        no_argument,        NULL,   1000},
        { "log",       required_argument,  NULL,   'l'},
        { "out",       required_argument,  NULL,   'o'}, 
       { "quiet",          no_argument,        NULL,   'q'},
       { "src-mac",        required_argument,  NULL,   's'},
       { "config",    required_argument,  NULL,   'c'},
       { "unrestricted",   no_argument,        NULL,   'u'},      
       { "daemon",         no_argument,        NULL,   'd'},       
       { "netlink-buffer-instant-death",     no_argument,   NULL,1001},
       { "ring-buffer-instant-death",        no_argument,   NULL,1002},       
       { NULL,             0,                  NULL,   0  }   
    };

    optind = 0;
    int opt = 0;
    
    while((opt = getopt_long(argc, argv, ":vhi:F:t:r:w:l:o:qp:s:c:ud",long_options,NULL)) != -1){

        switch(opt){
    
            case 'h':{
                // Print usage help
                vespercsi_options_print_usage();
                exit(EXIT_SUCCESS);
            }break;

            case 'v':{
                vespercsi_options.verbose = true;
                vespercsi_options.quiet = false;
                
                if(optarg){
                    int verbosity = atoi(optarg);
                    if(verbosity < 0 || verbosity > 2){
                        printf( "VesperCSI: Verbose level not correct, possible values: 0,1,2\n");
                        exit(EXIT_FAILURE);                    
                    }
                    vespercsi_options.verbosity = verbosity;
                }

            }break;

            case 'q':{
                vespercsi_options.verbose = false;
                vespercsi_options.verbosity = 0;
                vespercsi_options.quiet = true;
            }break;            

            case 'u':{
                vespercsi_options.unrestricted = true;
            }break;     

            case 'l':{
                vespercsi_options.log_file_set = true;
                strncpy(vespercsi_options.log_file_path, optarg, sizeof(vespercsi_options.log_file_path)-1);
            }break;

            case 'o':{
                vespercsi_options.out_file_set = true;
                strncpy(vespercsi_options.out_file_path, optarg, sizeof(vespercsi_options.out_file_path)-1);
            }break;            

            case 'c':{
                vespercsi_options.config_file_set = true;
                strncpy(vespercsi_options.config_file_path, optarg, sizeof(vespercsi_options.config_file_path)-1);
            }break;  

            case 'd':{
                // TODO: Implement in the future
                // For now it is undocumented
                vespercsi_options.daemon = true;
            }break;

            case 'f':{
                int f = atoi(optarg);
                if(f <= 0){
                    printf( "VesperCSI: Frequency value is not correct\n");
                    exit(EXIT_FAILURE);
                }
                vespercsi_options.frequency = f;
            }break;

            case 'w':{
                vespercsi_wifi_channel_mode_t mode = vespercsi_wifi_get_channel_mode(optarg);

                if(mode.width == 0){
                    printf("VesperCSI: Bad bandwidth. Possible values of bandwidth are [20|40|HT40-|80|160]\n");
                    exit(EXIT_FAILURE);
                }
                strncpy(vespercsi_options.bandwidth, optarg, sizeof(vespercsi_options.bandwidth)-1);
                vespercsi_options.channel_width = vespercsi_wifi_channel_mode_to_width(&mode);
            }break;

            case 'r':{
                int n = atoi(optarg);
                if(n < 1 || n > 2){
                    printf("VesperCSI: Bad RX count\n");
                    exit(EXIT_FAILURE);
                }
                vespercsi_options.num_rx = n;
            }break;

            case 't':{
                int n = atoi(optarg);
                if(n < 1 || n > 2){
                    printf("VesperCSI: Bad TX count\n");
                    exit(EXIT_FAILURE);
                }
                vespercsi_options.num_tx = n;
            }break;

            case 'P':{
                strncpy(vespercsi_plugin.config_path, optarg, sizeof(vespercsi_plugin.config_path)-1);
            }break;

            case 'p':{
                if(!vespercsi_plugin.set){
                    strncpy(vespercsi_plugin.path, optarg, sizeof(vespercsi_plugin.path)-1);
                    vespercsi_plugin.set = true;
                }
            }break;

            case 'i':{
                int res = sscanf(optarg, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", 
                    &vespercsi_options.interface_mac[0], &vespercsi_options.interface_mac[1], &vespercsi_options.interface_mac[2], &vespercsi_options.interface_mac[3], &vespercsi_options.interface_mac[4], &vespercsi_options.interface_mac[5]);

                if(res != ETH_ALEN){
                    printf("VesperCSI: Interface MAC cannot be parsed\n");
                    exit(EXIT_FAILURE);
                }
                vespercsi_options.interface_mac_set = true;

            }break;

            case 's':{
                int res = sscanf(optarg, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", 
                    &vespercsi_options.src_mac[0], &vespercsi_options.src_mac[1], &vespercsi_options.src_mac[2], &vespercsi_options.src_mac[3], &vespercsi_options.src_mac[4], &vespercsi_options.src_mac[5]);

                if(res != ETH_ALEN){
                    printf("VesperCSI: Source MAC cannot be parsed\n");
                    exit(EXIT_FAILURE);
                }
                vespercsi_options.src_mac_set = true;

            }break;

            case 'F':{
                if(strcmp(optarg, "NOHT") == 0){
                    strcpy(vespercsi_options.format, "NOHT");
                }else if(strcmp(optarg, "HT") == 0){
                    strcpy(vespercsi_options.format, "HT");
                }else if(strcmp(optarg, "VHT") == 0){
                    strcpy(vespercsi_options.format, "VHT");
                }else if(strcmp(optarg, "HESU") == 0){
                    strcpy(vespercsi_options.format, "HESU");
                }else{
                    printf("VesperCSI: Bad format. Possible values [NOHT|HT|VHT|HESU]\n");
                    exit(EXIT_FAILURE);
                }

            }break;

            case '?':{
                printf("VesperCSI: The command line option does not exists\n");
                exit(EXIT_FAILURE);
            }break;

            case ':':{
               printf("VesperCSI: Option needs argument\n");
               exit(EXIT_FAILURE);
            }
            break;

            case 1000:{
                // Version
                printf("VesperCSI: Version: %s\n", VESPERCSI_VERSION);
                exit(EXIT_SUCCESS);
            }break;

            case 1001:{
                vespercsi_options.netlink_buffer_instant_death = true;
            }break;

            case 1002:{
                vespercsi_options.ring_buffer_instant_death = true;
            }break;
        }
    
    }
    
    return;
}

void vespercsi_options_check_mandatory(){

    if(vespercsi_options.interface_mac_set == false){
        printf("VesperCSI: Interface MAC is not set\n");
        exit(EXIT_FAILURE);
    }
}

void vespercsi_options_print_usage() {
    printf(
        "Usage: vespercsi [OPTIONS]\n\n"
        "VesperCSI - Wi-Fi Channel State Information Capture Tool\n\n"
        "Capture Settings:\n"
        "  -i, --interface-mac <mac>   [Required] MAC address of the capture interface\n"
        "  -f, --frequency <freq>      Center frequency in MHz (e.g., 2412, 5180)\n"
        "  -w, --channel-width <width> Channel bandwidth [20|40|HT40-|80|160]\n"
        "  -t, --format <format>       CSI format [NOHT|HT|VHT|HESU]\n\n"
        "Filtering Options:\n"
        "  -s, --src-mac <mac>         Filter incoming packets by source MAC address\n"
        "  -u, --unrestricted          Do not restrict capture to matched channel width and format\n\n"
        "Output & Plugins:\n"
        "  -o, --out-file <file>       Path to write raw CSI data\n"
        "  -p, --plugin <path>         Path to load an external shared library plugin (.so)\n"
        "  -c, --config-file <file>    Path to configuration file for extra functionality\n\n"
        "Logging & Output Control:\n"
        "  -l, --log-file <file>       Path to write application logs\n"
        "  -v, --verbose[=level]       Enable verbose debug output (levels: 0, 1, 2)\n"
        "  -q, --quiet                 Suppress all console output (overrides verbose)\n\n"
        "Miscellaneous:\n"
        "  -h, --help                  Show this help message and exit\n"
        "      --version               Show program version and exit\n"
    );    
}
