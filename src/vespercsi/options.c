#include <vespercsi/wifi.h>
#include <vespercsi/options.h>
#include <vespercsi/log.h>


// Global Options Struct
vespercsi_options_t vespercsi_options = {
    .interface_mac_set = false,
    .interface_mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
    .antenna = RATE_MCS_ANT_A_MSK,
    .frequency = 2412,
    .format = "HT",
    .channel_width = 20,
    .verbose = false,
    .log_file_set = false,
    .log_file_path = "",
    .out_file_set = false,
    .out_file_path = "",
    .quiet = false,
    .udp_port_set = false,
    .udp_port = 8000,
    .bandwidth = "20",
    .tx_power = 10

};

void vespercsi_options_parse(int argc, char* argv[]){

    static const struct option long_options[] = {
        { "help",           no_argument,        NULL,   'h'},    
        { "interface-mac",  required_argument,  NULL,   'i'},
        { "frequency",      required_argument,  NULL,   'f'},
        { "format",         required_argument,  NULL,   't'},
        { "channel-width",  required_argument,  NULL,   'w'},
        { "tx-power",       required_argument,  NULL,   'p'},
        { "verbose",        no_argument,        NULL,   'v'},
        { "version",        no_argument,        NULL,   1000},
        { "log-file",       required_argument,  NULL,   'l'},
        { "out-file",       required_argument,  NULL,   'o'}, 
        { "quiet",          no_argument,        NULL,   'q'},
       { "udp",            required_argument,  NULL,   'u'},
       { NULL,             0,                  NULL,   0  }   
    };

    optind = 0;
    int opt = 0;
    
    while((opt = getopt_long(argc, argv, ":vhi:f:t:w:l:o:qu:p:",long_options,NULL)) != -1){

        switch(opt){
    
            case 'h':{
                // Print usage help
                vespercsi_options_print_usage();
                exit(EXIT_SUCCESS);
            }break;

            case 'v':{
                vespercsi_options.verbose = true;
                vespercsi_options.quiet = false;
            }break;

            case 'q':{
                vespercsi_options.verbose = false;
                vespercsi_options.quiet = true;
            }break;            

            case 'u':{
                unsigned long int port = strtoul(optarg, NULL, 10);
                if(port > 65535){
                    printf("VesperCSI: Bad UDP Port\n");
                    exit(EXIT_FAILURE);
                }
                vespercsi_options.udp_port_set = true;
                vespercsi_options.udp_port = port;
            }break;

            case 'l':{
                vespercsi_options.log_file_set = true;
                strncpy(vespercsi_options.log_file_path, optarg, 512-1);
            }break;

            case 'o':{
                vespercsi_options.out_file_set = true;
                strncpy(vespercsi_options.out_file_path, optarg, 512-1);
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
                strncpy(vespercsi_options.bandwidth, optarg, 127);
                vespercsi_options.channel_width = vespercsi_wifi_channel_mode_to_width(&mode);
            }break;

            case 'p':{

                int tx = atoi(optarg);
                if (tx < 1 || tx > 22){
                    printf("VesperCSI: Bad tx power. Possible values [1-22]\n");
                    exit(EXIT_FAILURE);
                }
                vespercsi_options.tx_power = (uint8_t)tx;

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

            case 't':{
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

void vespercsi_options_print_usage(){
    printf(
        "Usage: %s [OPTIONS]\n\n"
        "VesperCSI - Wi-Fi Channel State Information Capture Tool\n\n"
        "Capture Settings:\n"
        "  -i, --interface-mac <mac>   [Required] MAC address of the capture interface\n"
        "  -f, --frequency <freq>      Center frequency in MHz (e.g., 2412, 5180)\n"
        "  -w, --channel-width <width> Channel width in MHz (e.g., 20, 40, 80, 160)\n"
        "  -t, --format <format>       CSI format (e.g., HT, VHT, HE)\n\n"
        "Output & Networking:\n"
        "  -o, --out-file <file>       Path to write raw CSI data\n"
        "  -u, --udp <ip:port>         Stream CSI data over UDP\n\n"
        "Logging & Output Control:\n"
        "  -l, --log-file <file>       Path to write application logs\n"
        "  -v, --verbose               Enable verbose debug output\n"
        "  -q, --quiet                 Suppress all console output (overrides verbose)\n\n"
        "Miscellaneous:\n"
        "  -h, --help                  Show this help message and exit\n"
        "      --version               Show program version and exit\n",
        "vespercsi"
    );    
}
