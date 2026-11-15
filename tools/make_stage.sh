#!/bin/sh
# make_stage.sh : the build root (/root/tinyc/root, Alpine + packages) -> what tinyc ships (/root/tinyc/stage)
# leaves out 3D graphics (Mesa/LLVM), extra image loaders, docs and other things tinyc doesn't use
#   ARCH=x86 sh make_stage.sh : the 32-bit tinyc (from /root/tinyc32: its own Alpine root, kernel modules and programs)
set -e
T=/root/tinyc; W=$T; [ "$ARCH" = x86 ] && W=/root/tinyc32
R=$W/root; S=$W/stage
rm -rf "$S"; mkdir -p "$S"
( cd "$R" && tar cf - --exclude=./proc/* --exclude=./sys/* --exclude=./dev/* --exclude=./var/cache/apk/* . ) | ( cd "$S" && tar xf - )
cd "$S"
# 3D: Mesa, LLVM, SPIR-V, and the X modules that use them (X draws with the CPU on fbdev/vesa)
rm -f usr/lib/libLLVM* usr/lib/libgallium* usr/lib/libGL* usr/lib/libEGL* usr/lib/libGLES* usr/lib/libSPIRV* usr/lib/libspirv* \
      usr/lib/libglapi* usr/lib/libvulkan* \
      usr/lib/xorg/modules/libglamoregl.so usr/lib/xorg/modules/extensions/libglx.so usr/bin/spirv-*
      # (kept: the modesetting X driver and the tiny libgbm it links; without glamor it draws with the CPU
      #  but still gets a hardware pointer and 60 Hz page flips on computers with a real graphics driver)
rm -rf usr/lib/dri usr/lib/gbm usr/share/vulkan usr/share/glvnd usr/share/drirc.d
# docs, headers, static libraries, locales
rm -rf usr/share/doc usr/share/man usr/share/info usr/include usr/share/gtk-doc usr/share/locale usr/share/i18n usr/lib/pkgconfig usr/share/pkgconfig usr/share/aclocal
find . -name '*.a' -delete
# apk's downloads; the CA bundle back to Alpine's own (no build-machine proxy certificate)
rm -rf var/cache/apk/*
[ -f $W/alpine-ca.crt.orig ] && cp $W/alpine-ca.crt.orig etc/ssl/certs/ca-certificates.crt
rm -f etc/resolv.conf

# --- make it tiny: leftovers nothing in tinyc uses
# fonts: DejaVu Sans, Sans Mono and Serif (regular + bold); the old X bitmap fonts only in their basic sizes
( cd usr/share/fonts/dejavu && for f in *.ttf; do case $f in DejaVuSans.ttf|DejaVuSans-Bold.ttf|DejaVuSansMono.ttf|DejaVuSansMono-Bold.ttf|DejaVuSerif.ttf|DejaVuSerif-Bold.ttf) ;; *) rm -f "$f" ;; esac; done )
( cd usr/share/fonts/misc && for f in *.pcf.gz; do case $f in cursor.pcf.gz|6x13.pcf.gz|6x13B.pcf.gz|7x14.pcf.gz|7x14B.pcf.gz|9x15.pcf.gz|9x15B.pcf.gz|10x20.pcf.gz) ;; *) rm -f "$f" ;; esac; done )
rm -rf usr/share/fonts/100dpi usr/share/fonts/75dpi usr/share/fonts/cyrillic
chroot . /usr/bin/mkfontscale /usr/share/fonts/misc /usr/share/fonts/dejavu 2>/dev/null || true
chroot . /usr/bin/mkfontdir /usr/share/fonts/misc /usr/share/fonts/dejavu
chroot . /usr/bin/fc-cache -f >/dev/null 2>&1 || true
# Xfe: one icon set; IceWM: only the ccos theme and the default one
( cd usr/share/xfe/icons && for d in *; do [ "$d" = default-theme ] || rm -rf "$d"; done )
( cd usr/share/icewm/themes && for d in *; do case $d in default) ;; *) rm -rf "$d" ;; esac; done )
# the MIME database's sources (programs read the compiled mime.cache), ALSA's test sounds,
# the PCI name list, GObject introspection data (for Python/JS apps; they bring their own back)
rmdir media/cdrom media/floppy media/usb 2>/dev/null || true
rm -rf usr/share/mime/packages usr/share/sounds/alsa usr/share/hwdata/pci.ids usr/lib/girepository-1.0
# tinyc's own files on top
cp -a $T/overlay/. "$S/"
# 32-bit: tinyc's own programs built for 32-bit (the overlay has the 64-bit ones)
[ "$ARCH" = x86 ] && cp -a $W/buildroot/src/out/usr/. "$S/usr/"
# the Terminal uses bash (for the easy commands)
sed -i "s#^root:x:0:0:root:/root:/bin/sh\$#root:x:0:0:root:/root:/bin/bash#" "$S/etc/passwd"
chmod +x "$S/sbin/tinyc-init"
echo "stage with overlay: $(du -xsh "$S" | cut -f1)"
# Wi-Fi firmware (the newest file per chip: Intel, Realtek, Atheros USB)
cp -a $T/firmware/. "$S/"
# the kernel's modules (Wi-Fi drivers)
[ -d $W/modules/lib/modules ] && cp -a $W/modules/lib/modules "$S/lib/"
echo "stage with firmware and modules: $(du -xsh "$S" | cut -f1)"
# what tinyc itself is made of (Apps won't remove these)
mkdir -p "$S/etc/tinyc"; cp $R/etc/apk/world "$S/etc/tinyc/base-world"
# a copy of the app list, so Apps can show apps before the first update
mkdir -p "$S/var/cache/apk"; cp $W/buildroot/var/cache/apk/APKINDEX.*.tar.gz "$S/var/cache/apk/" 2>/dev/null || true
# packages tinyc ships with some files left out (3D graphics, image loaders): apps that need them get them back
python3 - "$R" "$S" <<'PY'
import sys, os
root, stage = sys.argv[1], sys.argv[2]
trimmed, allp, cur, folder = set(), [], None, ''
minor = ('usr/lib/girepository-1.0', 'usr/share/mime/packages', 'usr/share/fonts', 'usr/share/xfe/icons', 'usr/share/icewm/themes',
         'usr/share/sounds', 'usr/share/hwdata', 'usr/share/aclocal', 'usr/lib/pkgconfig', 'usr/share/pkgconfig', 'usr/include',
         'usr/share/locale', 'media', 'usr/share/doc', 'usr/share/man', 'usr/share/info', 'usr/share/i18n', 'usr/share/applications')
for l in open(root + '/lib/apk/db/installed'):
    l = l.rstrip('\n')
    if l.startswith('P:'): cur = l[2:]; allp.append(cur)
    elif l.startswith('F:'): folder = l[2:]
    elif l.startswith('R:') and cur and not os.path.lexists(os.path.join(stage, folder, l[2:])):
        # small leftovers removed on purpose (fonts, icons, docs...) don't count: apps don't need them back
        if not folder.startswith(minor): trimmed.add(cur)
open(stage + '/etc/tinyc/trimmed', 'w').write('\n'.join(sorted(trimmed)) + '\n')
open(stage + '/etc/tinyc/base-installed', 'w').write('\n'.join(sorted(allp)) + '\n')
print('trimmed packages:', ' '.join(sorted(trimmed)))
PY
# menu entries for programs that aren't there (Xfe's package lists tools it doesn't ship) or are already in the menu
rm -f "$S/usr/share/applications/xfa.desktop" "$S/usr/share/applications/xfi.desktop" "$S/usr/share/applications/xfp.desktop" "$S/usr/share/applications/xfw.desktop" "$S/usr/share/applications/uxterm.desktop" "$S/usr/share/applications/xterm.desktop" "$S/usr/share/applications/xfe.desktop"
# the modules' index (which module for which device), made with tinyc's own depmod
for k in "$S"/lib/modules/*; do [ -d "$k" ] && chroot "$S" /sbin/depmod -a "$(basename "$k")"; done
