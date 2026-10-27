#!/bin/sh
# Builds and flashes test/esp32_sender (or another sketch) onto an ESP32 through Docker (no sudo,
# no host toolchain). Usage: scripts/flash-esp32.sh [port] [board] [sketch-dir]
#   board: esp32c3 (default), esp32, esp32s3, esp32c6 ...
set -eu
cd "$(dirname "$0")/.."

PORT=${1:-/dev/ttyACM0}
BOARD=${2:-esp32c3}
SKETCH=${3:-test/esp32_sender}
# Pins: see docs/pins.md
ARDUINO_CLI=1.1.1
ESP32_CORE=3.2.0	# ESP-IDF 5.4: first core with ESP-NOW v2.0
IMAGE=python:3.12-slim@sha256:12316debbfb86890f6b2d825820612741ba527ad5a44e3902759141d572bfe8f

mkdir -p cache/arduino
docker run --rm --device "$PORT" -v "$PWD:/work" -w /work \
	-e HOME=/work/cache/arduino -e PORT="$PORT" -e BOARD="$BOARD" \
	-e SKETCH="$SKETCH" -e ARDUINO_CLI="$ARDUINO_CLI" -e ESP32_CORE="$ESP32_CORE" -e HOST_ID="$(id -u):$(id -g)" \
	"$IMAGE" sh -euc '
trap "chown -R \$HOST_ID cache/arduino" EXIT
cli=cache/arduino/arduino-cli-$ARDUINO_CLI
if [ ! -x "$cli" ]; then
	python3 -c "import urllib.request,sys; urllib.request.urlretrieve(sys.argv[1], \"/tmp/a.tgz\")" \
		"https://downloads.arduino.cc/arduino-cli/arduino-cli_${ARDUINO_CLI}_Linux_64bit.tar.gz"
	tar xzf /tmp/a.tgz -C /tmp arduino-cli && mv /tmp/arduino-cli "$cli"
fi
url=https://espressif.github.io/arduino-esp32/package_esp32_index.json
$cli core update-index --additional-urls $url
$cli core install esp32:esp32@$ESP32_CORE --additional-urls $url
fqbn=esp32:esp32:$BOARD
case $BOARD in esp32c3|esp32s3|esp32c6) fqbn=$fqbn:CDCOnBoot=cdc ;; esac
$cli compile --fqbn $fqbn "$SKETCH"
$cli upload --fqbn $fqbn -p "$PORT" "$SKETCH"
'
echo "Flashed. Serial monitor: picocom -b 115200 $PORT"
