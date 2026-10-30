# ESS response recovery

## Design refresh using OpenDesign / Baseline UI

Use the actual upstream baseline-ui skill, loaded project-locally under
.pio/design-skills, from OpenDesign's ui-skills catalog. Existing plain HTML/CSS,
native controls and theme tokens remain the foundation (offline ESP32 UI).
Audit: oversized centered tiles waste vertical space, repeated accent headings
and outlines compete with faults, diagnosis block is oversized, form spacing
varies. Direction: quiet dark instrument panel, single theme accent, tabular
left-aligned metrics, six columns on wide screens, compact status with readable
warnings, grouped daily energy, consistent settings and focus states. Preserve
all APIs, IDs, controls, three themes and full-width layout. No animation or CDN.
Verify actual browser screenshots at desktop/mobile plus regression tests and
both firmware builds. Do not install/deploy to the physical device.

## History repair and latest values

Live history reports FormatError, 0 written, 0 restored, 1234 RAM points.
Arduino VFS size() uses stat while fwrite remains buffered: initial header can
be overwritten by padding starting at stale size 0. Reproduced with buffered
native stdio. Use explicit header length, flush and reopen verification.
Preserve invalid owned history files under one of three unused backup names;
salvage only CRC-valid recent records into a newly initialized ring. Never
format an unknown filesystem or overwrite a backup. Expose recovered count.
Show latest valid minute-average per chart below its canvas with units and its
own timestamp; missing fields remain unknown. Save current RAM JSON locally
before any later device firmware update. Device repair is pending installation.

## Persistent daily restart budget

Replace per-boot latch with two reserved attempts per Berlin calendar day and
1800 s between attempts, including across midnight/reboots. Reserve the attempt
in a single NVS blob and verify it before any OFF command. Count aborted cycles.
No valid clock, backwards clock/date, malformed storage or failed persistence:
no new cycle. Do not erase/replenish budget when automation is toggled. Display
daily count, cooldown and clock/storage problems through API and dashboard.

## One-shot menu restart

User confirmed the web menu OFF/ON resolves the failure. After 120 seconds of
fresh permitted >=200 W demand with <50 W actual output, allow one OFF/ON
cycle per LILYGO boot. Keep the used latch across auto toggles. Require configured
voltage cutoff, raw voltage above cutoff+resume delta, no low-voltage lock,
fresh battery/mode/evcc data and no alarm LEDs. The DC permission may be absent
(the reported fault) but is never forged or written. Confirm OFF, wait 10 s,
request ON, then confirm mode plus post-ON measurements before ramping.
Missing data, alarms, changed demand or manual commands abort. No recursive
restart and no settings/protection modification. Test state transitions natively.

## Discharge mode enable

Victron ESS mode 2/3 documentation separates switch ON from the watt setpoint.
Use the existing, manually tested setSwitchState(ON), never inverter-only.
On entering a permitted discharge episode, queue zero and ON, then ramp on
subsequent fresh snapshots. Allow charger-only to recover; never wake an OFF
device through this path. Existing automatic low-voltage PV wake stays separate.
If power remains absent, existing 30-second/60-second/3-retry recovery also
reasserts ON. Manual mode commands already disable automation. Keep DC/voltage,
freshness and external balance gates; do not change ESS assistant flags.

## Internal history storage

Replace the SD adapter with LittleFS on the existing min_spiffs data partition
(0x20000 = 128 KiB); keep both OTA partitions unchanged. Store a 1440-slot,
28,820-byte CRC ring with minute averages, flush batches every five minutes in
a background task. No SD/SPI access remains in the application. Mount without
formatting; initialize only a verified fully erased partition. Existing unknown
filesystems remain untouched. Restore only valid records within the last 24 h
after NTP. Validate ring/replay/UI and both builds; actual flash persistence
requires a later device test.

Analysis: enabling automation disables virtual setpoint mode, clears the input
snapshot, and sends zero before ramping from fresh evcc data. It does not send
an inverter restart or an additional ESS enable command. ESS power is already
refreshed periodically. The published effective value is a queued command, not
confirmation that the inverter accepted or delivered it.

Implementation: detect sustained missing discharge with fresh local/external
data and all existing permissions intact. Reproduce only the zero-setpoint and
virtual-mode reset, then resume normal ramping. Limit retries and expose them
in the automation API. Never switch the device on/off or bypass voltage/BMS
permissions. Validate the timing policy natively and compile both firmware
variants; physical recovery remains unverified until installed.
