# ESP32-C6 Zigbee Bridge OS - Build Makefile
#
# ESP32 targets: build, flash, monitor, flash-monitor
# Host targets: all, test, run, clean

# ESP32 serial port - override with: make flash PORT=/dev/tty.usbmodem...
# Find your port: ls /dev/tty.usb* (macOS) or ls /dev/ttyUSB* (Linux)
PORT ?= /dev/tty.usbmodem5AAF1845231

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O0
CFLAGS += -DOS_PLATFORM_HOST=1
CFLAGS += -D_GNU_SOURCE

# Include paths
INCLUDES = -I os/include \
           -I services/include \
           -I services/ha_disc \
           -I services/local_node \
           -I adapters/mqtt_adapter \
           -I drivers/zigbee \
           -I drivers/gpio_button \
           -I drivers/i2c_sensor \
           -I apps/src

# Source files
OS_SRCS = os/src/os.c \
          os/src/os_fibre.c \
          os/src/os_event.c \
          os/src/os_log.c \
          os/src/os_console.c \
          os/src/os_shell.c \
          os/src/os_persist.c

SVC_SRCS = services/src/registry.c \
           services/src/reg_shell.c \
           services/src/interview.c \
           services/src/capability.c \
           services/ha_disc/ha_disc.c \
           services/local_node/local_node.c \
           services/src/quirks.c

ADAPT_SRCS = adapters/mqtt_adapter/mqtt_adapter.c

DRV_SRCS = drivers/zigbee/zb_fake.c \
           drivers/gpio_button/gpio_button.c \
           drivers/i2c_sensor/i2c_sensor.c

APP_SRCS = apps/src/app_blink.c

MAIN_SRCS = main/src/main.c

TEST_SRCS = tests/unit/test_os.c \
            tests/unit/test_ha_disc.c \
            tests/unit/test_zb_adapter.c \
            tests/unit/test_local_node.c

# Object files
OS_OBJS = $(OS_SRCS:.c=.o)
SVC_OBJS = $(SVC_SRCS:.c=.o)
ADAPT_OBJS = $(ADAPT_SRCS:.c=.o)
DRV_OBJS = $(DRV_SRCS:.c=.o)
APP_OBJS = $(APP_SRCS:.c=.o)
MAIN_OBJS = $(MAIN_SRCS:.c=.o)
TEST_OBJS = $(TEST_SRCS:.c=.o)

# Targets
MAIN_TARGET = build/bridge
TEST_TARGET = build/test_os

# Libraries
LIBS = -lpthread

# ESP32 targets: idf.py convenience wrappers
.PHONY: all clean test run esp build flash monitor console help

# ESP32 targets
build:
	idf.py build

flash:
	idf.py -p $(PORT) flash

monitor:
	idf.py -p $(PORT) monitor

flash-monitor: flash monitor

menuconfig:
	idf.py menuconfig

# Host targets
all: $(MAIN_TARGET)

help:
	@echo "ESP32-C6 GatewayOS Build System"
	@echo ""
	@echo "ESP32 targets:"
	@echo "  make esp       - Build ESP32 firmware"
	@echo "  make flash     - Flash to device"
	@echo "  make monitor   - Start serial console"
	@echo "  make console   - Flash + start console"
	@echo ""
	@echo "Host targets:"
	@echo "  make all       - Build host binary"
	@echo "  make test      - Run unit tests"
	@echo "  make run       - Run host binary"
	@echo "  make clean     - Remove build artifacts"
	@echo ""
	@echo "Options:"
	@echo "  PORT=/dev/xxx  - Override serial port (default: $(PORT))"

# ESP32 convenience alias
esp: build

$(MAIN_TARGET): $(OS_OBJS) $(SVC_OBJS) $(ADAPT_OBJS) $(DRV_OBJS) $(APP_OBJS) $(MAIN_OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
	@echo "Built: $@"

$(TEST_TARGET): $(TEST_OBJS) os/src/os_event.o os/src/os_log.o os/src/os_fibre.o os/src/os_persist.o services/src/registry.o services/src/interview.o services/src/capability.o services/src/quirks.o services/ha_disc/ha_disc.o services/local_node/local_node.o adapters/mqtt_adapter/mqtt_adapter.o $(DRV_OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@
	@echo "Built: $@"

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TEST_TARGET)
	@echo ""
	@echo "Running tests..."
	@./$(TEST_TARGET)

run: $(MAIN_TARGET)
	@echo ""
	@echo "Running bridge..."
	@./$(MAIN_TARGET)

clean:
	rm -f $(OS_OBJS) $(SVC_OBJS) $(ADAPT_OBJS) $(DRV_OBJS) $(APP_OBJS) $(MAIN_OBJS) $(TEST_OBJS)
	rm -rf build/
	@echo "Cleaned"

# Dependencies
os/src/os.o: os/include/os.h os/include/os_types.h os/include/os_config.h
os/src/os_fibre.o: os/include/os_fibre.h os/include/os_types.h os/include/os_config.h
os/src/os_event.o: os/include/os_event.h os/include/os_types.h os/include/os_config.h
os/src/os_log.o: os/include/os_log.h os/include/os_types.h os/include/os_config.h
os/src/os_console.o: os/include/os_console.h os/include/os_types.h os/include/os_config.h
os/src/os_shell.o: os/include/os_shell.h os/include/os_types.h os/include/os_config.h
os/src/os_persist.o: os/include/os_persist.h os/include/os_types.h os/include/os_config.h
services/src/registry.o: services/include/registry.h services/include/reg_types.h os/include/os.h
services/src/reg_shell.o: services/include/registry.h os/include/os.h
services/src/interview.o: services/include/interview.h services/include/registry.h os/include/os.h
services/src/capability.o: services/include/capability.h services/include/registry.h os/include/os.h
services/ha_disc/ha_disc.o: services/ha_disc/ha_disc.h services/include/capability.h services/include/registry.h adapters/mqtt_adapter/mqtt_adapter.h os/include/os.h
services/local_node/local_node.o: services/local_node/local_node.h services/include/capability.h services/include/registry.h drivers/gpio_button/gpio_button.h drivers/i2c_sensor/i2c_sensor.h os/include/os.h
services/src/quirks.o: services/include/quirks.h services/include/capability.h os/include/os.h
adapters/mqtt_adapter/mqtt_adapter.o: adapters/mqtt_adapter/mqtt_adapter.h services/include/capability.h os/include/os.h
drivers/zigbee/zb_fake.o: drivers/zigbee/zb_adapter.h os/include/os_event.h os/include/os_log.h
drivers/gpio_button/gpio_button.o: drivers/gpio_button/gpio_button.h os/include/os_fibre.h
drivers/i2c_sensor/i2c_sensor.o: drivers/i2c_sensor/i2c_sensor.h os/include/os_fibre.h
apps/src/app_blink.o: apps/src/app_blink.h os/include/os.h
main/src/main.o: os/include/os.h apps/src/app_blink.h
tests/unit/test_os.o: os/include/os_types.h os/include/os_event.h os/include/os_log.h tests/unit/test_ha_disc.h tests/unit/test_zb_adapter.h tests/unit/test_local_node.h tests/unit/test_support.h
tests/unit/test_local_node.o: services/local_node/local_node.h drivers/gpio_button/gpio_button.h drivers/i2c_sensor/i2c_sensor.h os/include/os_types.h tests/unit/test_support.h
