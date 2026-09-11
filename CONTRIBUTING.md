# Contributing

This is a small CC BY-SA 4.0 fork of AirGradient DIY BASIC for a
NodeMCU + PMS5003 outdoor monitor, with an optional DHT22 / AM2302.

## Ground rules

- Keep Wi-Fi passwords and device-id overrides out of git and out of
  the public `.bin`.
- Do not add a 24/7 open setup hotspot.
- ESP8266 RAM is tight. Prefer server-rendered HTML over a heavy
  front-end.
- Map, sharing, and dashboard calibration settings stay in AirGradient.
  The local Readings page may preview the same PMS5003_20250530 + EPA
  2021 pair from values already on the board; do not add extra API
  calls or bake in a location.
- Credit AirGradient when you redistribute.

## Build

See [docs/flash.md](docs/flash.md). Use `./scripts/flash.sh` without a
port to compile only.

## Patches

`firmware/library-patches/` are the only files we overlay onto
AirGradient Arduino 3.7.0. Keep those diffs small.

## What this project is not

- Not Sensor.Community / airRohr
- Not ESP32 Open Air firmware
- Not a Mac-side Python bridge
