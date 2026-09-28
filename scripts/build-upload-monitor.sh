#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

ENV_NAME="tdisplay-p4"
# The pioarduino platform lives in a separate core so another project's
# espressif32 resolution cannot replace it.
DEFAULT_PIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
PIO_CORE_DIR="${ZELLO_PIO_CORE_DIR:-${ZELLO_P4_PIO_CORE_DIR:-${DEFAULT_PIO_CORE_DIR}-p4}}"
# ESP-IDF refuses a build directory whose path contains a space, and this
# project sits under "/Volumes/T7 Shield/...", so build outside it.
BUILD_DIR="${ZELLO_BUILD_DIR:-$PIO_CORE_DIR/build/scheff-zello}"
PORT=""
ERASE_FIRST=false
FULLCLEAN=false
JUST_BUILD=false

show_usage() {
	echo "Usage: $0 [--port PORT] [--erase|-E] [--fullclean|-F] [--just-build|-B]"
	echo "  --port PORT   Serial port of the selected device (e.g. /dev/cu.usbmodem1101)."
	echo "                If omitted, PlatformIO auto-detects the port."
	echo "  --erase, -E   Erase flash before build/upload. This wipes NVS, so the"
	echo "                Wi-Fi and Zello settings have to be entered again."
	echo "  --fullclean, -F  Run PlatformIO fullclean before upload"
	echo "  --just-build, -B  Compile only - no upload, no monitor, no device needed."
}

run_pio() {
	PLATFORMIO_CORE_DIR="$PIO_CORE_DIR" PLATFORMIO_BUILD_DIR="$BUILD_DIR" pio "$@"
}

run_pio_target() {
	local target="$1"
	local label="$2"
	local port_args=()
	if [ -n "$PORT" ]; then
		case "$target" in
			monitor) port_args=(--monitor-port "$PORT") ;;
			*)       port_args=(--upload-port "$PORT") ;;
		esac
	fi
	echo "[PIO] $label ($ENV_NAME)..."
	run_pio run -e "$ENV_NAME" -t "$target" ${port_args[@]+"${port_args[@]}"}
}

format_duration() {
	local total_seconds="$1"
	local hours=$((total_seconds / 3600))
	local minutes=$(((total_seconds % 3600) / 60))
	local seconds=$((total_seconds % 60))

	if [ "$hours" -gt 0 ]; then
		printf "%dh %02dm %02ds" "$hours" "$minutes" "$seconds"
	else
		printf "%dm %02ds" "$minutes" "$seconds"
	fi
}

if ! command -v pio >/dev/null 2>&1; then
	echo "PlatformIO CLI not found: install it or run from an environment that provides 'pio'."
	exit 1
fi

while [ $# -gt 0 ]; do
	case "$1" in
		--tdisplay-p4)
			ENV_NAME="tdisplay-p4"
			;;
		--port)
			if [ $# -lt 2 ]; then
				echo "$1 needs a port argument"
				exit 1
			fi
			PORT="$2"
			shift
			;;
		--erase|-E)
			ERASE_FIRST=true
			;;
		--fullclean|-F)
			FULLCLEAN=true
			;;
		--just-build|-B)
			JUST_BUILD=true
			;;
		--help|-h)
			show_usage
			exit 0
			;;
		*)
			echo "Unknown argument: $1"
			show_usage
			exit 1
			;;
	esac
	shift
done

if [ "$JUST_BUILD" = true ] && [ "$ERASE_FIRST" = true ]; then
	echo "--erase needs a connected device; it cannot be combined with --just-build."
	exit 1
fi

# ESP-Hosted is vendored in-tree (third_party/esp_hosted), not a submodule, so
# a plain clone is enough to build. Catch a truncated checkout early all the
# same: the error it produces otherwise is a confusing missing-component one.
if [ -z "$(ls -A third_party/esp_hosted 2>/dev/null)" ]; then
	echo "third_party/esp_hosted is empty: this checkout is incomplete." >&2
	exit 1
fi

if [ "$ERASE_FIRST" = true ]; then
	run_pio_target "erase" "Erasing device flash"
fi

BUILD_START_TS="$(date +%s)"
if [ "$FULLCLEAN" = true ]; then
	run_pio_target "fullclean" "Full clean"
fi
if [ "$JUST_BUILD" = true ]; then
	echo "[PIO] Build ($ENV_NAME)..."
	run_pio run -e "$ENV_NAME"
else
	run_pio_target "upload" "Upload"
fi
BUILD_END_TS="$(date +%s)"
BUILD_ELAPSED_SECS=$((BUILD_END_TS - BUILD_START_TS))
echo "[PIO] Build completed in $(format_duration "$BUILD_ELAPSED_SECS")."

ELF_PATH="$BUILD_DIR/${ENV_NAME}/firmware.elf"
BIN_PATH="$BUILD_DIR/${ENV_NAME}/firmware.bin"
if [ -f "$ELF_PATH" ]; then
	ELF_SHA="$(shasum -a 256 "$ELF_PATH" | awk '{print $1}')"
	echo "[PIO] ELF SHA256: $ELF_SHA"
	echo "[PIO] Boot log 'ELF file SHA256' line should start with: ${ELF_SHA:0:8}"
fi
if [ -f "$BIN_PATH" ]; then
	BIN_SHA="$(shasum -a 256 "$BIN_PATH" | awk '{print $1}')"
	echo "[PIO] BIN SHA256: $BIN_SHA  ($(( $(wc -c <"$BIN_PATH") / 1024 )) KB)"
fi

if [ "$JUST_BUILD" = true ]; then
	exit 0
fi

run_pio_target "monitor" "Monitor"
