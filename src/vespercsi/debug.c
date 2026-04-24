#include <vespercsi/debug.h>
#include <vespercsi/csi.h>
#include <vespercsi/wifi.h>
#include <vespercsi/config.h>
#include <vespercsi/netlink.h>
#include <vespercsi/options.h>


void vespercsi_debug_constants(void){
        printf("VesperCSI Debug: CSI Parameters\n");
        printf("-------------------------------\n");
        printf("VESPERCSI_CSI_SUPPORTED_TX_MAX:\t\t\t%d\n", VESPERCSI_CSI_SUPPORTED_TX_MAX);
        printf("VESPERCSI_CSI_SUPPORTED_RX_MAX:\t\t\t%d\n", VESPERCSI_CSI_SUPPORTED_RX_MAX);
        printf("VESPERCSI_CSI_SUPPORTED_SUBCARRIERS_MAX:\t%d\n", VESPERCSI_CSI_SUPPORTED_SUBCARRIERS_MAX);
        printf("VESPERCSI_CSI_SUPPORTED_TIME_FRAME_MAX:\t\t%d\n", VESPERCSI_CSI_SUPPORTED_TIME_FRAME_MAX);
        printf("VESPERCSI_CSI_SUPPORTED_RATE_MAX:\t\t%d\n", VESPERCSI_CSI_SUPPORTED_RATE_MAX);
        printf("VESPERCSI_CSI_HEADER_LENGTH:\t\t\t%d\n", VESPERCSI_CSI_HEADER_LENGTH);
        printf("VESPERCSI_CSI_DATA_SIZE_MAX:\t\t\t%d\n", VESPERCSI_CSI_DATA_SIZE_MAX);
        printf("VESPERCSI_CSI_SIZE_MAX:\t\t\t\t%d\n", VESPERCSI_CSI_SIZE_MAX);

        printf("\n\n");

        printf("VesperCSI Debug: Ring Buffers\n");
        printf("-----------------------------\n");
        printf("CSI Ringbuffer Size:\t\t\t\t2 x %d\n", VESPERCSI_CSI_SUPPORTED_RATE_MAX * VESPERCSI_CSI_SUPPORTED_TIME_FRAME_MAX);
        printf("CSI Whole Structure Size:\t\t\t%zu\n", sizeof(vespercsi_csi_t));
        printf("CSI Header Structure Size:\t\t\t%zu\n", sizeof(vespercsi_csi_header_t));
        printf("CSI Data Structure Size:\t\t\t%zu\n", sizeof(vespercsi_csi_data_t));

        printf("\n\n");

        printf("VesperCSI Debug: Wi-Fi Parameters\n");
        printf("---------------------------------\n");
        printf("VESPERCSI_WIFI_INTERFACE_MAX_SIZE:\t\t%d\n", VESPERCSI_WIFI_INTERFACE_MAX_SIZE);
        printf("VESPERCSI_WIFI_PHY_MAX_SIZE:\t\t\t%d\n", VESPERCSI_WIFI_PHY_MAX_SIZE);
        printf("VESPERCSI_WIFI_INTERFACE_NAME_STR_SIZE:\t\t%d\n", VESPERCSI_WIFI_INTERFACE_NAME_STR_SIZE);
        printf("VESPERCSI_WIFI_PHY_NAME_STR_SIZE:\t\t%d\n", VESPERCSI_WIFI_PHY_NAME_STR_SIZE);
        printf("VESPERCSI_WIFI_MAC_STR_SIZE:\t\t\t%d\n", VESPERCSI_WIFI_MAC_STR_SIZE);
        printf("VESPERCSI_WIFI_INTERFACE_NAME:\t\t%s\n", VESPERCSI_WIFI_INTERFACE_NAME);

        printf("\n");

}
