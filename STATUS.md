# STATUS

## Current state
- Date (UTC): 2026-01-15
- Commit: (initial)
- Branch: main
- Last completed milestone: M0
- Next milestone in progress: M1
- Active task IDs: T010
- Known blockers: None

## Verified working
- [ ] Boots reliably
- [ ] Shell responsive over UART
- [ ] Fibres switching (>=3 tasks)
- [ ] Event bus stable under load
- [ ] Logging queue stable (no stalls)
- [ ] Persistence stable across reboot
- [ ] Zigbee coordinator up
- [ ] Permit join works
- [ ] Interview/provisioner works
- [ ] Registry persists and restores
- [ ] Light control end-to-end
- [ ] MQTT connects and stays connected
- [ ] MQTT publishes state updates
- [ ] MQTT commands trigger Zigbee actions

## Progress notes
**Since last update**
- Created project structure (os/, services/, adapters/, apps/, drivers/, docs/, main/)
- Added Context Capsule + architecture overview in docs/README.md
- Added STATUS.md snapshot template

**Today focus**
- Complete M0: Repo + conventions
- Prepare for M1: Tiny OS core

**Next actions**
- Begin M1: Create FreeRTOS host task os_host_task()
- Implement fibre scheduler v0: create/switch/yield/sleep
- Implement tick source: ESP-IDF timer increments os_ticks

## Decisions locked in
- Substrate approach: ESP-IDF/FreeRTOS substrate; kernel-in-a-task for fibres/services
- Capability taxonomy version: caps-v0
- Command semantics: actuators optimistic_with_reconcile; sensors report_driven
- MQTT topic scheme: bridge/<node_id>/<capability>/{state|set|meta}
- Persistence schema version: 1
- Queue overflow policy: drop_oldest_noncritical; log and count drops

## Metrics
- Counters: (not yet measured)
- Resource usage: (not yet measured)

## Open questions
- None at this time

## Blockers and risks
**Blockers**
- None

**Risks**
- R1: Zigbee stack callback context constraints
- R2: Persistence schema evolution
- R3: Device quirks / non-standard attributes
- R4: Cooperative scheduling starvation

## How to resume
Paste: Context Capsule + this STATUS.md + the next task request (e.g., "Implement M1 (Tiny OS core)").
