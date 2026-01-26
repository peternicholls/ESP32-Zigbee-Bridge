/**
 * @file zb_shell.c
 * @brief Zigbee shell commands for testing
 *
 * ESP32-C6 Zigbee Bridge OS - Shell commands for Zigbee adapter
 */

#include "os.h"
#include "zb_adapter.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Command: zb_on <ieee_addr> [endpoint] - Turn on a light */
static int cmd_zb_on(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: zb_on <ieee_addr> [endpoint]\n");
    printf("  Example: zb_on 001788010816AE07 11\n");
    return -1;
  }

  os_eui64_t addr = strtoull(argv[1], NULL, 16);
  uint8_t ep = (argc >= 3) ? (uint8_t)atoi(argv[2]) : 11; /* Hue default */

  printf("Sending ON to " OS_EUI64_FMT " ep=%u\n", OS_EUI64_ARG(addr), ep);

  os_err_t err = zba_send_onoff(addr, ep, true, 1);
  if (err != OS_OK) {
    printf("Error: %d\n", err);
    return -1;
  }
  printf("Command sent\n");
  return 0;
}

/* Command: zb_off <ieee_addr> [endpoint] - Turn off a light */
static int cmd_zb_off(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: zb_off <ieee_addr> [endpoint]\n");
    printf("  Example: zb_off 001788010816AE07 11\n");
    return -1;
  }

  os_eui64_t addr = strtoull(argv[1], NULL, 16);
  uint8_t ep = (argc >= 3) ? (uint8_t)atoi(argv[2]) : 11;

  printf("Sending OFF to " OS_EUI64_FMT " ep=%u\n", OS_EUI64_ARG(addr), ep);

  os_err_t err = zba_send_onoff(addr, ep, false, 2);
  if (err != OS_OK) {
    printf("Error: %d\n", err);
    return -1;
  }
  printf("Command sent\n");
  return 0;
}

/* Command: zb_level <ieee_addr> <level%> [transition_ms] [endpoint] */
static int cmd_zb_level(int argc, char *argv[]) {
  if (argc < 3) {
    printf(
        "Usage: zb_level <ieee_addr> <level%%> [transition_ms] [endpoint]\n");
    printf("  Example: zb_level 001788010816AE07 50 500\n");
    return -1;
  }

  os_eui64_t addr = strtoull(argv[1], NULL, 16);
  uint8_t level = (uint8_t)atoi(argv[2]);
  uint16_t trans = (argc >= 4) ? (uint16_t)atoi(argv[3]) : 500;
  uint8_t ep = (argc >= 5) ? (uint8_t)atoi(argv[4]) : 11;

  printf("Sending LEVEL %u%% to " OS_EUI64_FMT " ep=%u trans=%ums\n", level,
         OS_EUI64_ARG(addr), ep, trans);

  os_err_t err = zba_send_level(addr, ep, level, trans, 3);
  if (err != OS_OK) {
    printf("Error: %d\n", err);
    return -1;
  }
  printf("Command sent\n");
  return 0;
}

/* Command: zb_join - Enable permit join for 180 seconds */
static int cmd_zb_join(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  printf("Enabling permit join for 180 seconds...\n");

  os_err_t err = zba_set_permit_join(180);
  if (err != OS_OK) {
    printf("Error: %d\n", err);
    return -1;
  }
  printf("Permit join enabled\n");
  return 0;
}

/* Register zigbee shell commands */
os_err_t zba_shell_init(void) {
  static const os_shell_cmd_t cmds[] = {
      {"zb_on", "Turn on <ieee_addr> [ep]", cmd_zb_on},
      {"zb_off", "Turn off <ieee_addr> [ep]", cmd_zb_off},
      {"zb_level", "Set level <ieee> <%> [ms] [ep]", cmd_zb_level},
      {"zb_join", "Enable permit join", cmd_zb_join},
  };

  for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
    os_err_t err = os_shell_register(&cmds[i]);
    if (err != OS_OK) {
      return err;
    }
  }

  return OS_OK;
}
