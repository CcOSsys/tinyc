/* tinyc-tray - the network and volume icons next to the clock (freedesktop system tray, plain Xlib)
 *   network: cable / Wi-Fi bars (signal) / crossed out when not connected.  click -> Wi-Fi window
 *   volume:  speaker with waves / crossed out when muted.  click -> volume slider, wheel -> louder/quieter */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>

#define SZ 22
static Display *dpy; static int scr; static Window root;
static Atom a_tray, a_opcode, a_xembed_info, a_manager;
static Window w_net, w_vol;
static GC gc;
static unsigned long c_on, c_off, c_red;

/* ---- what to show */
static int net_kind;      /* 0 none, 1 cable, 2 wifi */
static int net_bars;      /* 0..4 */
static int vol_pct = -1, vol_mute;

static void read_net(void) {
    net_kind = 0; net_bars = 0;
    DIR *d = opendir("/sys/class/net"); struct dirent *e;
    if (d) {
        while ((e = readdir(d))) {
            if (strncmp(e->d_name, "eth", 3) && strncmp(e->d_name, "en", 2)) continue;
            char p[300]; snprintf(p, sizeof p, "/sys/class/net/%s/carrier", e->d_name);
            FILE *f = fopen(p, "r"); int c = 0; if (f) { if (fscanf(f, "%d", &c) != 1) c = 0; fclose(f); }
            if (c == 1) { net_kind = 1; break; }
        }
        closedir(d);
    }
    if (net_kind) return;
    FILE *f = fopen("/proc/net/wireless", "r"); char l[256];
    if (!f) return;
    while (fgets(l, sizeof l, f)) {          /* " wlan0: 0000   54.  -56.  ..." : link quality out of 70 */
        char ifn[32]; int st; float q;
        if (sscanf(l, " %31[^:]: %x %f", ifn, &st, &q) == 3 && !strncmp(ifn, "wl", 2)) {
            /* connected = it has an address */
            char cmd[96]; snprintf(cmd, sizeof cmd, "ip -4 addr show dev %s 2>/dev/null | grep -q inet", ifn);
            if (system(cmd) == 0) { net_kind = 2; net_bars = q >= 52 ? 4 : q >= 38 ? 3 : q >= 24 ? 2 : 1; }
        }
    }
    fclose(f);
}
static void read_vol(void) {
    FILE *p = popen("amixer get Master 2>/dev/null", "r"); char l[256]; int pct = -1, mute = 0;
    if (!p) return;
    while (fgets(l, sizeof l, p)) {
        char *a = strchr(l, '['); if (!a) continue;
        if (pct < 0) sscanf(a, "[%d%%]", &pct);
        if (strstr(l, "[off]")) mute = 1;
    }
    pclose(p);
    vol_pct = pct; vol_mute = mute;
}

/* ---- drawing */
static void line(Window w, int x1, int y1, int x2, int y2) { XDrawLine(dpy, w, gc, x1, y1, x2, y2); }
static void draw_net(void) {
    XClearWindow(dpy, w_net);
    XSetLineAttributes(dpy, gc, 2, LineSolid, CapRound, JoinRound);
    if (net_kind == 1) {                     /* a cable plug */
        XSetForeground(dpy, gc, c_on);
        XFillRectangle(dpy, w_net, gc, 6, 4, 10, 8);
        line(w_net, 9, 4, 9, 1); line(w_net, 13, 4, 13, 1);
        line(w_net, 11, 12, 11, 20);
        return;
    }
    for (int i = 0; i < 4; i++) {           /* Wi-Fi bars, lit up to the signal */
        XSetForeground(dpy, gc, net_kind == 2 && i < net_bars ? c_on : c_off);
        int h = 4 + i * 4;
        XFillRectangle(dpy, w_net, gc, 2 + i * 5, 20 - h, 3, h);
    }
    if (net_kind == 0) {                     /* not connected: a red x */
        XSetForeground(dpy, gc, c_red);
        line(w_net, 13, 3, 19, 9); line(w_net, 19, 3, 13, 9);
    }
}
static void draw_vol(void) {
    XClearWindow(dpy, w_vol);
    XSetLineAttributes(dpy, gc, 2, LineSolid, CapRound, JoinRound);
    XSetForeground(dpy, gc, vol_pct < 0 ? c_off : c_on);
    XPoint sp[] = {{2, 8}, {6, 8}, {11, 3}, {11, 19}, {6, 14}, {2, 14}};
    XFillPolygon(dpy, w_vol, gc, sp, 6, Convex, CoordModeOrigin);
    if (vol_pct < 0) return;
    if (vol_mute || vol_pct == 0) {
        XSetForeground(dpy, gc, c_red);
        line(w_vol, 14, 7, 20, 15); line(w_vol, 20, 7, 14, 15);
        return;
    }
    if (vol_pct > 0)  XDrawArc(dpy, w_vol, gc, 7, 7, 8, 8, -60 * 64, 120 * 64);
    if (vol_pct > 33) XDrawArc(dpy, w_vol, gc, 6, 4, 12, 14, -60 * 64, 120 * 64);
    if (vol_pct > 66) XDrawArc(dpy, w_vol, gc, 5, 1, 16, 20, -60 * 64, 120 * 64);
}

/* ---- the tray */
static void dock(Window w) {
    Window tray = XGetSelectionOwner(dpy, a_tray);
    if (tray == None) return;
    XEvent ev; memset(&ev, 0, sizeof ev);
    ev.xclient.type = ClientMessage; ev.xclient.window = tray; ev.xclient.message_type = a_opcode;
    ev.xclient.format = 32; ev.xclient.data.l[0] = CurrentTime; ev.xclient.data.l[1] = 0; /* REQUEST_DOCK */
    ev.xclient.data.l[2] = w;
    XSendEvent(dpy, tray, False, NoEventMask, &ev); XSync(dpy, False);
}
static unsigned long c_bg;
static int xerr(Display *d, XErrorEvent *e) { (void)d; (void)e; return 0; }   /* the tray may go away: never die */
static Window make(Window parent, int x, int w, const char *name) {
    XSetWindowAttributes at; at.background_pixel = c_bg;   /* the taskbar's colour (ParentRelative breaks on a 32-bit tray) */
    at.event_mask = ExposureMask | ButtonPressMask | StructureNotifyMask;
    Window win = XCreateWindow(dpy, parent, x, 0, w, SZ, 0, CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &at);
    XStoreName(dpy, win, name);
    return win;
}
static void run(const char *cmd) {           /* start a program, not waiting for it */
    if (fork() == 0) { setsid(); execl("/bin/sh", "sh", "-c", cmd, (char *)NULL); _exit(1); }
}
static unsigned long rgb(int r, int g, int b) {
    XColor c; c.red = r * 257; c.green = g * 257; c.blue = b * 257; c.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, DefaultColormap(dpy, scr), &c); return c.pixel;
}

int main(void) {
    signal(SIGCHLD, SIG_IGN);
    dpy = XOpenDisplay(NULL); if (!dpy) return 1;
    scr = DefaultScreen(dpy); root = RootWindow(dpy, scr);
    char tn[32]; snprintf(tn, sizeof tn, "_NET_SYSTEM_TRAY_S%d", scr);
    a_tray = XInternAtom(dpy, tn, False); a_opcode = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
    a_xembed_info = XInternAtom(dpy, "_XEMBED_INFO", False); a_manager = XInternAtom(dpy, "MANAGER", False);
    XSelectInput(dpy, root, StructureNotifyMask);
    gc = XCreateGC(dpy, root, 0, NULL);
    c_on = rgb(0x24, 0x26, 0x2a); c_off = rgb(0xb4, 0xbc, 0xc8); c_red = rgb(0xd8, 0x3a, 0x3a);
    c_bg = rgb(0xf3, 0xf5, 0xf8);
    XSetErrorHandler(xerr);
    w_vol = make(root, 0, SZ, "Volume"); w_net = make(root, 0, SZ, "Network");
    Window both[2] = {w_vol, w_net};
    for (int i = 0; i < 2; i++) {
        long info[2] = {0, 1};               /* XEmbed version 0, mapped */
        XChangeProperty(dpy, both[i], a_xembed_info, a_xembed_info, 32, PropModeReplace, (unsigned char *)info, 2);
        XSizeHints *sh = XAllocSizeHints(); sh->flags = PMinSize | PBaseSize;
        sh->min_width = sh->base_width = SZ; sh->min_height = sh->base_height = SZ;
        XSetWMNormalHints(dpy, both[i], sh); XFree(sh);
    }
    for (int i = 0; i < 40 && XGetSelectionOwner(dpy, a_tray) == None; i++) usleep(250000);   /* the taskbar starts */
    dock(w_vol); usleep(300000); dock(w_net);
    read_net(); read_vol();
    int fd = ConnectionNumber(dpy), n = 0;
    for (;;) {
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            if (ev.type == Expose && ev.xexpose.count == 0) { if (ev.xexpose.window == w_net) draw_net(); else draw_vol(); }
            else if (ev.type == ButtonPress) {
                if (ev.xbutton.window == w_net && ev.xbutton.button == 1) run("wifi");
                else if (ev.xbutton.window == w_vol) {
                    if (ev.xbutton.button == 1) run("tinyc-volume");
                    else if (ev.xbutton.button == 4) system("amixer -q set Master 5%+ unmute");
                    else if (ev.xbutton.button == 5) system("amixer -q set Master 5%-");
                    else if (ev.xbutton.button == 3 || ev.xbutton.button == 2) system("amixer -q set Master toggle");
                    read_vol(); draw_vol();
                }
            } else if (ev.type == ClientMessage && ev.xclient.message_type == a_manager) {
                dock(w_vol); usleep(300000); dock(w_net);    /* the taskbar came back: dock again */
            }
        }
        fd_set fs; FD_ZERO(&fs); FD_SET(fd, &fs);
        struct timeval tv = {1, 0};
        if (select(fd + 1, &fs, NULL, NULL, &tv) == 0) {   /* every second: volume; every 4: network */
            int ov = vol_pct, om = vol_mute; read_vol(); if (ov != vol_pct || om != vol_mute) draw_vol();
            if (n < 60) {                 /* not in the taskbar yet? ask again */
                Window r, par, *kids; unsigned nk;
                Window ws[2] = {w_vol, w_net};
                for (int i = 0; i < 2; i++)
                    if (XQueryTree(dpy, ws[i], &r, &par, &kids, &nk)) { if (kids) XFree(kids); if (par == root) dock(ws[i]); }
            }
            if (++n % 4 == 0) { int ok = net_kind, ob = net_bars; read_net(); if (ok != net_kind || ob != net_bars) draw_net(); }
        }
    }
}
