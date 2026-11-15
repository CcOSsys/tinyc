#!/bin/sh
# make_initrd.sh OUT : the stage -> a compressed initramfs
set -e
cd /root/tinyc/stage
find . -print0 | cpio --null -o -H newc --owner=0:0 2>/dev/null | xz -9 --check=crc32 -T0 > "$1"
ls -la "$1"
