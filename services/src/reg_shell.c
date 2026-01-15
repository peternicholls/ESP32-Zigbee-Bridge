/**
 * @file reg_shell.c
 * @brief Registry shell commands
 * 
 * ESP32-C6 Zigbee Bridge OS - Shell commands for device registry
 */

#include "registry.h"
#include "os.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Command: devices - List all devices */
static int cmd_devices(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    uint32_t count = reg_node_count();
    
    if (count == 0) {
        printf("No devices registered.\n");
        return 0;
    }
    
    printf("%-18s %-6s %-12s %-20s %-20s\n",
           "IEEE ADDRESS", "NWK", "STATE", "MANUFACTURER", "MODEL");
    printf("------------------ ------ ------------ -------------------- --------------------\n");
    
    for (uint32_t i = 0; i < count; i++) {
        reg_node_info_t info;
        if (reg_get_node_info(i, &info) == OS_OK) {
            printf(OS_EUI64_FMT " 0x%04X %-12s %-20.20s %-20.20s\n",
                   OS_EUI64_ARG(info.ieee_addr),
                   info.nwk_addr,
                   reg_state_name(info.state),
                   info.manufacturer[0] ? info.manufacturer : "-",
                   info.model[0] ? info.model : "-");
        }
    }
    
    printf("\nTotal: %u device(s)\n", count);
    
    return 0;
}

/* Command: device <id> - Show device details */
static int cmd_device(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: device <ieee_addr|nwk_addr>\n");
        return -1;
    }
    
    reg_node_t *node = NULL;
    
    /* Try to parse as hex IEEE address */
    if (strlen(argv[1]) >= 16) {
        os_eui64_t addr = strtoull(argv[1], NULL, 16);
        node = reg_find_node(addr);
    } else {
        /* Try as network address */
        uint16_t nwk = (uint16_t)strtoul(argv[1], NULL, 16);
        node = reg_find_node_by_nwk(nwk);
    }
    
    if (!node) {
        printf("Device not found: %s\n", argv[1]);
        return -1;
    }
    
    printf("Device: " OS_EUI64_FMT "\n", OS_EUI64_ARG(node->ieee_addr));
    printf("  Network addr:   0x%04X\n", node->nwk_addr);
    printf("  State:          %s\n", reg_state_name(node->state));
    printf("  Manufacturer:   %s\n", node->manufacturer[0] ? node->manufacturer : "-");
    printf("  Model:          %s\n", node->model[0] ? node->model : "-");
    printf("  Friendly name:  %s\n", node->friendly_name[0] ? node->friendly_name : "-");
    printf("  LQI:            %u\n", node->lqi);
    printf("  RSSI:           %d dBm\n", node->rssi);
    printf("  Power source:   %s\n", 
           node->power_source == REG_POWER_MAINS ? "Mains" :
           node->power_source == REG_POWER_BATTERY ? "Battery" :
           node->power_source == REG_POWER_DC ? "DC" : "Unknown");
    printf("  Endpoints:      %u\n", node->endpoint_count);
    
    /* List endpoints */
    for (uint8_t i = 0; i < REG_MAX_ENDPOINTS; i++) {
        if (node->endpoints[i].valid) {
            reg_endpoint_t *ep = &node->endpoints[i];
            printf("\n  Endpoint %d (profile=0x%04X device=0x%04X):\n",
                   ep->endpoint_id, ep->profile_id, ep->device_id);
            
            /* List clusters */
            for (uint8_t j = 0; j < REG_MAX_CLUSTERS; j++) {
                if (ep->clusters[j].valid) {
                    reg_cluster_t *cl = &ep->clusters[j];
                    printf("    Cluster 0x%04X (%s) - %u attrs\n",
                           cl->cluster_id,
                           cl->direction == REG_CLUSTER_SERVER ? "server" : "client",
                           cl->attr_count);
                }
            }
        }
    }
    
    return 0;
}

/* Register registry shell commands */
os_err_t reg_shell_init(void) {
    static const os_shell_cmd_t cmds[] = {
        {"devices", "List all registered devices", cmd_devices},
        {"device",  "Show device details <addr>",  cmd_device},
    };
    
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        os_err_t err = os_shell_register(&cmds[i]);
        if (err != OS_OK) {
            return err;
        }
    }
    
    return OS_OK;
}
