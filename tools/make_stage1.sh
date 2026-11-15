#!/bin/sh
# make_stage1.sh OUT : tinyc's small first initrd (finds the stick, makes the data part, starts tinyc)
#   ARCH=x86 sh make_stage1.sh OUT : the 32-bit one
set -e
T=/root/tinyc; R=$T/root; W=$(mktemp -d)
BB=$T/buildroot/bin/busybox.static; MKP=$T/buildroot/src/tinyc-mkpart; LD=ld-musl-x86_64.so.1; LC=libc.musl-x86_64.so.1
if [ "$ARCH" = x86 ]; then
    R=/root/tinyc32/root; BB=/root/tinyc32/buildroot/bin/busybox.static; MKP=/root/tinyc32/buildroot/src/out/sbin/tinyc-mkpart
    LD=ld-musl-i386.so.1; LC=libc.musl-x86.so.1
fi
mkdir -p $W/bin $W/sbin $W/lib $W/etc $W/proc $W/sys $W/dev $W/tmp
cp $BB $W/bin/busybox                      # static BusyBox
for a in sh mount umount sleep blkid sed readlink basename losetup switch_root setsid dmesg printf cat mkdir uname reboot poweroff sync ls; do
    ln -s busybox $W/bin/$a
done
cp $MKP $W/sbin/tinyc-mkpart
# mke2fs (ext4 for the data part) from Alpine, with the musl libraries it needs
cp $R/sbin/mke2fs $W/sbin/mke2fs
cp $R/lib/$LD $W/lib/
# every library mke2fs needs, and theirs
need() { readelf -d "$1" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'; }
todo=$(need $R/sbin/mke2fs)
while [ -n "$todo" ]; do
    next=""
    for l in $todo; do
        [ -e $W/lib/$l ] && continue
        case $l in $LC) ln -sf $LD $W/lib/$l; continue ;; esac
        f=$(ls $R/lib/$l $R/usr/lib/$l 2>/dev/null | head -1); [ -n "$f" ] || { echo "missing $l"; exit 1; }
        cp -L "$f" $W/lib/$l; next="$next $(need "$f")"
    done
    todo=$next
done
cp $R/etc/mke2fs.conf $W/etc/ 2>/dev/null || true
cp $T/stage1/init $W/init; chmod +x $W/init
cp $T/overlay/etc/tinyc-version $W/tinyc-version
( cd $W && find . -print0 | cpio --null -o -H newc --owner=0:0 2>/dev/null | gzip -9 ) > "$1"
rm -rf $W
ls -la "$1"
