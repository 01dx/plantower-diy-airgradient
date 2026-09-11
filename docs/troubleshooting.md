# Troubleshooting

## Board does not show up on USB

See the Mac USB notes in [flash.md](flash.md). Data cable, USB-A dongle, hub, or another OS.

## Hotspot never appears

- Wait 30–60 seconds after power-up.
- If Wi-Fi was already saved, the hotspot stays off on purpose. Hold **FLASH** (GPIO0) for 3 seconds to forget Wi-Fi.
- After 3 minutes of a failed join, the hotspot comes back by itself.
- Join `airgradient-<serial>` with password `cleanair`.

## Joined the hotspot but no page

Open `http://192.168.4.1` in the phone browser. Captive-portal detection is flaky on some phones.

## Board will not join home Wi-Fi

- ESP8266 is 2.4 GHz only.
- Use WPA2. Some AX-only / WPA3-only guest networks fail.
- Weak signal: the original outdoor spot may be too far. Test next to the router first.
- Password is stored on the board, not in the `.bin`. Reflashing does not fix a wrong password; use the hotspot and enter it again.

## Local page will not open on the laptop

Guest and IoT SSIDs often isolate clients. Cloud upload can still work. Try:

- Phone on the same SSID
- `http://airgradient_<serial>.local/`
- The AirGradient dashboard for live PM
- Setup hotspot if the board is offline

## PM always zero

- Confirm 5 V on PMS5003 VCC, not 3.3 V
- Swap TX/RX on D5/D6
- First boot in duty-cycle mode can miss the sensor; wait one cycle or reboot

## Humidity / temperature stay on Open-Meteo

That is expected until a DHT22 is wired, and also if the chip misses a few reads. Check:

- `+` on 3V3, not 5 V
- `OUT` on D7 (GPIO13)
- `-` on GND
- Readings label should say **DHT22 on D7** when the chip answers
- Serial log: `DHT22 temperature=… humidity=…`

Keep the Network-tab coordinates as fallback so EPA still gets `rhum`.

## AirGradient shows 400 / unknown device

Register the serial in the dashboard first. The serial is the MAC without colons, not a shorter chip id.

## AirGradient has PM but humidity is empty

If there is no DHT22, set latitude/longitude on the Network tab. `0,0` means not set. Open-Meteo needs that point.

## Humidity looks stuck at 100%

Turn off PMS5003T temperature/humidity correction in the dashboard. This hardware is a plain PMS5003, with optional DHT22, not a PMS5003T.

## EPA numbers look much lower than the large local PM2.5

Expected. The large local number is raw Plantower. The smaller **Humidity compensated** line on Readings uses the same PMS5003_20250530 then EPA 2021 pair as the AirGradient map, from the latest values already on the board.

If that smaller line is `--` while PM2.5 and humidity are present, wait for the next Plantower cycle so PM0.3 count is valid, then refresh.

## Changed my router

If the local page still opens, use Network → save the new Wi-Fi. If it does not, wait for the hotspot or hold FLASH 3 seconds. You should not need to reflash.
