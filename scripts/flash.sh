#!/usr/bin/env bash
set -euo pipefail

export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

root="$(cd "$(dirname "$0")/.." && pwd)"
sketch="$root/firmware/PlantowerAirGradientPortal"
build="$root/build/PlantowerAirGradientPortal"
vendor="$root/.arduino"
ag_root="$vendor/official"
ag_lib="$ag_root/arduino-master"
ag_version="${AIRGRADIENT_ARDUINO_VERSION:-3.7.0}"
ag_url="https://github.com/airgradienthq/arduino/archive/refs/tags/${ag_version}.tar.gz"

arduino_cli="$(command -v arduino-cli || true)"
if [[ -z "$arduino_cli" ]]; then
  echo "arduino-cli not found. Install it from https://arduino.github.io/arduino-cli/"
  exit 1
fi

mkdir -p "$vendor/data" "$vendor/user" "$vendor/staging"
config="$vendor/arduino-cli.yaml"
cat > "$config" <<EOF
board_manager:
  additional_urls:
    - https://arduino.esp8266.com/stable/package_esp8266com_index.json
directories:
  data: $vendor/data
  downloads: $vendor/staging
  user: $vendor/user
EOF

if ! "$arduino_cli" --config-file "$config" core list 2>/dev/null | grep -q 'esp8266:esp8266'; then
  echo "Installing ESP8266 Arduino core 3.1.2 (once)..."
  "$arduino_cli" --config-file "$config" core update-index
  "$arduino_cli" --config-file "$config" core install esp8266:esp8266@3.1.2
fi

if [[ ! -f "$ag_lib/library.properties" ]]; then
  echo "Downloading AirGradient Arduino ${ag_version}..."
  mkdir -p "$ag_root"
  tmpdir="$(mktemp -d)"
  curl -fsSL "$ag_url" | tar -xz -C "$tmpdir"
  rm -rf "$ag_lib"
  mv "$tmpdir/arduino-${ag_version}" "$ag_lib"
  rmdir "$tmpdir" 2>/dev/null || rm -rf "$tmpdir"
fi

echo "Applying library patches..."
cp "$root/firmware/library-patches/AirGradient.cpp" "$ag_lib/src/AirGradient.cpp"
cp "$root/firmware/library-patches/AgConfigure.cpp" "$ag_lib/src/AgConfigure.cpp"
cp "$root/firmware/library-patches/AgConfigure.h" "$ag_lib/src/AgConfigure.h"
cp "$root/firmware/library-patches/AgWiFiConnector.cpp" "$ag_lib/src/AgWiFiConnector.cpp"
cp "$root/firmware/library-patches/AgOledDisplay.cpp" "$ag_lib/src/AgOledDisplay.cpp"

# Compile only unless a port is passed. Do not auto-flash whatever is plugged in.
port="${1:-}"
if [[ -z "$port" ]]; then
  echo "No port argument. Compiling only."
  compile_only=1
else
  compile_only=0
  echo "Flashing via $port"
fi

# Pass the parent of arduino-master so Arduino-CLI finds bundled
# PubSubClient / PrintLog. Naming the library AirGradient and pointing
# --libraries at that folder treats src/ as a library and fails to link.
"$arduino_cli" --config-file "$config" compile \
  --fqbn esp8266:esp8266:d1_mini \
  --libraries "$ag_root" \
  --build-property "compiler.cpp.extra_flags=-DPLANTOWER_AIRGRADIENT_NO_DISPLAY" \
  --build-path "$build" \
  "$sketch"

cp "$build/PlantowerAirGradientPortal.ino.bin" "$root/releases/PlantowerAirGradientPortal.bin"
(
  cd "$root/releases"
  shasum -a 256 PlantowerAirGradientPortal.bin | awk '{print $1"  PlantowerAirGradientPortal.bin"}' > SHA256SUMS
)

if [[ "$compile_only" -eq 1 ]]; then
  echo "Compiled public image:"
  echo "  $root/releases/PlantowerAirGradientPortal.bin"
  exit 0
fi

"$arduino_cli" --config-file "$config" upload \
  --fqbn esp8266:esp8266:d1_mini \
  --port "$port" \
  --input-dir "$build" \
  "$sketch"

echo "Done. Join airgradient-<serial> with password cleanair if the board has no saved Wi-Fi."
