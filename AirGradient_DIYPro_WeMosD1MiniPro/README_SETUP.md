# AirGradient DIY Pro V4.2 — WeMos D1 Mini Pro (ESP8266) Setup

Firmware: **AirGradient 3.6.6** `DiyProIndoorV4_2` example, copied here for the
**WeMos D1 Mini Pro V3.0 (ESP8266, 16MB)**.

Sketch: `AirGradient_DIYPro_WeMosD1MiniPro.ino` (+ `LocalServer.*`, `OpenMetrics.*`).
Board type is fixed in code: `AirGradient ag(DIY_PRO_INDOOR_V4_2)`.

> **Board note:** The D1 Mini **Pro** uses the same ESP8266 and pinout as the standard D1 Mini, so it is
> a drop-in MCU for the DIY Pro V4.2 PCB. The only differences are the external-antenna footprint and
> 16MB flash — neither affects this firmware.

## Arduino IDE setup
1. **File > Preferences** → Additional Boards Manager URLs, add:
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. **Boards Manager** → search `esp8266` → install **ESP8266 by ESP8266 Community**.
3. **Tools > Board > esp8266 > LOLIN(WeMos) D1 mini Pro**
   (or "LOLIN(Wemos) D1 R2 & mini" — both work).
4. Tools: Flash Size **16MB (FS:2MB OTA:~6MB)**, Upload speed 921600 (drop to 115200 if uploads fail).
5. Install the AirGradient library: *Sketch > Include Library > Add .ZIP Library…* →
   `f:\Project\air sensor\arduino-3.6.6.zip`. All dependencies are vendored inside the library.

## Sensors (DIY Pro V4.2 PCB)
| Sensor | Function | Bus |
| :--- | :--- | :--- |
| SenseAir S8 | CO2 | Hardware Serial |
| PMS5003 | PM2.5 | Hardware Serial |
| SHT3x | Temp & Humidity | I2C |
| SGP41 | TVOC & NOx | I2C |

## Flash & first run
- Connect USB, select Port, Upload.
- First boot with no WiFi ⇒ hotspot **`airgradient`**; connect, enter WiFi + optional dashboard token.

## Local change: I2C sensor dropout recovery (SHT + SGP41)
The stock 3.6.6 example marks a failed read as invalid and never recovers if the
shared I2C bus latches up (SHT3x, SGP41 and OLED are all on it) — the affected
reading then stays gone until a reboot. The `.ino` has been hardened (sketch-only,
no library change needed):
- **SHT** (`tempHumUpdate`): retries a transient bad read up to
  `SHT_READ_MAX_RETRY` times; after `SHT_FAILS_BEFORE_REINIT` consecutive failed
  cycles it clears a stuck bus (clocks SCL up to 9× to free SDA, issues a STOP)
  and re-inits the sensor.
- **SGP41** (`updateTvoc`): after `SGP_FAILS_BEFORE_REINIT` cycles (~30 s) of dead
  TVOC/NOx it runs the same bus recovery and re-inits. Threshold is generous so
  the normal ~10 s startup conditioning is never mistaken for a fault. Note:
  re-init resets the SGP41 gas-index learning baseline.

Watch the serial monitor (115200) for `SHT read failed (N consecutive)` /
`Recovering SHT: ...` / `Recovering SGP41: ...` to confirm it self-heals. If
recovery messages appear constantly, the dropout is a physical wiring/connector
fault (see `firmware.txt` Option A), not something firmware can fully mask.

## Optional add-on: BME680 barometric pressure
The DIY Pro V4.2 PCB has no BME680 spot, so this is a hand-wired extra on the
shared I2C bus (address 0x76 or 0x77, auto-detected — no conflict with SHT 0x44,
SGP41 0x59, OLED 0x3C):

| BME680 | → ESP8266 / V4.2 |
| :--- | :--- |
| SDA | GPIO4 (D2) |
| SCL | GPIO5 (D1) |
| VCC | 3V3 |
| GND | GND |

- Library: **"BME680" by SV-Zanshin** (Library Manager). The Adafruit driver is
  deliberately *not* used — it bundles a second `Adafruit_BusIO` that collides
  with the copy the AirGradient library vendors (multiple-definition link error).
- Fully optional: if no BME680 answers at boot, the firmware runs exactly as the
  stock build. Look for `BME680 found` / `BME680 not found` on serial.
- **The AirGradient cloud dashboard cannot show pressure** (fixed schema). Pressure
  is exposed only on the device's local endpoints:
  - `http://<device-ip-or-airgradient_<serial>.local>/metrics` →
    `airgradient_pressure_hpa` (OpenMetrics / Prometheus / Grafana)
  - `http://<device>/measures/current` → `"pressure"` field (hPa) in the JSON
- Temp/humidity/gas from the BME680 are also read and printed to serial each cycle,
  but not published (temp/hum would duplicate the SHT; gas unit left unpublished).

## Sketch change: OLED 180° rotation (readability fix)
This V4.2 panel is mounted rotated, so with the stock firmware the screen reads
upside-down / mirrored. The AirGradient library keeps its U8g2 instance private
with no rotation API, so the fix is done **in the sketch** (the library stays
stock): `oledRotate180()` runs right after `oledDisplay.begin()` and sends the
SH1106 a reversed segment re-map (`0xA0`) and COM scan direction (`0xC0`) over
I2C — together a 180° flip.

- U8g2 issues segment-remap/COM-scan only in its init sequence (not on each
  redraw), so this override persists.
- The SH1106's column margin is symmetric (2 px each side of 128 within 132), so
  flipping causes no horizontal shift.
- To tune: in `oledRotate180()` send only `{0xC0}` for a vertical-only flip, or
  only `{0xA0}` for a horizontal-only (left/right) mirror.
