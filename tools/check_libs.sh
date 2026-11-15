#!/bin/sh
# check_libs.sh ROOT : every ELF file's NEEDED libraries must exist in ROOT (musl paths)
R=$1; bad=0
find "$R" -xdev -type f \( -perm -u+x -o -name '*.so*' \) 2>/dev/null | while read -r f; do
    head -c4 "$f" 2>/dev/null | grep -q 'ELF' || continue
    for n in $(readelf -d "$f" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
        [ -e "$R/lib/$n" ] || [ -e "$R/usr/lib/$n" ] || [ -e "$R/usr/lib/xorg/modules/$n" ] || echo "MISSING $n  <- ${f#$R}"
    done
done
