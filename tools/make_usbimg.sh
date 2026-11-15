#!/bin/sh
# make_usbimg.sh OUT.img : the tinyc USB stick image (write it with Rufus or dd)
#   one FAT32 part "TINYC": GRUB (BIOS + UEFI), the kernel, the small initrd and tinyc/system.sfs.
#   The rest of the stick becomes "tinyc-data" (your files, settings and apps) on the first start.
#   ARCH=x86 sh make_usbimg.sh OUT.img : the 32-bit stick (old BIOS, and 32-bit UEFI)
set -e
T=/root/tinyc; D=$T/tools; OUT=$1; W32=/root/tinyc32
K=$T/bzImage-tinyc; I=$T/stage1.gz; SFS=$T/system.sfs
[ "$ARCH" = x86 ] && { K=$W32/bzImage-tinyc32; I=$W32/stage1.gz; SFS=$W32/system.sfs; }
W=$(mktemp -d); FATMB=128
mkdir -p $W/f/boot/grub $W/f/tinyc $W/f/EFI/BOOT
cp $K $W/f/boot/vmlinuz
cp $I $W/f/boot/initrd.gz
cp $SFS $W/f/tinyc/system.sfs
cp $D/grub.cfg $W/f/boot/grub/grub.cfg
cat > $W/f/tinyc/tinyc.txt <<'TXT'
This is a tinyc USB stick.
tinyc itself is on this small part of the stick. Your files, settings and installed
apps are kept on the rest of the stick (a second part called "tinyc-data", made the
first time tinyc starts). Windows may offer to format it: say no.
TXT
printf 'search --no-floppy --file --set=root /tinyc/tinyc.txt\nset prefix=($root)/boot/grub\nconfigfile ($root)/boot/grub/grub.cfg\n' > $W/embed.cfg
MODS="part_msdos part_gpt fat search search_fs_file configfile linux normal echo test all_video gfxterm reboot halt"
# UEFI
if [ "$ARCH" = x86 ]; then
    # 32-bit UEFI (some old netbooks and tablets), with the 32-bit GRUB from the 32-bit build root
    mkdir -p $W32/buildroot/tmp/efi && cp $W/embed.cfg $W32/buildroot/tmp/efi/embed.cfg
    chroot $W32/buildroot /usr/bin/grub-mkstandalone -O i386-efi -o /tmp/efi/BOOTIA32.EFI --modules="$MODS" --locales="" --themes="" --fonts="" "boot/grub/grub.cfg=/tmp/efi/embed.cfg"
    mv $W32/buildroot/tmp/efi/BOOTIA32.EFI $W/f/EFI/BOOT/BOOTIA32.EFI
else
    grub-mkstandalone -O x86_64-efi -o $W/f/EFI/BOOT/BOOTX64.EFI --modules="$MODS" --locales="" --themes="" --fonts="" "boot/grub/grub.cfg=$W/embed.cfg"
fi
# old BIOS: GRUB's core goes in the gap after the partition table (sectors 1...)
grub-mkimage -O i386-pc -o $W/core.img -c $W/embed.cfg -p /boot/grub biosdisk $MODS
# the FAT32 part
truncate -s ${FATMB}M $W/fat.img
mkfs.vfat -F 32 -n TINYC $W/fat.img >/dev/null
mcopy -s -i $W/fat.img $W/f/* ::/
truncate -s 0 "$OUT"; truncate -s $((FATMB + 1))M "$OUT"
dd if=$W/fat.img of="$OUT" bs=1M seek=1 conv=notrunc status=none
dd if=$W/core.img of="$OUT" bs=512 seek=1 conv=notrunc status=none
python3 - "$OUT" $FATMB <<'PY'
import sys, struct
out, mb = sys.argv[1], int(sys.argv[2])
boot = open('/usr/lib/grub/i386-pc/boot.img', 'rb').read()
mbr = bytearray(512)
mbr[0:440] = boot[0:440]                                     # GRUB's BIOS boot code (it loads sector 1 on)
mbr[440:444] = struct.pack('<I', 0x74696e79)                 # disk id "tiny"
mbr[446:462] = bytes([0x80, 0xFE, 0xFF, 0xFF, 0x0C, 0xFE, 0xFF, 0xFF]) + struct.pack('<II', 2048, mb * 2048)
mbr[510:512] = b'\x55\xaa'
with open(out, 'r+b') as f: f.write(mbr)
PY
rm -rf $W
ls -la "$OUT"
