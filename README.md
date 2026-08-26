# DIY outdoor PM2.5 monitor for AirGradient

Flash a **NodeMCU ESP8266 + Plantower PMS5003**, join Wi-Fi from your phone, and send outdoor PM readings to [AirGradient](https://www.airgradient.com/). No always-on computer. No Sensor.Community.

This is a small CC BY-SA 4.0 derivative of [AirGradient DIY BASIC](https://github.com/airgradienthq/arduino) (library 3.7.0). Credit AirGradient if you reuse it.

## What you get

- Prebuilt firmware in [`releases/PlantowerAirGradientPortal.bin`](releases/PlantowerAirGradientPortal.bin)
- Phone hotspot setup (`airgradient-<serial>` / `cleanair`) — Wi-Fi is **not** compiled in
- Local site that opens on **Readings**
- Network tab to change Wi-Fi later without reflashing
- Open-Meteo humidity/temperature for EPA correction until you add a DHT22
- Uploads to the AirGradient cloud, map, and (if you enable sharing) OpenAQ

## Quick start

1. Buy the [parts](docs/bom.md) and [wire](docs/wiring.md) the PMS5003 (`5V`, `GND`, `TX→D5`, `RX→D6`).
2. [Flash](docs/flash.md) the `.bin` once over USB.
3. Join the board hotspot and enter home Wi-Fi. Details: [setup](docs/setup.md).
4. On **Network**, save nearby latitude/longitude for Open-Meteo.
5. Register the serial in AirGradient and turn on [calibration / sharing](docs/airgradient.md).

```text
PMS5003  --5V/GND/D5/D6-->  NodeMCU  --Wi-Fi-->  AirGradient
                                   \-> Open-Meteo (RH / temp)
```

## Local pages

| URL | What |
|---|---|
| `/` | Readings (default) |
| `/network` | Wi-Fi, weather location, intervals |
| `/help` | Wiring and recovery |
| `/plantower/settings` | JSON |

After a router change, use the Network tab, or hold **FLASH** for 3 seconds to reopen the hotspot. The hotspot is not left on all the time.

## Hardware

ESP8266 only (NodeMCU or D1 mini). Not ESP32. PMS5003 only — no OLED, CO2, or TVOC in this build.

## License

[CC BY-SA 4.0](LICENSE). Based on AirGradient Arduino. See [NOTICE](NOTICE).

## Docs

- [Parts](docs/bom.md)
- [Wiring](docs/wiring.md)
- [Flash](docs/flash.md)
- [Phone setup](docs/setup.md)
- [AirGradient, EPA, humidity](docs/airgradient.md)
- [Troubleshooting](docs/troubleshooting.md)
