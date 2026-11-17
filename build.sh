#!/bin/sh
# Builds out/alpine-espnow-pi5.img. Needs only Docker, no sudo. See README.md.
#   ./build.sh          build everything
#   ./build.sh binfmt   one-time: register arm64 emulation (until host reboot)
set -eu
cd "$(dirname "$0")"

# Pins: keep in sync with docs/pins.md
ALPINE_VER=3.23.8
ALPINE_BRANCH=v3.23
ALPINE_SHA256=b8e6298889deac0382df24e5d94362def5558825c55c7c0f4e538244aa9daa56
ALPINE_IMAGE=alpine@sha256:ff7f0c727dd796aa36787bfcd9a64bf35a6ce72a73451b3359fde849b6703d63
BINFMT_IMAGE=tonistiigi/binfmt@sha256:400a4873b838d1b89194d982c45e5fb3cda4593fbfd7e08a02e76b03b21166f0
MIRROR=https://dl-cdn.alpinelinux.org/alpine

if [ "${1:-}" = binfmt ]; then
	exec docker run --rm --privileged "$BINFMT_IMAGE" --install arm64
fi

if ! docker run --rm --platform linux/arm64 "$ALPINE_IMAGE" true 2>/dev/null; then
	echo "arm64 emulation is not registered: run './build.sh binfmt' once." >&2
	exit 1
fi

tarball=cache/alpine-rpi-$ALPINE_VER-aarch64.tar.gz
mkdir -p cache out
[ -f "$tarball" ] || curl -fLo "$tarball" "$MIRROR/$ALPINE_BRANCH/releases/aarch64/${tarball#cache/}"
echo "$ALPINE_SHA256  $tarball" | sha256sum -c -

# Optional SSH key for root (dropbear only accepts keys)
: "${SSH_PUBKEY_FILE:=$HOME/.ssh/id_ed25519.pub}"
SSH_PUBKEY=$(cat "$SSH_PUBKEY_FILE" 2>/dev/null || true)
[ -n "$SSH_PUBKEY" ] && echo "Adding SSH key from $SSH_PUBKEY_FILE" || echo "No SSH key: log in over the UART console"

run() {
	docker run --rm -v "$PWD:/work" -w /work -e HOST_ID="$(id -u):$(id -g)" "$@"
}
run --platform linux/arm64 "$ALPINE_IMAGE" sh scripts/build-packages.sh
run -e ALPINE_VER="$ALPINE_VER" -e ALPINE_BRANCH="$ALPINE_BRANCH" -e MIRROR="$MIRROR" \
	-e SSH_PUBKEY="$SSH_PUBKEY" "$ALPINE_IMAGE" sh scripts/make-image.sh
