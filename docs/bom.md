# Parts

You can build this with generic parts. Exact brands are not required.

## Required

| Part | Notes | Typical cost |
|---|---|---|
| NodeMCU ESP8266 or Wemos D1 mini | ESP8266, 4 MB flash. USB-C or Micro-USB. Prefer CP2102 or CH9102 on a Mac. | $3–8 |
| Plantower PMS5003 | Particulate sensor with 2.54 mm breakout / JST adapter | $15–25 |
| USB 5 V supply | Phone charger is fine after flashing. Sensor needs 5 V. | — |
| USB data cable | Charge-only cables will not flash | — |
| Dupont jumper wires | 4 wires: 5 V, GND, TX, RX | $1 |

## Useful, not required

| Part | Why |
|---|---|
| USB-C to USB-A dongle | Cheap USB-C NodeMCU boards with a CH340 chip often fail to appear on Mac USB-C ports. A dongle or USB hub usually fixes it. |
| Weatherproof box with airflow | Outdoor use. Do not seal the PMS5003. Keep the inlet open and dry. |
| USB extension | Puts the board antenna away from metal, USB 3 hubs, and walls. |

## Do not buy for this firmware

- ESP32 / ESP32-C3 boards (AirGradient Open Air firmware is a different stack)
- PMS5003T (that module has its own humidity chip; this project uses a plain PMS5003)
- OLED, CO2, or TVOC modules (supported by stock AirGradient BASIC, unused here)

## Humidity later

A DHT22 / AM2302 can replace Open-Meteo later. Until then the board fetches nearby temperature and relative humidity from Open-Meteo so AirGradient can apply EPA correction.

## What we used

This recipe was proven on:

- Generic NodeMCU ESP8266 V3-style USB-C board (CH340)
- Plantower PMS5003 (batch label `PMS5003-20250530…`)
- USB 5 V from a phone charger after the first flash
