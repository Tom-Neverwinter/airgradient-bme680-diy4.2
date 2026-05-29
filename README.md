# AirGradient DIY Pro V4.2 (ESP8266) — BME680 + reliability fork

A customized fork of the official [AirGradient Arduino library](https://github.com/airgradienthq/arduino),
targeting a **DIY Pro V4.2** PCB with a **WeMos D1 Mini / D1 Mini Pro (ESP8266)**.
It builds on the stock `DiyProIndoorV4_2` firmware and adds:

- 🌡️ **BME680 barometric pressure** (optional add-on) exposed on the local API.
- 🔁 **Self-healing I²C recovery** for the SHT (temperature/humidity) and SGP41
  (TVOC/NOx) sensors — fixes the intermittent dropout where a reading vanishes
  until reboot.
- 🪞 **OLED mirror fix** for the V4.2 SH1106 panel that renders backwards.

Everything else is the upstream AirGradient library, unchanged. This fork tracks
upstream at commit `d7529cc` (post `feat/sps30-ooa`).

## The firmware lives here

➡️ **[`AirGradient_DIYPro_WeMosD1MiniPro/`](AirGradient_DIYPro_WeMosD1MiniPro/)**

That folder holds the modified sketch (`.ino`, `LocalServer.*`, `OpenMetrics.*`).
**Build & flash instructions, wiring, and full details are in its
[`README_SETUP.md`](AirGradient_DIYPro_WeMosD1MiniPro/README_SETUP.md).**

## What's different from upstream

| Change | Where | Summary |
| :--- | :--- | :--- |
| BME680 pressure | `AirGradient_DIYPro_WeMosD1MiniPro/*.ino`, `OpenMetrics.cpp`, `LocalServer.cpp` | Optional, auto-detected; no-op if absent |
| SHT recovery | `AirGradient_DIYPro_WeMosD1MiniPro/*.ino` (`tempHumUpdate`) | Retry → bus clear → re-init |
| SGP41 recovery | `AirGradient_DIYPro_WeMosD1MiniPro/*.ino` (`updateTvoc`) | Bus clear → re-init after ~30 s dead |
| OLED mirror | [`src/AgOledDisplay.cpp`](src/AgOledDisplay.cpp) (`begin()`) | `U8G2_MIRROR` for `isPro4_2()` only |

### 1. BME680 barometric pressure (optional)
The V4.2 PCB has no BME680 footprint, so it's a hand-wired extra on the shared I²C
bus (auto-detected at 0x76/0x77). It uses the **SV-Zanshin "BME680"** library — the
Adafruit driver is avoided because it pulls in a second `Adafruit_BusIO` that
collides with the copy this library already vendors.

**The AirGradient cloud dashboard cannot display pressure** (fixed schema), so it's
published only on the device's local endpoints:
- `GET /metrics` → `airgradient_pressure_hpa` (OpenMetrics / Prometheus / Grafana)
- `GET /measures/current` → `"pressure"` field (hPa) in the JSON

### 2. I²C sensor dropout recovery (SHT + SGP41)
The stock firmware marks a failed I²C read invalid and never recovers if the shared
bus latches up — the reading stays gone until a reboot. Both sensor paths now retry,
then clear a wedged bus (clock SCL up to 9× to free SDA + STOP) and re-initialize.
SGP41 uses a generous ~30 s threshold so normal startup conditioning isn't mistaken
for a fault.

### 3. OLED mirror fix (V4.2 only)
This V4.2 SH1106 panel renders horizontally mirrored with stock `U8G2_R0`. `begin()`
now uses `U8G2_MIRROR` for `isPro4_2()` only (ONE / DIY Pro 3.3 stay `U8G2_R0`).
U8g2 also corrects the SH1106 column offset per orientation, which raw flip commands
would not.

## Hardware (DIY Pro V4.2)

| Sensor | Function | Bus |
| :--- | :--- | :--- |
| SenseAir S8 | CO₂ | Hardware Serial |
| Plantower PMS5003 | PM2.5 | Hardware Serial |
| Sensirion SHT3x | Temp & Humidity | I²C |
| Sensirion SGP41 | TVOC & NOx | I²C |
| Bosch BME680 *(optional, added)* | Pressure | I²C |

## Credits, upstream & license

This is a derivative of the **AirGradient Arduino library**
(<https://github.com/airgradienthq/arduino>) — see their
[compilation](docs/howto-compile.md), [local server API](docs/local-server.md),
and [OTA](docs/ota-updates.md) docs, and the [AirGradient forum](https://forum.airgradient.com/).

Licensed under **CC BY-SA 4.0** (Attribution-ShareAlike 4.0 International), same as
upstream. Modifications in this fork are released under the same license.
