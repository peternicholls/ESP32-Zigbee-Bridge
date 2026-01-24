# Feature Specification: Wi-Fi Connectivity

**Feature Branch**: `002-wifi-connectivity`  
**Created**: 2026-01-20  
**Status**: Draft  
**Input**: Add Wi-Fi connectivity to enable MQTT communication with external broker for Home Assistant integration

## Overview

Enable Wi-Fi station mode on the ESP32-C6 to connect to a local network, allowing the MQTT adapter to reach an external broker (e.g., Home Assistant's Mosquitto). Wi-Fi credentials will initially be hardcoded, with shell-based provisioning as a follow-up. The Wi-Fi service MUST emit network events to the bus and handle reconnection automatically.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Connect to Wi-Fi Network (Priority: P1)

As a user, I want the bridge to connect to my home Wi-Fi so that it can communicate with my Home Assistant server.

**Why this priority**: Without network connectivity, the bridge cannot publish to MQTT or receive commands.

**Independent Test**: Configure SSID/password, boot device, verify IP address obtained and logged.

**Acceptance Scenarios**:

1. **Given** valid Wi-Fi credentials configured, **When** device boots, **Then** connects to network within 10 seconds
2. **Given** connection established, **When** `wifi status` shell command runs, **Then** shows SSID, IP address, RSSI
3. **Given** connection established, **When** `NET_UP` event checked, **Then** event was emitted to bus

---

### User Story 2 - Automatic Reconnection (Priority: P1)

As a user, I want the bridge to automatically reconnect if Wi-Fi drops so that the system recovers without intervention.

**Why this priority**: Home networks are unreliable; automatic recovery is essential for unattended operation.

**Independent Test**: Disconnect/reconnect router, verify bridge reconnects and MQTT resumes.

**Acceptance Scenarios**:

1. **Given** Wi-Fi is connected, **When** AP becomes unreachable, **Then** `NET_DOWN` event emitted
2. **Given** Wi-Fi is disconnected, **When** AP becomes reachable again, **Then** reconnection attempted automatically
3. **Given** reconnection succeeds, **When** connection restored, **Then** `NET_UP` event emitted
4. **Given** multiple disconnects, **When** reconnecting, **Then** exponential backoff applied (max 60s)

---

### User Story 3 - MQTT Broker Reachability (Priority: P2)

As a user, I want the MQTT adapter to connect to my broker once Wi-Fi is up so that I can monitor devices in Home Assistant.

**Why this priority**: This validates the end-to-end path from Wi-Fi → MQTT → Home Assistant.

**Independent Test**: Boot device with Wi-Fi configured, verify MQTT connection established in logs.

**Acceptance Scenarios**:

1. **Given** Wi-Fi is connected, **When** MQTT adapter starts, **Then** connects to configured broker
2. **Given** MQTT is connected, **When** `mqtt status` shell command runs, **Then** shows "connected"
3. **Given** broker is unreachable, **When** connection fails, **Then** retries with backoff

---

### User Story 4 - Credential Configuration via Shell (Priority: P3)

As a developer, I want to configure Wi-Fi credentials via shell so that I don't need to recompile for different networks.

**Why this priority**: Convenience for development; not critical for initial testing.

**Independent Test**: Use `wifi set <ssid> <pass>` command, reboot, verify connection to new network.

**Acceptance Scenarios**:

1. **Given** shell is active, **When** `wifi set MyNetwork MyPassword` entered, **Then** credentials persisted to NVS
2. **Given** credentials saved, **When** device reboots, **Then** connects using stored credentials
3. **Given** invalid credentials, **When** connection fails, **Then** error logged, device retries periodically

---

### Edge Cases

- What happens when SSID not found? → Scan periodically, log "SSID not found", emit `NET_DOWN`
- What happens when password wrong? → Auth failure logged, retry with backoff, emit `NET_DOWN`
- What happens when DHCP times out? → Retry DHCP, log timeout, emit `NET_DOWN` if persistent
- What happens during Wi-Fi/Zigbee coexistence? → ESP-IDF handles coex automatically; no special handling needed
- What happens if no credentials configured? → Log warning, Wi-Fi service remains idle, MQTT offline

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST connect to Wi-Fi in station mode using ESP-IDF Wi-Fi driver
- **FR-002**: System MUST emit `NET_UP` event when IP address obtained
- **FR-003**: System MUST emit `NET_DOWN` event when connection lost
- **FR-004**: System MUST automatically attempt reconnection on disconnect
- **FR-005**: System MUST use exponential backoff for reconnection (1s → 2s → 4s → ... → 60s max)
- **FR-006**: System MUST persist Wi-Fi credentials in NVS
- **FR-007**: System MUST expose `wifi status` shell command showing SSID, IP, RSSI, state
- **FR-008**: System MUST expose `wifi set <ssid> <password>` shell command for provisioning
- **FR-009**: Wi-Fi callbacks MUST enqueue events and return immediately (constitution constraint)
- **FR-010**: System MUST log connection state changes at INFO level

### Key Entities

- **wifi_state_t**: Connection state (DISCONNECTED, CONNECTING, CONNECTED, FAILED)
- **wifi_config_t**: SSID and password (persisted in NVS)
- **NET_UP event**: Indicates network ready, includes IP address
- **NET_DOWN event**: Indicates network lost, includes reason code

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Wi-Fi connects within 10 seconds of boot (given valid credentials and reachable AP)
- **SC-002**: Automatic reconnection succeeds within 30 seconds of AP restoration
- **SC-003**: MQTT connection established within 5 seconds of Wi-Fi up
- **SC-004**: Shell `wifi status` responds within 100ms
- **SC-005**: No Wi-Fi-related crashes or stack overflows under normal operation
- **SC-006**: System recovers from 10 consecutive disconnect/reconnect cycles without degradation

## Assumptions

- Home network uses WPA2-PSK or WPA3-Personal (no enterprise auth in v0)
- DHCP is available on the network
- Broker IP/hostname is resolvable (DNS available or static IP configured)
- Wi-Fi credentials are known and can be hardcoded initially

## Dependencies

- ESP-IDF Wi-Fi driver (included in ESP-IDF)
- Event bus (`os_event.h`)
- Persistence service (`os_persist.h`) for credential storage
- Shell service (`os_shell.h`) for commands

## Out of Scope

- Wi-Fi AP mode (for initial provisioning)
- WPS provisioning
- Captive portal provisioning
- Enterprise authentication (802.1X)
- Static IP configuration (DHCP only in v0)
