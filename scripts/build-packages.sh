#!/bin/sh
# Runs inside an arm64 Alpine container (see build.sh): builds packages/* into
# a signed apk repo at out/repo/packages/aarch64.
set -eu
trap 'chown -R "$HOST_ID" out keys cache' EXIT

apk update && apk add abuild build-base openssl

mkdir -p keys out/repo cache/distfiles
if [ ! -f keys/espnow.rsa ]; then
	openssl genrsa -out keys/espnow.rsa 2048
	openssl rsa -in keys/espnow.rsa -pubout -out keys/espnow.rsa.pub
fi
cp keys/espnow.rsa.pub /etc/apk/keys/

export PACKAGER_PRIVKEY=/work/keys/espnow.rsa REPODEST=/work/out/repo SRCDEST=/work/cache/distfiles
# abuild -r installs each package's depends from the repo, so the kernel module
# (a dependency of py3-rpi-ws281x) is built first; every other package once, in glob order.
# -f rebuilds even when the apk is already in out/repo (abuild otherwise skips it as "up to date")
built=
for p in packages/rpi-ws281x-pwm/ packages/*/; do
	case " $built " in *" $p "*) continue ;; esac
	built="$built $p"
	(cd "$p" && abuild -F -f -r)
done
