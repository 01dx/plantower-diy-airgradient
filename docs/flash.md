# Flash the firmware

You flash once. Wi-Fi, location, and intervals are stored on the board after that.

Board: **LOLIN(WEMOS) D1 R2 & mini** (`esp8266:esp8266:d1_mini`). This is still an ESP8266, even on USB-C NodeMCU clones.

## Option A — prebuilt image (easiest)

1. Install [esptool](https://github.com/espressif/esptool) (`pip3 install esptool`) or use Arduino-CLI as below.
2. Plug in the board with a **data** USB cable.
3. Find the port:

```zsh
ls /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* 2>/dev/null
```

On Windows look in Device Manager for COM3, COM4, and similar. On Linux look for `/dev/ttyUSB0` or `/dev/ttyACM0`.

4. Flash the image from `releases/PlantowerAirGradientPortal.bin`:

```zsh
python3 -m esptool --chip esp8266 --port /dev/cu.usbserial-XXXX write_flash -fm dio 0x0 releases/PlantowerAirGradientPortal.bin
```

Replace the port with yours. Do not hold FLASH unless the upload fails to connect.

5. Unplug serial tools when the flash finishes. USB after that is only power.

SHA-256 of the published image is in `releases/SHA256SUMS`.

## Option B — Arduino-CLI from source

Needs [Arduino CLI](https://arduino.github.io/arduino-cli/) and the ESP8266 core **3.1.2**.

```zsh
./scripts/flash.sh
```

Pass a port to compile and upload:

```zsh
./scripts/flash.sh /dev/cu.usbserial-XXXX
```

The script downloads AirGradient Arduino **3.7.0**, applies the small library patches in `firmware/library-patches/`, and builds `firmware/PlantowerAirGradientPortal`.

Compile flags used for the public image:

```text
-DPLANTOWER_AIRGRADIENT_NO_DISPLAY
```

There is **no** Wi-Fi password and **no** device-id override in the public binary. Each board uses its own MAC-derived AirGradient serial.

## Mac USB quirk

Cheap USB-C NodeMCU boards often use a CH340 serial chip. On macOS they sometimes do not show up when plugged straight into a USB-C port.

That is a known Mac + cheap-board issue, not a firmware bug.

If `ls /dev/cu.*` shows nothing:

1. Use a data cable, not a charge-only cable.
2. Try a cheap USB-C to USB-A dongle, or a USB-A port on a monitor or dock.
3. A USB hub can also work.
4. Windows or Linux often sees these boards with no extra adapter.

Look for `/dev/cu.usbserial-*` or `/dev/cu.wchusbserial*`. A Wemos D1 mini with CP2102 or CH9102 is less fussy on Macs.

Some USB-C NodeMCU sockets drop power if the plug is pushed fully home. Seat it so the board LED is on.

## After flashing

Join the setup hotspot from your phone. See [setup.md](setup.md).
