# Hardcore Henry Drive

GTA1-style top-down autonomous "wander" demo for an ESP32-S3 dev board with an Elecrow ES3C35P (ST77922) display. The car drives itself through an endless procedurally-generated city while NPC traffic is spawned from nearby BLE and Wi‑Fi radios so the world mirrors the street.

## Features
- Procedural city: roads, buildings, parks, parking lots, and intersection-aware dashed lanes.
- Self-driving player vehicle (pure pursuit + wander) with 3 vehicle types: FIT, RV, PICKUP.
- NPC traffic spawned from nearby BLE/Wi‑Fi devices (BLE = cyan, Wi‑Fi = amber).
- Pre-rendered 8-direction sprites, brake/headlight glow, shadows, HUD and minimap.
- Trip odometer persisted to NVS (Preferences).
- Simple controls: tap boot button to change vehicle, hold boot button to pause/resume.

## Hardware
- ESP32‑S3 Dev Module (tested on Elecrow ES3C35P)
- ST77922 SPI/QSPI LCD used by Elecrow board
- Boot button (PIN_BOOT = 0)
- LCD backlight pin configured as 41 in code (adjust if your board differs)

Recommended board config (used during development):
- Flash size: 8MB
- PSRAM: OPI PSRAM enabled
- Partition scheme: "8M with spiffs (3MB APP / 1.5MB SPIFFS)"

The sketch requires PSRAM for the framebuffer and pre-rendered sprites; it will halt with a message if PSRAM is not available.

## Wiring / Pins (as used in topdrive.ino)
- LCD QSPI pins: CS=10, SCK=12, D0=11, D1=13, D2=14, D3=9
- LCD BL (backlight): 41
- Boot button: 0
(Adjust pins in the sketch to match your hardware if different.)

## Software / Libraries
- Arduino framework for ESP32‑S3
- esp_display_panel (esp_panel::drivers; for ST77922)
- BLEDevice (BLE)
- WiFi (built-in)
- Preferences (NVS storage)
- Uses ps_malloc / heap_caps to allocate PSRAM buffers

## Build & Flash
1. Set up the Arduino IDE or PlatformIO for ESP32‑S3.
2. Ensure your board profile enables PSRAM and uses an 8MB flash partition layout (see "Partition Scheme").
3. Install the required libraries (esp_display_panel or the platform package that provides it).
4. Compile and flash topdrive.ino to the ESP32‑S3.
5. Power on the device and allow the splash screen; PSRAM framebuffer allocation and sprite pre-rendering happen at boot.

## Controls
- TAP BOOT: cycle vehicle (FIT / RV / PICKUP)
- HOLD BOOT (>=600 ms): pause / resume simulation

## Notes & Troubleshooting
- If the sketch logs "no PSRAM framebuffer" at boot, enable OPI PSRAM in your board settings and reflash.
- BLE scanning and Wi‑Fi scanning are used to spawn NPCs. If you run on a network with few devices nearby, fewer NPCs will appear.
- The odometer is saved every minute to NVS under key `trip_mi` in namespace `hhdrive`.

## Suggested README additions
- Add a screenshot or short GIF (useful to show HUD and minimap).
- Add a wiring diagram and a board variant note if you want to support multiple Elecrow/clone boards.
- Add a LICENSE file (MIT / permissive) and attribution if you want to open-source.

## License
(Choose a license; e.g., MIT) — add a LICENSE file to the repo.

## Credits
- Author: mutillix
- Demo: "Hardcore Henry Drive" (topdrive.ino)
