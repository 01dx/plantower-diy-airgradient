# AirGradient dashboard, calibration, and sharing

The board uploads raw PMS5003 readings plus temperature (`atmp`) and humidity (`rhum`) to AirGradient. Those last two come from a DHT22 / AM2302 on D7 when it is answering, otherwise from Open-Meteo for the saved location. Corrections, the public map, and OpenAQ sharing are configured in the dashboard, not in this firmware.

## Register the monitor

1. Create an account at [AirGradient Dashboard](https://app.airgradient.com/).
2. Add a monitor. Use the serial printed on the local **Help** tab and in the setup hotspot name. It is the ESP8266 MAC with colons removed, lowercase.
3. Choose an **outdoor** DIY / Place model. `O-1PST` is a reasonable outdoor placeholder for PMS-only hardware. This is not a stock AirGradient Open Air kit.
4. Set country and a public location if you want the map.
5. Enable public sharing (`placeOpen`) if you want AirGradient Map / OpenAQ.

The device talks to:

```text
GET  http://hw.airgradient.com/sensors/airgradient:<serial>/one/config
POST http://hw.airgradient.com/sensors/airgradient:<serial>/measures
```

A `400` before registration usually means AirGradient does not know the serial yet. After registration, uploads should return `200`. A `429` after rapid test flashes is rate limiting; the default 10 minute upload interval avoids that.

## What the board sends

| Field | Source |
|---|---|
| `pm01`, `pm02`, `pm10` | PMS5003 raw µg/m³ |
| `pm003Count` | PMS5003 particle count |
| `wifi` | RSSI in dBm |
| `atmp`, `rhum` | DHT22 on D7 when it reads; otherwise Open-Meteo for the saved lat/lon |
| `firmware` | AirGradient firmware string |

The local Readings page shows raw PM values as the large numbers, labels the humidity source as **DHT22 on D7** or **API: Open-Meteo**, and puts a smaller humidity-compensated PM2.5 under the hero reading. That smaller line uses the latest humidity and PM0.3 count already on the board (`PMS5003_20250530` then EPA 2021). It does not replace the AirGradient map.

## PM2.5 calibration

AirGradient has **two different humidity-related toggles**. Use the first. Leave the second off.

### 1. Batch / SLR correction

Read the small print on the PMS5003 metal case. A label like `PMS5003-20250530…` maps to AirGradient preset **`PMS5003_20250530`**.

In Location settings → PM2.5 correction, pick that batch if it matches. For the 2025-05-30 Plantower batch AirGradient used:

- Algorithm: PM-count SLR (`slr_5003_20250530`)
- Scaling factor: `0.02411`
- Offset / intercept: `0`

If your batch is different, use the matching AirGradient preset. Do not invent a scale.

This SLR can be on even before humidity is available.

### 2. EPA 2021 PM2.5 correction — turn this on

This **is** the humidity correction in the AirGradient panel (`useEpa2021`). It uses uploaded `rhum` to adjust **PM2.5**, not to rewrite the humidity number.

Enable it once the board is sending real `rhum` (DHT22 answering, or Open-Meteo location saved). Until then, leave EPA off so AirGradient does not invent an indoor 70% RH.

In humid climates the EPA step often lowers reported PM2.5 a lot. Example at 70% RH:

- Raw 35 µg/m³ → about 20 µg/m³ corrected
- Raw 20 µg/m³ → about 10 µg/m³ corrected

### 3. Temperature / humidity *sensor* correction — leave this off

This is a **separate** dashboard setting. It tries to correct the temperature and humidity values themselves, as if they came from a Plantower **PMS5003T** (a different sensor with its own humidity chip).

This build does not have that chip. Humidity comes from a DHT22 or Open-Meteo. Set T/H correction to **none**. Do **not** enable `ag_pms5003t_2024`. Applying that formula to weather-API or DHT22 RH can clamp humidity to 100%, and then EPA uses the wrong RH.

## Where corrected values appear

AirGradient's own surfaces do not all show the same number:

| Surface | Typical value |
|---|---|
| Device Current Data / world API | Raw upload (`pm02`) |
| AirGradient Map | EPA-corrected when EPA is enabled |
| This board's local page | Large PM2.5 is raw. Smaller line is PMS5003_20250530 then EPA 2021 |

That split is expected. Compare local raw vs the map after EPA is on.

## Public map and OpenAQ

With `placeOpen: true` and a location set, AirGradient can publish the station on their map and onward to OpenAQ. Give it time after the first successful POSTs. Sharing, tokens, and the public API live in the [AirGradient docs](https://www.airgradient.com/documentation/kb/where-do-i-access-the-api-documentation-api-token-and-local-api).

World API example:

```text
https://api.airgradient.com/public/api/v1/world/locations/<location-id>/measures/current
```

## Local vs cloud

Keep using AirGradient for calibration, map, and sharing. The board's Network tab is only Wi-Fi, Open-Meteo location, and how often the Plantower runs. Changing home Wi-Fi later is done there, not by reflashing.
