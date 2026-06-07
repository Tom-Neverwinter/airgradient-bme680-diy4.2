# AirGradient DIY Pro V4.2 (ESP8266) — BME680 + reliability fork

A customized fork of the official [AirGradient Arduino library](https://github.com/airgradienthq/arduino),
targeting a **DIY Pro V4.2** PCB with a **WeMos D1 Mini / D1 Mini Pro (ESP8266)**.
It builds on the stock `DiyProIndoorV4_2` firmware and adds:

- 🌡️ **BME680 barometric pressure** (optional add-on) exposed on the local API.
- 🔁 **Self-healing I²C recovery** for the SHT (temperature/humidity) and SGP41
  (TVOC/NOx) sensors — fixes the intermittent dropout where a reading vanishes
  until reboot.
- 🪞 **OLED 180° rotation** for the V4.2 panel that's mounted upside-down.

Most of the AirGradient library remains upstream code. A few targeted ESP8266
support files are changed for HTTP/MQTT timeouts, configuration callback safety,
and SHT/SGP retry cleanup. This fork tracks upstream at commit `d7529cc` (post
`feat/sps30-ooa`).

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
| OLED rotation | [`AirGradient_DIYPro_WeMosD1MiniPro/`](AirGradient_DIYPro_WeMosD1MiniPro/) (`oledRotate180`) | 180° flip via raw SH1106 I²C commands, from the sketch |

Additional current changes:
- `src/Sht/Sht.cpp` and `src/Sgp41/Sgp41.cpp` clean up failed init attempts so
  retry loops do not consume heap.
- The sketch keeps SHT and SGP41 registered as expected DIY Pro V4.2 hardware,
  while separate `shtReady` / `sgpReady` flags guard driver calls.
- COM13 diagnostics showed a boot-time I2C failure path where SGP41 failed init,
  SHT was then not found, and the web/API output treated those sensors as absent.
  The current build retries missing I2C drivers once per minute and logs I2C
  scans so addresses `0x44` and `0x59` can be verified.

### 1. BME680 barometric pressure (optional)
The V4.2 PCB has no BME680 footprint, so it's a hand-wired extra on the shared I²C
bus (auto-detected at 0x76/0x77). It uses the **SV-Zanshin "BME680"** library — the
Adafruit driver is avoided because it pulls in a second `Adafruit_BusIO` that
collides with the copy this library already vendors.

**The AirGradient cloud dashboard cannot display pressure** (fixed schema), so it's
published only on the device's local endpoints:
- `GET /metrics` → `airgradient_pressure_hpa` (OpenMetrics / Prometheus / Grafana)
- `GET /measures/current` → `"pressure"` field (hPa) in the JSON

### 2. I2C sensor diagnostics and recovery (SHT + SGP41)
The stock firmware marks a failed I²C read invalid and never recovers if the shared
bus latches up — the reading stays gone until a reboot. Both sensor paths now retry,
then clear a wedged bus (clock SCL up to 9× to free SDA + STOP) and re-initialize.
SGP41 uses a generous ~30 s threshold so normal startup conditioning isn't mistaken
for a fault.

The firmware now separates board hardware presence from driver readiness:
`hasSensorSHT` / `hasSensorSGP` stay true for this DIY Pro V4.2 build, while
`shtReady` / `sgpReady` decide whether the code may call the driver.

### 3. OLED 180° rotation (V4.2)
This V4.2 panel is mounted rotated, so it reads upside-down / mirrored with the stock
library. The library's U8g2 instance is private with no rotation API, so the sketch's
`oledRotate180()` flips it 180° right after `begin()` by sending the SH1106 a reversed
segment re-map (`0xA0`) and COM scan direction (`0xC0`) over I²C — the AirGradient
library itself is left **stock**.

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
