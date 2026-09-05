# Wiring

Power the PMS5003 from 5 V. Talk to it on AirGradient's DIY BASIC pins, not the USB serial pins. If you add a DHT22 / AM2302, power that chip from **3.3 V**, not 5 V.

## PMS5003 (required)

| PMS5003 | NodeMCU / D1 mini | GPIO |
|---|---|---|
| VCC | `VIN`, `VU`, `VBUS`, or `5V` | 5 V from USB |
| GND | `GND` | GND |
| TX | `D5` | GPIO14 |
| RX | `D6` | GPIO12 |

```text
PMS5003 VCC ---- 5V / VBUS
PMS5003 GND ---- GND
PMS5003 TX  ---- D5 (GPIO14)
PMS5003 RX  ---- D6 (GPIO12)
```

Leave the board's hardware `RX`/`TX` pins free. USB flashing and serial logs use those.

## DHT22 / AM2302 (optional)

Many boards label the three pins `+`, `OUT`, and `-`.

| DHT22 | NodeMCU / D1 mini | Notes |
|---|---|---|
| `+` / VCC | `3V3` | 3.3 V only. Do not use VBUS / 5 V. |
| `OUT` / DATA | `D7` | GPIO13. Module usually has an onboard pull-up. |
| `-` / GND | `GND` | Same ground as the PMS5003. |

```text
DHT22 +   ---- 3V3
DHT22 OUT ---- D7 (GPIO13)
DHT22 -   ---- GND
```

The firmware bit-bangs the DHT22 on D7. If the chip is missing, unplugged, or fails a few reads, humidity and temperature fall back to Open-Meteo for the location saved on the Network tab.

## If PM stays at zero

Swap only TX and RX between D5 and D6. A reversed pair is the usual cause of a silent Plantower.

## Do not

- Power the PMS5003 from the 3.3 V pin
- Power the DHT22 from 5 V / VBUS
- Use D1/D2 (those were airRohr pins, not AirGradient)
- Run the sensors sealed in a box with no airflow

## Placement

- Roughly breathing height, open air
- Away from walls, corners, rugs, AC outlets, and direct drafts
- Outdoor boxes need a downward or sheltered inlet so rain does not hit the fan
