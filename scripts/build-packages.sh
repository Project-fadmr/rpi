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
for p in packages/*/; do
	(cd "$p" && abuild -F -r)
done
