// tinyc Wi-Fi - pick a network, type the password, connect.   (uses iwd's iwctl)
#include "tinyc_ui.h"
#include "tinyc_icons.h"
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <dirent.h>

struct Net { std::string name, security; int bars; bool connected, known; };
static std::vector<Net> nets;
static std::string dev;
static Fl_Double_Window *win;
static Fl_Hold_Browser *list;
static Fl_Box *status;
static Fl_Button *bconnect, *bforget;

static std::string run(const std::string &cmd) {
    std::string out; FILE *p = popen((cmd + " 2>&1").c_str(), "r");
    if (!p) return out;
    char b[512]; while (fgets(b, sizeof b, p)) out += b;
    pclose(p); return out;
}
static std::string strip(const std::string &s) {            // remove iwctl's colours
    std::string o; for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == 27) { while (i < s.size() && !isalpha((unsigned char)s[i])) i++; continue; }
        o += s[i];
    }
    return o;
}
static std::string q(const std::string &s) {                // safe for the shell
    std::string o = "'"; for (char c : s) { if (c == '\'') o += "'\\''"; else o += c; } return o + "'";
}
static std::string find_dev() {
    DIR *d = opendir("/sys/class/net"); std::string r;
    if (!d) return r;
    while (struct dirent *e = readdir(d)) if (!strncmp(e->d_name, "wl", 2)) { r = e->d_name; break; }
    closedir(d); return r;
}
static std::string connected_name() {
    std::string s = strip(run("iwctl station " + dev + " show"));
    size_t p = s.find("Connected network");
    if (p == std::string::npos) return "";
    p += strlen("Connected network");
    size_t e = s.find('\n', p); std::string v = s.substr(p, e - p);
    size_t a = v.find_first_not_of(" \t"), b = v.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : v.substr(a, b - a + 1);
}
static void fill() {
    list->clear(); nets.clear();
    std::string known = strip(run("iwctl known-networks list"));
    std::string cur = connected_name();
    std::string s = strip(run("iwctl station " + dev + " get-networks"));
    size_t pos = 0; int header = 0;
    while (pos < s.size()) {
        size_t e = s.find('\n', pos); if (e == std::string::npos) e = s.size();
        std::string l = s.substr(pos, e - pos); pos = e + 1;
        if (l.find("Network name") != std::string::npos) { header = 1; continue; }
        if (!header || l.find("----") != std::string::npos) continue;
        // "  > Name with spaces      psk       ****"
        size_t st = l.rfind('*'); if (st == std::string::npos) continue;
        int bars = 0; size_t k = st; while (k != std::string::npos && k > 0 && (l[k] == '*' || l[k] == ' ')) { if (l[k] == '*') bars++; k--; }
        std::string rest = l.substr(0, k + 1);
        size_t se = rest.find_last_not_of(" "); rest = rest.substr(0, se + 1);
        std::string sec = rest.substr(rest.find_last_of(' ') + 1);
        std::string name = rest.substr(0, rest.find_last_of(' '));
        size_t a = name.find_first_not_of(" >"); if (a == std::string::npos) continue;
        name = name.substr(a); name = name.substr(0, name.find_last_not_of(' ') + 1);
        Net n{name, sec, bars, name == cur, known.find(name) != std::string::npos};
        nets.push_back(n);
    }
    for (auto &n : nets) {
        std::string bar = std::string(n.bars, '|') + std::string(4 - std::min(n.bars, 4), '.');
        std::string lock = n.security == "open" ? "open" : "locked";
        std::string line = std::string(n.connected ? "@b" : "") + "@." + n.name + "\t" + bar + "\t" + lock + (n.connected ? "\tconnected" : n.known ? "\tsaved" : "");
        list->add(line.c_str());
    }
    static std::string st;
    st = cur.empty() ? (nets.empty() ? "No networks found. Click Refresh." : "Not connected. Pick a network and click Connect.")
                     : "Connected to " + cur + ".";
    status->label(st.c_str());
}
static void cb_refresh(Fl_Widget *, void *) {
    status->label("Looking for networks..."); win->cursor(FL_CURSOR_WAIT); Fl::check();
    run("iwctl station " + dev + " scan"); run("sleep 3");
    fill(); win->cursor(FL_CURSOR_DEFAULT);
}
static void cb_connect(Fl_Widget *, void *) {
    int i = list->value(); if (i < 1) { fl_message("Pick a network first."); return; }
    Net &n = nets[i - 1];
    std::string cmd = "iwctl";
    if (n.security != "open" && !n.known) {
        const char *pw = fl_password("Password for %s:", "", n.name.c_str());
        if (!pw) return;
        cmd += " --passphrase=" + q(pw);
    }
    static std::string st; st = "Connecting to " + n.name + "..."; status->label(st.c_str());
    win->cursor(FL_CURSOR_WAIT); Fl::check();
    std::string out = strip(run(cmd + " station " + dev + " connect " + q(n.name)));
    win->cursor(FL_CURSOR_DEFAULT);
    if (out.find("Operation failed") != std::string::npos || out.find("Invalid") != std::string::npos ||
        out.find("not found") != std::string::npos || out.find("Aborted") != std::string::npos)
    {
        if (out.find("Invalid") != std::string::npos || out.find("passphrase") != std::string::npos)
            fl_alert("Couldn't connect to %s.\nThe password seems to be wrong. Try again.", n.name.c_str());
        else
            fl_alert("Couldn't connect to %s.\nIf the password is right, wait a few seconds and try again.\n(What went wrong is in /var/log/iwd.log)", n.name.c_str());
    }
    run("sleep 1"); fill();
}
static void cb_disconnect(Fl_Widget *, void *) { run("iwctl station " + dev + " disconnect"); fill(); }
static void cb_forget(Fl_Widget *, void *) {
    int i = list->value(); if (i < 1) { fl_message("Pick a network first."); return; }
    Net &n = nets[i - 1];
    if (fl_choice("Forget %s? You'll need its password again.", "Cancel", "Forget", NULL, n.name.c_str()) != 1) return;
    run("iwctl known-networks " + q(n.name) + " forget"); fill();
}
static void cb_pick(Fl_Widget *, void *) { if (Fl::event_clicks()) cb_connect(0, 0); }

int main(int argc, char **argv) {
    tinyc_style();
    dev = find_dev();
    win = new Fl_Double_Window(460, 380, "Wi-Fi");
    status = new Fl_Box(10, 8, 440, 26); status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    list = new Fl_Hold_Browser(10, 38, 440, 290);
    static int widths[] = { 230, 50, 66, 0 };
    list->column_widths(widths); list->column_char('\t'); list->callback(cb_pick); list->selection_color(fl_rgb_color(0x26, 0x74, 0xd8));
    Fl_Button *br = new Fl_Button(10, 340, 90, 30, "&Refresh"); br->callback(cb_refresh);
    bconnect = new Fl_Button(108, 340, 100, 30, "&Connect"); bconnect->callback(cb_connect);
    Fl_Button *bd = new Fl_Button(216, 340, 110, 30, "&Disconnect"); bd->callback(cb_disconnect);
    bforget = new Fl_Button(334, 340, 116, 30, "&Forget"); bforget->callback(cb_forget);
    win->resizable(list);
    win->end();
    { static Fl_RGB_Image ic(icon_wifi, 32, 32, 4); win->icon(&ic); }
    win->show(argc, argv);
    if (dev.empty()) {
        status->label("No Wi-Fi card found (or its driver isn't loaded).");
        list->deactivate(); br->deactivate(); bconnect->deactivate(); bd->deactivate(); bforget->deactivate();
    } else { Fl::check(); cb_refresh(0, 0); }
    return Fl::run();
}
