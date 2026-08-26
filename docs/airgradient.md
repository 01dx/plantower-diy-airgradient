# AirGradient dashboard, calibration, and sharing

The board uploads raw PMS5003 readings plus Open-Meteo temperature (`atmp`) and humidity (`rhum`) to AirGradient. Corrections, the public map, and OpenAQ sharing are configured in the dashboard, not in this firmware.

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
| `atmp`, `rhum` | Open-Meteo for the saved lat/lon, until a DHT22 is fitted |
| `firmware` | AirGradient firmware string |

The local Readings page also shows those raw PM values. It does not replace the AirGradient map.

## PM2.5 calibration

AirGradient can apply a Plantower batch SLR (scaling) and the US EPA 2021 humidity correction.

### 1. Batch / SLR correction

Read the small print on the PMS5003 metal case. A label like `PMS5003-20250530…` maps to AirGradient preset **`PMS5003_20250530`**.

In Location settings → PM2.5 correction, pick that batch if it matches. For the 2025-05-30 Plantower batch AirGradient used:

- Algorithm: PM-count SLR (`slr_5003_20250530`)
- Scaling factor: `0.02411`
- Offset / intercept: `0`

If your batch is different, use the matching AirGradient preset. Do not invent a scale.

This SLR can be on even before humidity is available.

### 2. EPA 2021 correction (needs humidity)

EPA correction uses relative humidity. Enable **`useEpa2021`** only after the board is uploading real `rhum` (Open-Meteo location saved, or a future onboard sensor).

Until humidity is present, leave EPA off. A fake indoor 70% value will skew outdoor numbers.

In humid climates the EPA step often lowers reported PM2.5 a lot. Example at 70% RH:

- Raw 35 µg/m³ → about 20 µg/m³ corrected
- Raw 20 µg/m³ → about 10 µg/m³ corrected

### 3. Temperature / humidity correction

Set temperature and humidity correction to **none**.

Do **not** enable `ag_pms5003t_2024` or other PMS5003T formulas. Those assume a Plantower humidity chip this build does not have. Applying them to Open-Meteo RH can clamp humidity to 100%.

## Where corrected values appear

AirGradient's own surfaces do not all show the same number:

| Surface | Typical value |
|---|---|
| Device Current Data / world API | Raw upload (`pm02`) |
| AirGradient Map | EPA-corrected when EPA is enabled |
| This board's local page | Raw PMS5003 |

That split is expected. Compare local raw vs the map after EPA is on.

## Public map and OpenAQ

With `placeOpen: true` and a location set, AirGradient can publish the station on their map and onward to OpenAQ. Give it time after the first successful POSTs. Sharing, tokens, and the public API live in the [AirGradient docs](https://www.airgradient.com/documentation/kb/where-do-i-access-the-api-documentation-api-token-and-local-api).

World API example:

```text
https://api.airgradient.com/public/api/v1/world/locations/<location-id>/measures/current
```

## Local vs cloud

Keep using AirGradient for calibration, map, and sharing. The board's Network tab is only Wi-Fi, Open-Meteo location, and how often the Plantower runs.
