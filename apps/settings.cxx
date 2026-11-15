// tinyc Settings - time, keyboard, wallpaper, screen brightness, about.   settings [time|keyboard|desktop|screen|about]
// Saves to /etc/tinyc/settings (kept on the stick) and puts each change into effect with tinyc-settings-apply.
#include "tinyc_ui.h"
#include "tinyc_icons.h"
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Hor_Value_Slider.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>

static const char *SETTINGS = "/etc/tinyc/settings";
static std::map<std::string, std::string> cfg;
static std::vector<std::string> cfg_order;

static void load_cfg() {
    FILE *f = fopen(SETTINGS, "r"); if (!f) return;
    char l[1024];
    while (fgets(l, sizeof l, f)) {
        l[strcspn(l, "\n")] = 0;
        if (l[0] == '#' || !strchr(l, '=')) continue;
        std::string k(l, strchr(l, '=') - l), v(strchr(l, '=') + 1);
        if (!cfg.count(k)) cfg_order.push_back(k);
        cfg[k] = v;
    }
    fclose(f);
}
static void save_cfg(const char *apply) {
    FILE *f = fopen(SETTINGS, "w");
    if (!f) { fl_alert("Couldn't save the settings."); return; }
    fprintf(f, "# tinyc settings (the Settings window changes these; kept on the stick)\n");
    for (auto &k : cfg_order) fprintf(f, "%s=%s\n", k.c_str(), cfg[k].c_str());
    fclose(f);
    if (apply) { std::string c = std::string("tinyc-settings-apply ") + apply + " >/dev/null 2>&1"; system(c.c_str()); }
}
static void set(const std::string &k, const std::string &v) { if (!cfg.count(k)) cfg_order.push_back(k); cfg[k] = v; }
static std::string get(const std::string &k, const std::string &d = "") { return cfg.count(k) ? cfg[k] : d; }
static Fl_Color blue() { return fl_rgb_color(0x26, 0x74, 0xd8); }

// ---------------------------------------------------------------- Time
static Fl_Box *t_now;
static Fl_Input *t_search;
static Fl_Hold_Browser *t_list;
static std::vector<std::string> zones;
static void walk(const std::string &base, const std::string &rel) {
    DIR *d = opendir((base + "/" + rel).c_str()); if (!d) return;
    while (struct dirent *e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        std::string r = rel.empty() ? e->d_name : rel + "/" + e->d_name;
        struct stat st; if (stat((base + "/" + r).c_str(), &st)) continue;
        if (S_ISDIR(st.st_mode)) { if (strcmp(e->d_name, "posix") && strcmp(e->d_name, "right") && strcmp(e->d_name, "Etc")) walk(base, r); }
        else if (r.find('/') != std::string::npos || r == "UTC") {
            static const char *ok[] = {"Africa/", "America/", "Antarctica/", "Asia/", "Atlantic/", "Australia/", "Europe/", "Indian/", "Pacific/", "UTC", 0};
            for (int i = 0; ok[i]; i++) if (!r.compare(0, strlen(ok[i]), ok[i])) { zones.push_back(r); break; }
        }
    }
    closedir(d);
}
static void tick(void *) {
    char b[64]; time_t t = time(0); strftime(b, sizeof b, "%H:%M:%S   %A %d %B %Y", localtime(&t));
    static std::string s; s = std::string("Now: ") + b; t_now->label(s.c_str());
    Fl::repeat_timeout(1.0, tick);
}
static void fill_zones() {
    std::string q = t_search->value(); for (auto &c : q) c = tolower((unsigned char)c);
    std::string cur = get("TIMEZONE", "UTC");
    t_list->clear(); int sel = 0;
    for (auto &z : zones) {
        std::string lz = z; for (auto &c : lz) c = tolower((unsigned char)c);
        if (!q.empty() && lz.find(q) == std::string::npos) continue;
        std::string shown = z; for (auto &c : shown) if (c == '_') c = ' ';
        t_list->add(shown.c_str(), (void *)&z);
        if (z == cur) sel = t_list->size();
    }
    if (sel) { t_list->value(sel); t_list->middleline(sel); }
}
static void cb_zone(Fl_Widget *, void *) {
    int i = t_list->value(); if (i < 1) return;
    set("TIMEZONE", *(std::string *)t_list->data(i)); save_cfg("time");
    setenv("TZ", ("/usr/share/zoneinfo/" + get("TIMEZONE")).c_str(), 1); tzset();
}
static void cb_nettime(Fl_Widget *w, void *) { set("NET_TIME", ((Fl_Check_Button *)w)->value() ? "1" : "0"); save_cfg("time"); }
static void cb_rtc(Fl_Widget *w, void *) {
    set("RTC_LOCAL", ((Fl_Check_Button *)w)->value() ? "1" : "0"); save_cfg(NULL);
    system(get("RTC_LOCAL") == "1" ? "hwclock -w -l >/dev/null 2>&1" : "hwclock -w -u >/dev/null 2>&1");
}
static Fl_Group *tab_time(int x, int y, int w, int h) {
    Fl_Group *g = new Fl_Group(x, y, w, h, " Time ");
    t_now = new Fl_Box(x + 10, y + 10, w - 20, 26); t_now->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE); t_now->labelfont(FL_HELVETICA_BOLD);
    new Fl_Box(x + 10, y + 42, 90, 26, "Time zone:");
    t_search = new Fl_Input(x + 100, y + 42, w - 110, 26); t_search->tooltip("Type a city, like Warsaw or London");
    t_search->when(FL_WHEN_CHANGED); t_search->callback([](Fl_Widget *, void *) { fill_zones(); });
    t_list = new Fl_Hold_Browser(x + 10, y + 74, w - 20, h - 150); t_list->callback(cb_zone); t_list->selection_color(blue());
    Fl_Check_Button *n = new Fl_Check_Button(x + 10, y + h - 70, w - 20, 26, "Set the time from the internet");
    n->value(get("NET_TIME", "1") == "1"); n->callback(cb_nettime);
    Fl_Check_Button *r = new Fl_Check_Button(x + 10, y + h - 42, w - 20, 26, "Windows is on this computer too (keeps the time right in both)");
    r->value(get("RTC_LOCAL", "0") == "1"); r->callback(cb_rtc);
    g->end(); g->resizable(t_list);
    walk("/usr/share/zoneinfo", ""); std::sort(zones.begin(), zones.end());
    fill_zones();
    Fl::add_timeout(0.0, tick);
    return g;
}

// ---------------------------------------------------------------- Keyboard
struct Layout { const char *name, *code; };
static const Layout layouts[] = {
    {"Polish (programmer's)", "pl"}, {"English (US)", "us"}, {"English (UK)", "gb"}, {"German", "de"}, {"French", "fr"},
    {"Spanish", "es"}, {"Italian", "it"}, {"Portuguese", "pt"}, {"Portuguese (Brazil)", "br"}, {"Dutch", "nl"},
    {"Belgian", "be"}, {"Swiss", "ch"}, {"Czech", "cz"}, {"Slovak", "sk"}, {"Hungarian", "hu"}, {"Romanian", "ro"},
    {"Ukrainian", "ua"}, {"Russian", "ru"}, {"Lithuanian", "lt"}, {"Latvian", "lv"}, {"Estonian", "ee"},
    {"Swedish", "se"}, {"Norwegian", "no"}, {"Danish", "dk"}, {"Finnish", "fi"}, {"Greek", "gr"}, {"Turkish", "tr"},
    {"Canadian (French)", "ca"}, {"Japanese", "jp"}, {0, 0}};
static Fl_Hold_Browser *k_list;
static void cb_layout(Fl_Widget *, void *) {
    int i = k_list->value(); if (i < 1) return;
    set("KEYBOARD", layouts[i - 1].code); save_cfg("keyboard");
}
static Fl_Group *tab_keyboard(int x, int y, int w, int h) {
    Fl_Group *g = new Fl_Group(x, y, w, h, " Keyboard ");
    Fl_Box *b = new Fl_Box(x + 10, y + 10, w - 20, 26, "Keyboard layout (it changes straight away):"); b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    k_list = new Fl_Hold_Browser(x + 10, y + 40, w - 20, h - 120); k_list->callback(cb_layout); k_list->selection_color(blue());
    std::string cur = get("KEYBOARD", "us");
    for (int i = 0; layouts[i].name; i++) { k_list->add(layouts[i].name); if (cur == layouts[i].code) k_list->value(i + 1); }
    Fl_Box *t = new Fl_Box(x + 10, y + h - 72, w - 20, 26, "Try it here:"); t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    new Fl_Input(x + 10, y + h - 44, w - 20, 28);
    g->end(); g->resizable(k_list);
    return g;
}

// ---------------------------------------------------------------- Desktop (wallpaper)
static const char *WALLS = "/usr/share/tinyc/wallpapers";
struct Wall { std::string name, file, thumb; };
static std::vector<Wall> walls;
static std::vector<Fl_Button *> wbtn;
static void mark_walls() {
    std::string cur = get("WALLPAPER");
    for (size_t i = 0; i < wbtn.size(); i++) {
        bool on = walls[i].file == cur;
        wbtn[i]->box(on ? FL_BORDER_BOX : FL_FLAT_BOX); wbtn[i]->color(on ? blue() : FL_BACKGROUND_COLOR);
        wbtn[i]->labelfont(on ? FL_HELVETICA_BOLD : FL_HELVETICA); wbtn[i]->redraw();
    }
}
static void cb_wall(Fl_Widget *, void *d) {
    set("WALLPAPER", walls[(long)d].file); save_cfg("wallpaper"); mark_walls();
}
static void cb_own(Fl_Widget *, void *) {
    const char *h = getenv("HOME"); std::string start = std::string(h ? h : "/root") + "/";
    const char *p = fl_file_chooser("Choose a picture", "Pictures (*.{jpg,jpeg,png})", start.c_str());
    if (!p) return;
    set("WALLPAPER", p); save_cfg("wallpaper"); mark_walls();
}
static Fl_Group *tab_desktop(int x, int y, int w, int h) {
    Fl_Group *g = new Fl_Group(x, y, w, h, " Desktop ");
    Fl_Box *b = new Fl_Box(x + 10, y + 10, w - 20, 26, "Desktop background:"); b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    DIR *d = opendir(WALLS);
    if (d) {
        while (struct dirent *e = readdir(d)) {
            std::string n = e->d_name; if (n.size() < 5 || n.compare(n.size() - 4, 4, ".jpg")) continue;
            std::string base = n.substr(0, n.size() - 4), pretty = base;
            for (auto &c : pretty) if (c == '-') c = ' ';
            if (!pretty.empty()) pretty[0] = toupper((unsigned char)pretty[0]);
            walls.push_back({pretty, std::string(WALLS) + "/" + n, std::string(WALLS) + "/" + base + "-small.png"});
        }
        closedir(d);
    }
    std::sort(walls.begin(), walls.end(), [](const Wall &a, const Wall &b) { return a.name < b.name; });
    int cw = 150, ch = 112, cols = (w - 20) / (cw + 10);
    for (size_t i = 0; i < walls.size(); i++) {
        int cx = x + 10 + (int)(i % cols) * (cw + 10), cy = y + 42 + (int)(i / cols) * (ch + 10);
        Fl_Button *bt = new Fl_Button(cx, cy, cw, ch);
        bt->copy_label(walls[i].name.c_str());
        Fl_PNG_Image *im = new Fl_PNG_Image(walls[i].thumb.c_str());
        if (im->w() > 0) { Fl_Image *s = im->copy(cw - 12, (cw - 12) * im->h() / im->w()); bt->image(s); }
        bt->align(FL_ALIGN_IMAGE_OVER_TEXT | FL_ALIGN_INSIDE);
        bt->callback(cb_wall, (void *)(long)i);
        wbtn.push_back(bt);
    }
    Fl_Button *own = new Fl_Button(x + 10, y + h - 40, 200, 30, "Use my own picture..."); own->callback(cb_own);
    g->end();
    mark_walls();
    return g;
}

// ---------------------------------------------------------------- Screen
static std::string backlight;
static void cb_bright(Fl_Widget *w, void *) {
    int pct = (int)((Fl_Hor_Value_Slider *)w)->value();
    set("BRIGHTNESS", std::to_string(pct));
    if (Fl::event() == FL_RELEASE || Fl::event() == FL_KEYDOWN) save_cfg(NULL);
    system(("tinyc-brightness set " + std::to_string(pct) + " >/dev/null 2>&1").c_str());
}
static Fl_Group *tab_screen(int x, int y, int w, int h) {
    Fl_Group *g = new Fl_Group(x, y, w, h, " Screen ");
    DIR *d = opendir("/sys/class/backlight");
    if (d) { while (struct dirent *e = readdir(d)) if (e->d_name[0] != '.') { backlight = e->d_name; break; } closedir(d); }
    if (backlight.empty()) {
        Fl_Box *b = new Fl_Box(x + 10, y + 10, w - 20, 60, "This screen's brightness can't be changed from tinyc\n(on a desktop computer, use the monitor's own buttons).");
        b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    } else {
        Fl_Box *b = new Fl_Box(x + 10, y + 10, w - 20, 26, "Brightness (the brightness keys work too):"); b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Hor_Value_Slider *s = new Fl_Hor_Value_Slider(x + 10, y + 42, w - 20, 28);
        s->bounds(5, 100); s->step(5); s->precision(0); s->selection_color(blue());
        int cur = 100;
        FILE *f = popen("tinyc-brightness get", "r"); if (f) { if (fscanf(f, "%d", &cur) != 1) cur = 100; pclose(f); }
        s->value(cur); s->callback(cb_bright); s->when(FL_WHEN_CHANGED | FL_WHEN_RELEASE);
    }
    static std::string r; r = "Screen size: " + std::to_string(Fl::w()) + " x " + std::to_string(Fl::h()) + " pixels";
    Fl_Box *rb = new Fl_Box(x + 10, y + 90, w - 20, 26, r.c_str()); rb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    g->end();
    return g;
}

// ---------------------------------------------------------------- About
static std::string first_line(const char *cmd) {
    char b[256] = ""; FILE *f = popen(cmd, "r");
    if (f) { if (!fgets(b, sizeof b, f)) b[0] = 0; pclose(f); }
    b[strcspn(b, "\n")] = 0; return b;
}
static std::string human(double kb) {
    char s[32];
    if (kb >= 1048576) snprintf(s, sizeof s, "%.1f GB", kb / 1048576); else snprintf(s, sizeof s, "%.0f MB", kb / 1024);
    return s;
}
static Fl_Group *tab_about(int x, int y, int w, int h) {
    Fl_Group *g = new Fl_Group(x, y, w, h, " About ");
    static Fl_RGB_Image logo(icon_apps, 32, 32, 4);
    struct utsname u; uname(&u);
    long mt = 0, ma = 0; FILE *f = fopen("/proc/meminfo", "r");
    if (f) { char l[256]; while (fgets(l, sizeof l, f)) { sscanf(l, "MemTotal: %ld", &mt); sscanf(l, "MemAvailable: %ld", &ma); } fclose(f); }
    struct statvfs sv; std::string disk = "?";
    if (!statvfs("/mnt/data", &sv)) disk = human((double)sv.f_bavail * sv.f_frsize / 1024) + " free of " + human((double)sv.f_blocks * sv.f_frsize / 1024);
    std::string cpu = first_line("sed -n 's/^model name[^:]*: *//p' /proc/cpuinfo | head -1");
    static std::string t;
    t = "tinyCcOS " + first_line("cat /etc/tinyc-version") + " (" + std::string(u.machine) + ")\n\n"
        "Linux " + u.release + "\n"
        "Processor: " + (cpu.empty() ? "?" : cpu) + "\n"
        "Memory: " + human(ma) + " free of " + human(mt) + "\n"
        "Your files on the stick: " + disk + "\n\n"
        "tinyCcOS is a tiny version of CcOS, built on Alpine Linux.\n"
        "Help: ccossys.github.io/tinyc/docs.html";
    Fl_Box *b = new Fl_Box(x + 10, y + 10, w - 20, h - 20, t.c_str());
    b->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    g->end();
    return g;
}

int main(int argc, char **argv) {
    tinyc_style();
    fl_register_images();
    load_cfg();
    Fl_Double_Window *win = new Fl_Double_Window(560, 480, "Settings");
    Fl_Tabs *tabs = new Fl_Tabs(5, 5, 550, 470);
    tabs->selection_color(fl_rgb_color(0xff, 0xff, 0xff));
    int x = 5, y = 32, w = 550, h = 443;
    Fl_Group *gt = tab_time(x, y, w, h), *gk = tab_keyboard(x, y, w, h), *gd = tab_desktop(x, y, w, h),
             *gs = tab_screen(x, y, w, h), *ga = tab_about(x, y, w, h);
    tabs->end();
    if (argc > 1) {
        std::string a = argv[1];
        tabs->value(a == "keyboard" ? gk : a == "desktop" ? gd : a == "screen" ? gs : a == "about" ? ga : gt);
    }
    win->resizable(tabs);
    win->end();
    static Fl_RGB_Image ic(icon_settings, 32, 32, 4); win->icon(&ic);
    win->show();
    return Fl::run();
}
