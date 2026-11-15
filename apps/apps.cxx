// tinyc Apps - find, install and remove apps (Alpine packages, through apk)
#include "tinyc_ui.h"
#include "tinyc_icons.h"
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Progress.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <glob.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

struct Pkg { std::string name, ver, desc, origin; long size = 0, isize = 0; };
static std::map<std::string, Pkg> idx;               // everything in the repositories
static std::set<std::string> installed, world, base;
struct Pop { const char *group, *name, *what; };
static const Pop popular[] = {
    {"Internet", "firefox", "Web browser"},
    {"Internet", "netsurf", "Small, fast web browser"},
    {"Internet", "dillo", "Tiny web browser, for very old computers"},
    {"Internet", "claws-mail", "E-mail"},
    {"Internet", "pidgin", "Chat on many services"},
    {"Internet", "transmission-gtk", "Download torrents"},
    {"Internet", "filezilla", "Send files to servers (FTP)"},
    {"Internet", "telegram-desktop", "Telegram messenger"},
    {"Music and videos", "mpv", "Video player, small"},
    {"Music and videos", "vlc-qt", "VLC video player"},
    {"Music and videos", "audacious", "Music player"},
    {"Music and videos", "audacity", "Record and edit sound"},
    {"Music and videos", "guvcview", "Webcam"},
    {"Pictures", "gpicview", "Picture viewer"},
    {"Pictures", "feh", "Picture viewer, tiny"},
    {"Pictures", "gimp", "Edit photos"},
    {"Pictures", "inkscape", "Draw vector pictures"},
    {"Office", "abiword", "Word processor"},
    {"Office", "gnumeric", "Spreadsheets"},
    {"Office", "libreoffice", "Full office suite (big)"},
    {"Office", "atril", "PDF viewer"},
    {"Office", "galculator", "Calculator"},
    {"Office", "xarchiver", "Zip and unzip archives"},
    {"Office", "keepassxc", "Password manager"},
    {"Coding", "geany", "Code editor"},
    {"Coding", "mousepad", "Text editor with more features"},
    {"Coding", "python3", "Python"},
    {"Coding", "git", "Git"},
    {"Games", "supertux", "Jump'n'run game with Tux"},
    {"Games", "supertuxkart", "Kart racing"},
    {"Games", "gnome-mines", "Minesweeper"},
    {"Games", "gnome-chess", "Chess"},
    {"Games", "openttd", "Build a transport empire"},
    {"Games", "luanti", "Block-building game"},
    {"Tools", "htop", "See running programs (Terminal)"},
    {"Tools", "fastfetch", "Show system info (Terminal)"},
    {"Tools", "mc", "File manager for the Terminal"},
    {"Tools", "vim", "Vim text editor (Terminal)"},
};

static Fl_Double_Window *win;
static Fl_Input *search;
static Fl_Round_Button *rb_pop, *rb_all, *rb_inst;
static Fl_Hold_Browser *list;
static Fl_Box *info, *statusb;
static Fl_Button *binst, *brem;
static Fl_Progress *prog;
static std::vector<std::string> shown;               // package name per list line ("" = heading)
static FILE *job; static std::string job_out, job_what, job_name; static bool job_add;

static std::string mb(long b) {
    char s[32];
    if (b >= 1024 * 1024) snprintf(s, sizeof s, "%.1f MB", b / 1048576.0);
    else snprintf(s, sizeof s, "%ld KB", (b + 1023) / 1024);
    return s;
}
static std::string run(const std::string &cmd) {
    std::string out; FILE *p = popen((cmd + " 2>&1").c_str(), "r");
    if (!p) return out;
    char b[1024]; while (fgets(b, sizeof b, p)) out += b;
    pclose(p); return out;
}
static void read_records(FILE *f, bool into_idx) {
    char line[4096]; Pkg p;
    auto flush = [&]() {
        if (!p.name.empty()) { if (into_idx) idx[p.name] = p; else installed.insert(p.name); }
        p = Pkg();
    };
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line); if (n && line[n - 1] == '\n') line[--n] = 0;
        if (!n) { flush(); continue; }
        if (n < 2 || line[1] != ':') continue;
        const char *v = line + 2;
        switch (line[0]) {
            case 'P': p.name = v; break;
            case 'V': p.ver = v; break;
            case 'T': p.desc = v; break;
            case 'S': p.size = atol(v); break;
            case 'I': p.isize = atol(v); break;
            case 'o': p.origin = v; break;
        }
    }
    flush();
}
static void load_index() {
    idx.clear();
    glob_t g; if (glob("/var/cache/apk/APKINDEX.*.tar.gz", 0, NULL, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++) {
            std::string c = std::string("tar xzOf '") + g.gl_pathv[i] + "' APKINDEX 2>/dev/null";
            FILE *f = popen(c.c_str(), "r"); if (f) { read_records(f, true); pclose(f); }
        }
        globfree(&g);
    }
}
static void load_installed() {
    installed.clear(); world.clear();
    FILE *f = fopen("/lib/apk/db/installed", "r"); if (f) { read_records(f, false); fclose(f); }
    char w[256];
    if ((f = fopen("/etc/apk/world", "r"))) { while (fscanf(f, "%255s", w) == 1) world.insert(w); fclose(f); }
    if (base.empty() && (f = fopen("/etc/tinyc/base-world", "r"))) { while (fscanf(f, "%255s", w) == 1) base.insert(w); fclose(f); }
}
static bool is_app(const std::string &n, const Pkg &p) {     // hide libraries and the parts of packages
    static const char *skip_end[] = {"-dev", "-doc", "-dbg", "-lang", "-openrc", "-static", "-libs", "-pyc",
        "-bash-completion", "-zsh-completion", "-fish-completion", "-tests", "-common", "-data", "-systemd", "-udev", 0};
    static const char *skip_start[] = {"lib", "py3-", "perl-", "ruby-", "lua5", "lua-", "font-", "php", "nodejs-", "rust-",
        "go-", "R-", "ocaml", "ghc", "haskell", "linux-firmware", "linux-", "xf86-", "musl", "gst", "qt5-", "qt6-", "kf5", "kf6", 0};
    if (!p.origin.empty() && p.origin != n) return false;
    for (int i = 0; skip_end[i]; i++) { size_t l = strlen(skip_end[i]); if (n.size() > l && !n.compare(n.size() - l, l, skip_end[i])) return false; }
    for (int i = 0; skip_start[i]; i++) if (!n.compare(0, strlen(skip_start[i]), skip_start[i])) return false;
    return true;
}
static std::string lower(std::string s) { for (auto &c : s) c = tolower((unsigned char)c); return s; }
static void add_line(const std::string &name, const std::string &what) {
    const Pkg &p = idx[name];
    bool inst = installed.count(name);
    std::string l = std::string(inst ? "@b" : "") + "@." + (inst ? "\xe2\x9c\x94 " : "   ") + name + "\t@." + what + "\t@." + (p.size ? mb(p.size) : "");
    list->add(l.c_str()); shown.push_back(name);
}
static void heading(const std::string &h) { list->add(("@B16@C255@b@." + h).c_str()); shown.push_back(""); }
static void cb_select(Fl_Widget *, void *);
static void fill() {
    list->clear(); shown.clear();
    std::string q = lower(search->value());
    if (rb_pop->value() && q.empty()) {
        const char *g = "";
        for (auto &e : popular) {
            if (!idx.count(e.name)) continue;
            if (strcmp(g, e.group)) { heading(e.group); g = e.group; }
            add_line(e.name, e.what);
        }
    } else {
        std::vector<std::string> hit;
        for (auto &kv : idx) {
            const std::string &n = kv.first; const Pkg &p = kv.second;
            if (rb_inst->value() ? !(world.count(n) && !base.count(n)) : !is_app(n, p)) continue;
            if (!q.empty() && lower(n).find(q) == std::string::npos && lower(p.desc).find(q) == std::string::npos) continue;
            hit.push_back(n);
        }
        // names that start with the search first
        std::stable_sort(hit.begin(), hit.end(), [&](const std::string &a, const std::string &b) {
            return (lower(a).rfind(q, 0) == 0) > (lower(b).rfind(q, 0) == 0); });
        size_t max = 400;
        for (size_t i = 0; i < hit.size() && i < max; i++) add_line(hit[i], idx[hit[i]].desc);
        static std::string st;
        if (hit.empty()) st = rb_inst->value() ? "You haven't installed any apps yet." : "Nothing found for \"" + q + "\".";
        else if (hit.size() > max) st = std::to_string(hit.size()) + " found, showing the first " + std::to_string(max) + ". Type more to narrow it down.";
        else st = std::to_string(hit.size()) + " found.";
        statusb->label(st.c_str());
    }
    if (rb_pop->value() && q.empty()) statusb->label("Popular apps. Search to find thousands more.");
    info->label("Click an app to see more.");
    binst->deactivate(); brem->deactivate();
}
static void cb_fill(Fl_Widget *, void *) { if (!job) fill(); }
static std::string sel_name() { int i = list->value(); return i < 1 || i > (int)shown.size() ? "" : shown[i - 1]; }
static void cb_select(Fl_Widget *, void *) {
    std::string n = sel_name();
    if (n.empty()) { binst->deactivate(); brem->deactivate(); return; }
    const Pkg &p = idx[n];
    static std::string t;
    t = n + "  -  " + p.desc + "\nVersion " + p.ver;
    if (installed.count(n)) {
        t += "  \xc2\xb7  Installed";
        binst->deactivate();
        if (world.count(n) && !base.count(n)) brem->activate(); else brem->deactivate();
        if (base.count(n)) t += " (part of tinyc)";
    } else {
        // what apk would install for it, and how big the download is
        std::string s = run("apk add --simulate " + n);
        long total = 0; int count = 0;
        size_t pos = 0;
        while ((pos = s.find("Installing ", pos)) != std::string::npos) {
            pos += 11; size_t e = s.find(' ', pos); std::string dep = s.substr(pos, e - pos);
            if (idx.count(dep)) total += idx[dep].size;
            count++;
        }
        if (count) t += "  \xc2\xb7  Download " + mb(total) + (count > 1 ? " (with " + std::to_string(count - 1) + " other part" + (count > 2 ? "s" : "") + " it needs)" : "");
        binst->activate(); brem->deactivate();
    }
    info->label(t.c_str());
    if (Fl::event_clicks() && binst->active()) binst->do_callback();
}
static void job_line(const std::string &l) {
    int a, b;
    const char *p = strchr(l.c_str(), '(');
    if (p && sscanf(p, "(%d/%d)", &a, &b) == 2 && b > 0) {
        prog->value(100.0f * a / b);
        static std::string st; st = job_what + " " + job_name + "... (" + std::to_string(a) + " of " + std::to_string(b) + ")";
        statusb->label(st.c_str());
    }
}
static void job_read(int fd, void *) {
    char b[1024]; ssize_t n = read(fd, b, sizeof b);
    if (n > 0) {
        job_out.append(b, n);
        size_t e; static size_t done = 0;
        while ((e = job_out.find('\n', done)) != std::string::npos) { job_line(job_out.substr(done, e - done)); done = e + 1; }
        return;
    }
    Fl::remove_fd(fd);
    int st = pclose(job); job = NULL;
    win->cursor(FL_CURSOR_DEFAULT);
    prog->hide();
    load_installed();
    bool ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
    static std::string m;
    if (ok && job_add) m = "Installed " + job_name + ". Find it in the /o\\ menu under Programs.";
    else if (ok) m = "Removed " + job_name + ".";
    else {
        size_t e = job_out.rfind("ERROR"); std::string why = e == std::string::npos ? "" : job_out.substr(e, job_out.find('\n', e) - e);
        if (job_out.find("network") != std::string::npos || job_out.find("fetch") != std::string::npos || job_out.find("DNS") != std::string::npos)
            why = "Is the internet connected? (Wi-Fi in the /o\\ menu)";
        else if (job_out.find("No space") != std::string::npos) why = "There isn't enough room left on the stick.";
        fl_alert("Couldn't %s %s.\n%s", job_add ? "install" : "remove", job_name.c_str(), why.c_str());
        m = std::string(job_add ? "Installing " : "Removing ") + job_name + " didn't work.";
    }
    int keep = list->value();
    fill(); if (keep > 0 && keep <= list->size()) { list->value(keep); cb_select(0, 0); }
    statusb->label(m.c_str());
    search->activate(); list->activate(); rb_pop->activate(); rb_all->activate(); rb_inst->activate();
}
static void start_job(bool add, const std::string &n) {
    job_add = add; job_name = n; job_what = add ? "Installing" : "Removing"; job_out.clear();
    std::string cmd = std::string(add ? "apk add -U " : "apk del ") + n + " 2>&1";
    job = popen(cmd.c_str(), "r");
    if (!job) { fl_alert("Couldn't start apk."); return; }
    search->deactivate(); list->deactivate(); rb_pop->deactivate(); rb_all->deactivate(); rb_inst->deactivate();
    binst->deactivate(); brem->deactivate();
    prog->value(0); prog->show(); win->cursor(FL_CURSOR_WAIT);
    static std::string st; st = job_what + " " + n + "..."; statusb->label(st.c_str());
    Fl::add_fd(fileno(job), job_read);
}
static void cb_install(Fl_Widget *, void *) { std::string n = sel_name(); if (!n.empty()) start_job(true, n); }
static void cb_remove(Fl_Widget *, void *) {
    std::string n = sel_name(); if (n.empty()) return;
    if (fl_choice("Remove %s?", "Cancel", "Remove", NULL, n.c_str()) == 1) start_job(false, n);
}
static void get_list() {
    statusb->label("Getting the list of apps from the internet..."); win->cursor(FL_CURSOR_WAIT); Fl::check();
    std::string out = run("apk update");
    win->cursor(FL_CURSOR_DEFAULT);
    load_index();
    if (idx.empty()) fl_alert("Couldn't get the list of apps.\nIs the internet connected? (Wi-Fi is in the /o\\ menu)");
}
static void cb_refresh(Fl_Widget *, void *) { if (job) return; get_list(); load_installed(); fill(); }

int main(int argc, char **argv) {
    tinyc_style();
    Fl::set_color((Fl_Color)16, 0x26, 0x74, 0xd8);          // headings: the ccos blue
    win = new Fl_Double_Window(760, 540, "Apps");
    new Fl_Box(10, 10, 56, 28, "Search:");
    search = new Fl_Input(70, 10, 330, 28); search->when(FL_WHEN_CHANGED | FL_WHEN_ENTER_KEY); search->callback(cb_fill);
    Fl_Group *g = new Fl_Group(410, 10, 340, 28);
    rb_pop = new Fl_Round_Button(410, 10, 90, 28, "Popular"); rb_pop->type(FL_RADIO_BUTTON); rb_pop->value(1); rb_pop->callback(cb_fill);
    rb_all = new Fl_Round_Button(500, 10, 80, 28, "All apps"); rb_all->type(FL_RADIO_BUTTON); rb_all->callback(cb_fill);
    rb_inst = new Fl_Round_Button(585, 10, 100, 28, "Installed"); rb_inst->type(FL_RADIO_BUTTON); rb_inst->callback(cb_fill);
    Fl_Button *br = new Fl_Button(690, 10, 60, 28, "@reload"); br->tooltip("Get the newest list of apps"); br->callback(cb_refresh);
    g->end();
    list = new Fl_Hold_Browser(10, 46, 740, 370);
    static int widths[] = { 200, 440, 0 };
    list->column_widths(widths); list->column_char('\t'); list->callback(cb_select); list->selection_color(fl_rgb_color(0x26, 0x74, 0xd8));
    info = new Fl_Box(10, 422, 740, 46); info->box(FL_THIN_DOWN_BOX); info->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP | FL_ALIGN_TOP);
    binst = new Fl_Button(540, 474, 100, 30, "&Install"); binst->callback(cb_install);
    brem = new Fl_Button(650, 474, 100, 30, "&Remove"); brem->callback(cb_remove);
    prog = new Fl_Progress(10, 478, 300, 22); prog->selection_color(fl_rgb_color(0x26, 0x74, 0xd8)); prog->hide();
    statusb = new Fl_Box(0, 512, 760, 28); statusb->box(FL_THIN_UP_BOX); statusb->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    win->resizable(list);
    win->end();
    { static Fl_RGB_Image ic(icon_apps, 32, 32, 4); win->icon(&ic); }
    win->show(argc, argv);
    Fl::check();
    load_index(); load_installed();
    if (idx.empty()) get_list();
    fill();
    return Fl::run();
}
