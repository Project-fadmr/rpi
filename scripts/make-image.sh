#!/bin/sh
# Runs inside an x86_64 Alpine container (see build.sh): assembles the boot image (SD card or USB drive)
# from the official tarball, a local repo with every package, and the apkovl.
set -eu -o pipefail
trap 'chown -R "$HOST_ID" out' EXIT

apk add --no-cache abuild dosfstools mtools sfdisk tar

M=/tmp/card	# staging dir only; the Pi finds custom/ via .boot_repository
R=/tmp/root
O=/tmp/ovl
mkdir -p $M $R/etc/apk/keys $O

tar xzf cache/alpine-rpi-$ALPINE_VER-aarch64.tar.gz -C $M
krel=$(sed -n 's/^_krel=\([^[:space:]]*\).*/\1/p' packages/espnow-nexmon/APKBUILD)
[ -f "$M/boot/config-$krel" ] || { echo "tarball kernel is not $krel" >&2; exit 1; }

cp boot/usercfg.txt boot/cmdline.txt $M/

# One signed local repo with every package (ours + upstream deps), so the
# diskless boot installs everything offline. The .boot_repository marker makes
# Alpine's initramfs find it wherever the card is mounted.
A="apk --root $R --arch aarch64 -X /work/out/repo/packages"
mkdir -p $M/custom/aarch64
cp /usr/share/apk/keys/aarch64/* keys/espnow.rsa.pub $R/etc/apk/keys/
cp overlay/etc/apk/repositories $R/etc/apk/
$A --initdb add
$A update
$A fetch --recursive -o $M/custom/aarch64 $(cat overlay/etc/apk/world)
(cd $M/custom/aarch64 && apk index --allow-untrusted --rewrite-arch aarch64 -o APKINDEX.tar.gz *.apk &&
	abuild-sign -k /work/keys/espnow.rsa -p espnow.rsa.pub APKINDEX.tar.gz)
touch $M/custom/.boot_repository
# Real install with the Pi's boot-time flags (see initramfs /init), so a package
# the Pi can't get fails the build instead of silently missing at boot
mkdir -p /tmp/check/etc/apk && cp -r $R/etc/apk/keys /tmp/check/etc/apk/
apk --root /tmp/check --arch aarch64 --initdb add
apk --root /tmp/check --arch aarch64 --repositories-file /dev/null \
	--repository $M/apks --repository $M/custom --no-scripts \
	--initramfs-diskless-boot --no-network add $(cat overlay/etc/apk/world) alpine-base | tail -1

# apkovl: overlay files + key, runlevels, optional SSH key
cp -a overlay/. $O/
mkdir -p $O/etc/apk/keys $O/etc/runlevels/sysinit $O/etc/runlevels/boot \
	$O/etc/runlevels/default $O/etc/runlevels/shutdown
cp keys/espnow.rsa.pub $O/etc/apk/keys/
for s in devfs dmesg mdev hwdrivers modloop; do ln -s /etc/init.d/$s $O/etc/runlevels/sysinit/; done
for s in modules sysctl hostname bootmisc syslog; do ln -s /etc/init.d/$s $O/etc/runlevels/boot/; done
for s in networking dropbear espnow local; do ln -s /etc/init.d/$s $O/etc/runlevels/default/; done
for s in killprocs mount-ro; do ln -s /etc/init.d/$s $O/etc/runlevels/shutdown/; done
install -Dm755 test/espnow_rx.py $O/root/espnow_rx.py
if [ -n "$SSH_PUBKEY" ]; then
	mkdir -p $O/root/.ssh
	echo "$SSH_PUBKEY" > $O/root/.ssh/authorized_keys
	chmod 700 $O/root/.ssh
	chmod 600 $O/root/.ssh/authorized_keys
fi
# member names like lbu writes them (etc/..., no ./ prefix)
tar -C $O --owner=0 --group=0 --numeric-owner -czf $M/espnow-pi.apkovl.tar.gz $(ls -A $O)

# FAT32 partition, then a disk image with one bootable partition at 1 MiB
size_mb=$(( $(du -sm $M | cut -f1) * 11 / 10 + 64 ))
mkfs.vfat -F 32 -n ALPINE -C /tmp/part.img $((size_mb * 1024))
(cd $M && MTOOLS_SKIP_CHECK=1 mcopy -s -i /tmp/part.img ./* ./.alpine-release ::/)
img=out/alpine-espnow-pi5.img
rm -f $img
truncate -s $((size_mb + 1))M $img
echo 'start=2048, type=c, bootable' | sfdisk -q $img
dd if=/tmp/part.img of=$img bs=1M seek=1 conv=notrunc status=none

MTOOLS_SKIP_CHECK=1 mdir -i /tmp/part.img ::/ | grep -E 'apkovl|custom|usercfg|cmdline'
echo "Built $img (${size_mb} MiB)"
