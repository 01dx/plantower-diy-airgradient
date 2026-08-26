# First setup

USB does not open a website. The USB chip is only a serial adapter. Setup is always over Wi-Fi.

## 1. Power the board

Flash once, then power from any USB 5 V supply. You do not reflash to change Wi-Fi.

## 2. Join the setup hotspot

If the board has no saved Wi-Fi (or cannot join it), it opens a hotspot:

```text
Name:     airgradient-<serial>
Password: cleanair
```

`<serial>` is the board MAC with colons removed, lowercase, for example `aabbccddeeff`.

On your phone, join that network. Open `http://192.168.4.1` if a page does not appear by itself.

The hotspot is **not** always on. An always-on open AP would let anyone nearby change your settings. It appears on first boot, after a failed join, from **Forget Wi-Fi** on the Network tab, or if you hold the NodeMCU **FLASH** button for 3 seconds.

## 3. Enter home Wi-Fi

Pick your 2.4 GHz network and type the password. The board saves those details in its own flash and reboots onto your home network.

ESP8266 does not join 5 GHz Wi-Fi. Use a 2.4 GHz SSID, or a router IoT network that is 2.4 GHz only.

You do **not** type the password into the firmware. You do **not** reflash for a new router.

## 4. Open the local site

On a phone or computer on the same Wi-Fi:

```text
http://airgradient_<serial>.local/
http://<board-ip>/
```

Home is **Readings**. **Network** is Wi-Fi, weather location, and sensor intervals. **Help** is wiring and recovery.

Some guest / IoT networks isolate clients, so a laptop may not open the local page even while the board still uploads to AirGradient. Use the phone on the same SSID, or wait for the setup hotspot if the board cannot join.

## 5. Set the weather location

Until a humidity chip is wired, the board asks [Open-Meteo](https://open-meteo.com/) for nearby temperature and relative humidity.

On **Network**, enter latitude and longitude for the outdoor sensor. Four decimal places is enough. Get them from:

- The phone's compass / maps app
- [OpenStreetMap](https://www.openstreetmap.org/) (right-click → show address)
- Google Maps (right-click the pin)

Save. The board stores the point and fetches weather about every 10 minutes. That humidity is uploaded to AirGradient as `rhum` so EPA correction can run.

Do not leave 0,0. That means "not set", and humidity will not be sent.

## 6. Register on AirGradient

See [airgradient.md](airgradient.md).
