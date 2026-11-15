# tinyCcOS (tinyc)

A tiny version of [CcOS](https://ccossys.github.io), inspired by Tiny Core Linux. It does the normal
things a computer should do, it just isn't fancy about it. It even runs on a potato.

- Built on Alpine Linux (apk), with the IceWM taskbar and the /o\ CcOS look
- Runs from a USB stick; your files, settings and apps are kept on the same stick
- Apps (install programs), Wi-Fi, Files (Xfe), Notepad, Terminal, Settings
- The easy Terminal commands from CcOS: `mkf`, `rd`, `adf`, `rn`, `cpf`, `rmt`, `op`,
  `lsusb` / `cdusb` / `xusb` / `jusb`, `space`, `net`, `whereami`, `hints off/on`
- 64-bit, and a separate 32-bit version for really old computers

Docs: https://ccossys.github.io/tinyc/docs.html

## What's in here

| Folder / file | What it is |
| --- | --- |
| `apps/` | tinyc's own programs (C/C++ with FLTK and Xlib) |
| `overlay/` | tinyc's own files, copied on top of Alpine (start-up, settings, easy commands, theme, icons, wallpapers) |
| `stage1/init` | the small first start-up: finds the stick, makes the data part, starts tinyc |
| `tools/` | build scripts: `make_stage.sh`, `make_stage1.sh`, `make_usbimg.sh`, `grub.cfg` |
| `kernel-tinyc.config`, `kernel-tinyc32.config` | the Linux kernel settings (64-bit / 32-bit) |
| `packages.txt` | the Alpine packages tinyc is made of |
| `THIRD-PARTY.txt` | everything tinyc is built from, and their licences |

## How it's built (roughly)

1. Unpack Alpine's minirootfs (x86_64, or x86 for 32-bit) and `apk add` everything in `packages.txt`.
2. Build Linux 6.18.63 with the kernel config (`make bzImage modules`, `modules_install`).
3. Build the programs in `apps/` in an Alpine build root (`build-base fltk-dev busybox-static`):
   `g++ -Os -o settings settings.cxx -lfltk_images -lfltk -lX11 ...`, `gcc -Os -o tinyc-tray tray.c -lX11`.
4. `sh tools/make_stage.sh` → `mksquashfs stage system.sfs -comp xz` → `sh tools/make_stage1.sh stage1.gz`
   → `sh tools/make_usbimg.sh tinyc-usb.img` (add `ARCH=x86` in front of each for the 32-bit one).

The scripts expect the build folders in `/root/tinyc` (and `/root/tinyc32`); change the paths at the top for yours.
