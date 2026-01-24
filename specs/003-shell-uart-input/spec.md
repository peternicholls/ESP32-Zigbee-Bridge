# Feature Specification: Shell UART Input

**Feature Branch**: `003-shell-uart-input`  
**Created**: 2026-01-20  
**Status**: Draft  
**Input**: Verify and fix shell input over UART to enable interactive commands for device management and debugging

## Overview

The shell currently outputs to UART (visible in serial monitor) but input may not be working on ESP32-C6 hardware. This feature verifies and fixes UART input handling so developers can interactively use shell commands (`help`, `ps`, `devices`, `wifi`, `zb permit-join`) for debugging and device management. The shell is critical for development workflow and on-device diagnostics.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Type Commands in Serial Monitor (Priority: P1)

As a developer, I want to type commands in the serial monitor and see them executed so that I can debug the device.

**Why this priority**: Without interactive shell, development and debugging is severely limited.

**Independent Test**: Open serial monitor, type `help`, verify command list appears.

**Acceptance Scenarios**:

1. **Given** device is running, **When** user types `help` and presses Enter, **Then** help text displays
2. **Given** shell prompt `>` displayed, **When** user types characters, **Then** characters echo back
3. **Given** command entered, **When** Enter pressed, **Then** command executes and new prompt appears

---

### User Story 2 - Execute Device Commands (Priority: P1)

As a developer, I want to run `ps` and `devices` commands so that I can see system state.

**Why this priority**: These are the primary diagnostic commands needed for development.

**Independent Test**: Type `ps`, verify task list with names and states appears.

**Acceptance Scenarios**:

1. **Given** shell active, **When** `ps` entered, **Then** shows task list with ID, name, state, stack
2. **Given** shell active, **When** `uptime` entered, **Then** shows system uptime
3. **Given** shell active, **When** `devices` entered, **Then** shows registered Zigbee devices
4. **Given** shell active, **When** `stats` entered, **Then** shows event bus statistics

---

### User Story 3 - Command Line Editing (Priority: P2)

As a developer, I want basic line editing (backspace, clear line) so that I can fix typos without restarting.

**Why this priority**: Quality of life improvement; not blocking basic functionality.

**Independent Test**: Type partial command, press backspace, verify character deleted.

**Acceptance Scenarios**:

1. **Given** partial command typed, **When** backspace pressed, **Then** last character deleted
2. **Given** command typed, **When** Ctrl+C pressed, **Then** current line cancelled, new prompt
3. **Given** command typed, **When** Ctrl+U pressed, **Then** entire line cleared

---

### User Story 4 - Unknown Command Handling (Priority: P2)

As a developer, I want clear feedback for unknown commands so that I know what went wrong.

**Why this priority**: Good UX prevents confusion during debugging.

**Independent Test**: Type `foobar`, verify "Unknown command" message.

**Acceptance Scenarios**:

1. **Given** shell active, **When** unknown command entered, **Then** "Unknown command: foobar" displayed
2. **Given** unknown command, **When** prompt returns, **Then** shell remains functional
3. **Given** empty input (just Enter), **When** Enter pressed, **Then** new prompt, no error

---

### Edge Cases

- What happens when input buffer overflows? → Truncate at max length (128 chars), log warning
- What happens when UART RX interrupt fires during command execution? → Buffer characters, process after command completes
- What happens when multiple rapid commands entered? → Process sequentially, no interleaving
- What happens when shell task starved? → Other tasks continue; shell may lag but recovers

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Shell MUST read input from UART RX
- **FR-002**: Shell MUST echo typed characters back to UART TX
- **FR-003**: Shell MUST execute command when Enter (CR or LF) received
- **FR-004**: Shell MUST support backspace (0x7F or 0x08) for character deletion
- **FR-005**: Shell MUST display prompt (`> `) after each command completes
- **FR-006**: Shell MUST handle unknown commands gracefully with error message
- **FR-007**: Shell MUST NOT block other fibres while waiting for input
- **FR-008**: Shell input buffer MUST be at least 128 characters
- **FR-009**: Shell MUST support Ctrl+C to cancel current input line
- **FR-010**: Shell MUST work with standard serial monitors (115200 baud, 8N1)

### Key Entities

- **Shell command**: Name + handler function + help text
- **Input buffer**: Character array holding current line being typed
- **Command table**: Registered commands available to execute

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `help` command responds within 100ms of Enter pressed
- **SC-002**: Character echo latency under 10ms
- **SC-003**: Shell remains responsive while other tasks run (blink continues during typing)
- **SC-004**: All registered commands (`help`, `ps`, `uptime`, `devices`, `stats`) execute successfully
- **SC-005**: No characters lost when typing at normal human speed (<10 chars/second)
- **SC-006**: Shell recovers from Ctrl+C without requiring reboot

## Assumptions

- UART0 is available for console (not used by other peripherals)
- Serial monitor configured for 115200 baud, 8N1
- USB-serial adapter on dev board handles flow control
- ESP-IDF console UART configured in sdkconfig

## Dependencies

- UART driver (ESP-IDF or custom `os_console.c`)
- Shell service (`os_shell.h`, `os_shell.c`)
- Fibre scheduler for non-blocking input

## Out of Scope

- Command history (up/down arrows)
- Tab completion
- ANSI color codes
- Multiple simultaneous console connections
- Telnet/SSH remote shell
