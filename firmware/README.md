# Firmware

`PlantowerAirGradientPortal` is AirGradient DIY BASIC, trimmed to a
NodeMCU + PMS5003, with a local Readings / Network / Help site.

Humidity and temperature come from an optional DHT22 / AM2302 on D7
(GPIO13). If that chip is missing or fails, the board falls back to
Open-Meteo for the location saved on the Network tab.

`library-patches/` overlays five files onto AirGradient Arduino **3.7.0**:

| File | Why |
|---|---|
| `AirGradient.cpp` | Optional device-id override (not used in the public `.bin`) |
| `AgConfigure.cpp` / `.h` | Local upload / PM / duty-cycle settings |
| `AgOledDisplay.cpp` | Skip OLED when `PLANTOWER_AIRGRADIENT_NO_DISPLAY` |
| `AgWiFiConnector.cpp` | Setup hotspot password `cleanair` |

Public builds must **not** define `PLANTOWER_AIRGRADIENT_DEVICE_ID_OVERRIDE`
or bake in Wi-Fi credentials.

`./scripts/flash.sh` downloads the 3.7.0 tag, applies these patches, and
compiles with `-DPLANTOWER_AIRGRADIENT_NO_DISPLAY`.
