#!/bin/bash
# Flash the T-Display-P4 V1.0 ESP32-C6 Wi-Fi coprocessor through its dedicated
# 3.3 V UART connector. This does not flash the main ESP32-P4.
#
# The C6 must run ESP-Hosted slave firmware compatible with the host side in
# third_party/esp_hosted (v2.12.x). Only needed if the C6 no longer runs the
# LilygoBox factory image, e.g. after another project reflashed it.
#
# Put the C6 in download mode first: with the board powered normally, hold the
# coprocessor BOOT button, tap its RESET button, then release BOOT.
#
# Usage: ./scripts/flash-c6.sh <port> [esp32c6-slave.bin]
# Without a firmware argument, the newest esp32c6-v2.12.*.bin shipped with
# PlatformIO's Arduino-ESP32 libs is used (it matches the host's major.minor).

set -euo pipefail

PORT=${1:-}
FIRMWARE=${2:-}

if [[ -z "$PORT" ]]; then
    echo "Usage: $0 <port> [esp32c6-slave.bin]" >&2
    exit 1
fi

if [[ -z "$FIRMWARE" ]]; then
    FIRMWARE=$(find \
        "$HOME/.platformio-p4/packages/framework-arduinoespressif32-libs/hosted" \
        "$HOME/.platformio/packages/framework-arduinoespressif32-libs/hosted" \
        -type f -name 'esp32c6-v2.12.*.bin' 2>/dev/null \
        | awk -F/ '{print $NF "\t" $0}' | sort -V | tail -1 | cut -f2)
fi

if [[ -z "$FIRMWARE" || ! -f "$FIRMWARE" ]]; then
    echo "ESP-Hosted C6 firmware not found." >&2
    echo "Pass the network-adapter image from the LilygoBox releases:" >&2
    echo "  https://github.com/Xinyuan-LilyGO/lilygobox-espidf/releases/latest" >&2
    exit 1
fi

if command -v esptool.py >/dev/null 2>&1; then
    ESPTOOL=(esptool.py)
elif [[ -x "$HOME/.platformio-p4/penv/bin/python" ]] \
    && "$HOME/.platformio-p4/penv/bin/python" -m esptool version >/dev/null 2>&1; then
    ESPTOOL=("$HOME/.platformio-p4/penv/bin/python" -m esptool)
elif [[ -x "$HOME/.platformio/penv/bin/python" ]] \
    && "$HOME/.platformio/penv/bin/python" -m esptool version >/dev/null 2>&1; then
    ESPTOOL=("$HOME/.platformio/penv/bin/python" -m esptool)
elif python3 -m esptool version >/dev/null 2>&1; then
    ESPTOOL=(python3 -m esptool)
else
    echo "esptool is required. Install it with: python3 -m pip install esptool" >&2
    exit 1
fi

echo "Flashing ESP32-C6 coprocessor firmware: $FIRMWARE"
"${ESPTOOL[@]}" --chip esp32c6 --port "$PORT" --baud 921600 \
    --before no_reset --after no_reset \
    write_flash 0x0 "$FIRMWARE"

echo "C6 flash complete. Tap the coprocessor RESET button, then reboot the ESP32-P4."
