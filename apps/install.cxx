// Install tinyc - puts tinyc on the computer's own disk: next to Windows, in empty space, or on the whole disk.
// The real work is done by /usr/sbin/tinyc-install (scan / run); this is the window around it.
#include "tinyc_ui.h"
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Hor_Value_Slider.H>
#include <FL/Fl_Progress.H>
#include <FL/fl_draw.H>
#include <FL/Fl_PNG_Image.H>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <unistd.h>

static const int W = 600, H = 470;
static const Fl_Color BLUE = fl_rgb_color(0x26, 0x74, 0xd8), GREEN = fl_rgb_color(0x1e, 0x9e, 0x4a),
                      RED = fl_rgb_color(0xd8, 0x3a, 0x3a), GREY = fl_rgb_color(0x6a, 0x72, 0x7e);

// ------------------------------------------------------------- what the scan found
struct Win { std::string part; long size = 0, used = 0, keep = 0, room = 0; std::string state, msg; };
struct Free { long start = 0, size = 0; std::string problem; };
struct Disk { std::string name, table, model, problem; long size = 0; std::vector<Win> wins; std::vector<Free> frees; };
static std::string firmware, bits, secureboot, powerstate, source;
static std::vector<Disk> disks;

static std::vector<std::string> split(const std::string &l) {
    std::vector<std::string> v; size_t a = 0, b;
    while ((b = l.find('|', a)) != std::string::npos) { v.push_back(l.substr(a, b - a)); a = b + 1; }
    v.push_back(l.substr(a)); return v;
}
static Disk *find_disk(const std::string &n) { for (auto &d : disks) if (d.name == n) return &d; return nullptr; }
static void parse_line(const std::string &l) {
    auto f = split(l);
    if (f[0] == "sys" && f.size() >= 6) { firmware = f[1]; bits = f[2]; secureboot = f[3]; powerstate = f[4]; source = f[5]; }
    else if (f[0] == "disk" && f.size() >= 6) { Disk d; d.name = f[1]; d.size = atol(f[2].c_str()); d.table = f[3]; d.model = f[4]; d.problem = f[5]; disks.push_back(d); }
    else if (f[0] == "win" && f.size() >= 9) {
        Disk *d = find_disk(f[1]); if (!d) return;
        Win w; w.part = f[2]; w.size = atol(f[3].c_str()); w.used = atol(f[4].c_str()); w.keep = atol(f[5].c_str());
        w.room = atol(f[6].c_str()); w.state = f[7]; w.msg = f[8]; d->wins.push_back(w);
    } else if (f[0] == "free" && f.size() >= 6) {
        Disk *d = find_disk(f[1]); if (!d) return;
        Free r; r.start = atol(f[2].c_str()); r.size = atol(f[4].c_str()); r.problem = f[5]; d->frees.push_back(r);
    }
}
static std::string gb(long mb) {
    char b[32]; if (mb >= 10240) snprintf(b, sizeof b, "%ld GB", (mb + 512) / 1024); else snprintf(b, sizeof b, "%.1f GB", mb / 1024.0);
    return b;
}

// ------------------------------------------------------------- the window
static Fl_Double_Window *win;
static Fl_Group *pages[6];
static Fl_Box *title;
static Fl_Button *b_back, *b_next, *b_cancel;
static int page = 0;
// page 1
static Fl_Box *chk_icon[4], *chk_text[4];
static Fl_Check_Button *backed_up;
// page 2
static Fl_Choice *disk_choice;
static Fl_Round_Button *m_along, *m_free, *m_erase;
static Fl_Box *m_along_why, *m_free_why, *m_erase_why;
// page 3
static Fl_Hor_Value_Slider *size_slider;
static Fl_Box *size_info;
// page 4
static Fl_Box *summary;
static Fl_Check_Button *keep_files, *understand;
// page 5
static Fl_Progress *progress;
static Fl_Box *status, *detail;
static Fl_Button *b_restart;

static int cur_disk = 0;
static std::string mode;          // alongside / free / replace
static const Win *sel_win = nullptr;
static const Free *sel_free = nullptr;

static void label(Fl_Box *b, const std::string &s) { b->copy_label(s.c_str()); b->redraw(); }
static Fl_Box *text(int x, int y, int w, int h, const char *s, int size = 13, Fl_Font font = FL_HELVETICA) {
    Fl_Box *b = new Fl_Box(x, y, w, h); if (s) b->copy_label(s);
    b->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE | FL_ALIGN_WRAP); b->labelsize(size); b->labelfont(font); return b;
}

// ------------------------------------------------------------- page 1: before you start
static int n_problems = 0;
static void check_row(int i, int state, const std::string &s) {   // state: 1 ok, 0 problem, 2 warning
    chk_icon[i]->copy_label(state == 1 ? "\u2714" : state == 2 ? "!" : "\u2716");
    chk_icon[i]->labelcolor(state == 1 ? GREEN : state == 2 ? fl_rgb_color(0xd0, 0x8a, 0x00) : RED);
    chk_icon[i]->labelfont(FL_HELVETICA_BOLD); chk_icon[i]->labelsize(16);
    label(chk_text[i], s);
    if (state == 0) n_problems++;
}
static void fill_checks() {
    n_problems = 0;
    if (powerstate == "battery") check_row(0, 2, "Plug the laptop in: running out of power in the middle can damage Windows.");
    else check_row(0, 1, powerstate == "ac" ? "The laptop is plugged in." : "Plugged in (this computer has no battery).");
    if (firmware == "bios") check_row(1, 1, "Secure Boot: not needed on this computer (it starts the old BIOS way).");
    else if (secureboot == "1") check_row(1, 0, "Secure Boot is ON. Turn it off in the computer's setup (BIOS), or tinyc won't start.");
    else check_row(1, 1, "Secure Boot is off.");
    bool any = false, locked = false, hib = false, dirty = false;
    for (auto &d : disks) for (auto &w : d.wins) { any = true; if (w.state == "bitlocker") locked = true; if (w.state == "hibernated") hib = true; if (w.state == "dirty") dirty = true; }
    if (!any) { check_row(2, 1, "BitLocker: no Windows found on this computer."); check_row(3, 1, "Fast Startup: no Windows found on this computer."); }
    else {
        if (locked) check_row(2, 0, "BitLocker (device encryption) is ON. Turn it off in Windows first, or tinyc can only go in empty space.");
        else check_row(2, 1, "BitLocker (device encryption) is off.");
        if (hib) check_row(3, 0, "Windows wasn't fully shut down. Turn off Fast Startup in Windows, then shut down (not restart).");
        else if (dirty) check_row(3, 0, "Windows wants to check its disk. Start Windows once, then shut it down properly.");
        else check_row(3, 1, "Windows was fully shut down (Fast Startup is off).");
    }
}

// ------------------------------------------------------------- page 2: where
static void fill_where() {
    Disk &d = disks[cur_disk];
    sel_win = nullptr; sel_free = nullptr;
    for (auto &w : d.wins) if (!sel_win || w.room > sel_win->room) sel_win = &w;
    for (auto &f : d.frees) if (f.problem.empty() && (!sel_free || f.size > sel_free->size)) sel_free = &f;
    // next to Windows
    std::string why; bool ok = false;
    if (!d.problem.empty() && d.problem.find("only 'erase'") != std::string::npos) why = "Not on this disk: " + d.problem.substr(0, d.problem.find(':')) + ".";
    else if (!sel_win) why = "There's no Windows on this disk.";
    else if (sel_win->state == "ok" && sel_win->keep >= sel_win->size)
        why = "Can't: Windows has no space to spare (it needs all of its " + gb(sel_win->size) + " itself).";
    else if (sel_win->state == "ok") { ok = true; why = "Windows (" + gb(sel_win->size) + ", " + gb(sel_win->used) + " used) is made smaller. tinyc can have up to " + gb(sel_win->room) + ". At start-up you pick tinyc or Windows."; }
    else if (sel_win->state == "bitlocker") why = "Can't: BitLocker is on. Turn it off in Windows first (see Before you start).";
    else if (sel_win->state == "hibernated") why = "Can't: Windows wasn't fully shut down. Turn off Fast Startup, then shut Windows down.";
    else if (sel_win->state == "dirty") why = "Can't: Windows wants to check its disk. Start Windows once and shut it down properly.";
    else why = "Can't: " + (sel_win->msg.empty() ? std::string("Windows can't be made smaller") : sel_win->msg) + ".";
    if (ok) m_along->activate(); else { m_along->deactivate(); m_along->value(0); }
    label(m_along_why, why);
    // empty space
    ok = false;
    if (!d.problem.empty() && d.problem.find("only 'erase'") != std::string::npos) why = "Not on this disk.";
    else if (sel_free) { ok = true; why = gb(sel_free->size) + " of this disk isn't used by anything. Nothing else is changed."; }
    else {
        long big = 0; std::string p; for (auto &f : d.frees) if (f.size > big) { big = f.size; p = f.problem; }
        why = big ? "Can't: the empty space (" + gb(big) + ") is " + p + "." : "There's no empty space on this disk.";
    }
    if (ok) m_free->activate(); else { m_free->deactivate(); m_free->value(0); }
    label(m_free_why, why);
    // the whole disk
    if (d.size < 4096) { m_erase->deactivate(); m_erase->value(0); label(m_erase_why, "This disk is too small for tinyc."); }
    else {
        m_erase->activate();
        std::string w = "Everything on this disk is deleted for good";
        if (!d.wins.empty()) w += ", including Windows and all your files in it";
        label(m_erase_why, w + ". tinyc gets all " + gb(d.size) + ".");
    }
    if (!m_along->value() && !m_free->value() && !m_erase->value()) {
        if (m_along->active()) m_along->setonly(); else if (m_free->active()) m_free->setonly();
    }
}
static void cb_disk(Fl_Widget *, void *) { cur_disk = disk_choice->value(); m_along->value(0); m_free->value(0); m_erase->value(0); fill_where(); }

// ------------------------------------------------------------- page 3: how much space
static void show_size() {
    long t = (long)size_slider->value() * 1024;
    if (mode == "alongside") {
        // Windows keeps whatever tinyc doesn't take (it never grows)
        long keeps = std::min(sel_win->size, sel_win->room + sel_win->keep + 2 - t);
        label(size_info, "tinyc gets " + gb(t) + ".\nWindows keeps " + gb(keeps) + " (" + gb(sel_win->used) + " of it is used now, so "
              + gb(std::max(0L, keeps - sel_win->used)) + " stays free for Windows).");
    } else {
        label(size_info, "tinyc gets " + gb(t) + " of the " + gb(sel_free->size) + " empty space.");
    }
}
static void cb_size(Fl_Widget *, void *) { show_size(); }
static void fill_size() {
    long max = mode == "alongside" ? sel_win->room : sel_free->size;
    long maxg = max / 1024; if (maxg < 4) maxg = 4;
    size_slider->bounds(4, maxg);
    // next to Windows: half of what's possible (Windows likes room to grow), up to 64 GB; empty space: all of it
    long def = mode == "alongside" ? std::min(64L, std::max(std::min(8L, maxg), maxg / 2)) : maxg;
    size_slider->value(def); size_slider->step(1);
    show_size();
}

// ------------------------------------------------------------- page 4: ready
static void fill_ready() {
    Disk &d = disks[cur_disk];
    std::string s;
    if (mode == "alongside")
        s = "tinyc goes next to Windows on " + d.model + ".\n\n1. Windows is made " + gb((long)size_slider->value() * 1024) + " smaller. Its files stay as they are.\n"
            "2. tinyc goes in the freed space.\n3. When the computer starts, a menu lets you pick tinyc or Windows.\n\n"
            "Windows may check its disk once the next time it starts. That's normal, let it finish.";
    else if (mode == "free")
        s = "tinyc goes in the empty space on " + d.model + " (" + gb((long)size_slider->value() * 1024) + ").\n\nNothing else on the disk is changed.";
    else
        s = "The whole disk " + d.model + " (" + gb(d.size) + ") is erased and used for tinyc.\n\nEVERYTHING on it is deleted for good"
            + std::string(d.wins.empty() ? "." : ", including Windows and all the files in it.");
    s += "\n\nIt takes a few minutes. Don't turn the computer off or pull out the USB stick until it's done.";
    label(summary, s);
    if (mode == "replace") { understand->show(); understand->value(0); understand->copy_label(("I understand that everything on " + d.model + " will be deleted for good").c_str()); }
    else { understand->hide(); understand->value(1); }
}

// ------------------------------------------------------------- page 5: installing
static FILE *job;
static std::string jobbuf;
static bool finished = false, failed = false;
static void job_line(const std::string &l) {
    if (l.rfind("STEP ", 0) == 0) {
        int n = atoi(l.c_str() + 5); size_t sp = l.find(' ', 5);
        progress->value(n * 100.0f / 8); label(status, sp == std::string::npos ? "" : l.substr(sp + 1));
    } else if (l == "DONE") {
        finished = true; progress->value(100);
        label(status, "tinyc is installed!");
        label(detail, "Take out the USB stick and restart the computer." +
              std::string(mode != "replace" ? "\nWhen it starts, a menu lets you pick tinyc or Windows (it remembers your last choice)." : "")
              + "\nIf the computer still starts the old way, open its boot menu (often F12) and pick tinyc.");
        b_restart->show(); b_cancel->copy_label("Close"); b_cancel->activate();
    } else if (l.rfind("ERR ", 0) == 0) {
        failed = true; label(status, "Installing didn't work");
        label(detail, l.substr(4) + "\n\nIf Windows was there, it's still there. You can close this and try again.");
        detail->labelcolor(RED); b_cancel->copy_label("Close"); b_cancel->activate();
    }
}
static void job_read(int fd, void *) {
    char buf[1024]; ssize_t n = read(fd, buf, sizeof buf);
    if (n <= 0) {
        Fl::remove_fd(fd); pclose(job); job = nullptr;
        if (!finished && !failed) { failed = true; label(status, "Installing stopped unexpectedly"); b_cancel->copy_label("Close"); b_cancel->activate(); }
        return;
    }
    jobbuf.append(buf, n); size_t p;
    while ((p = jobbuf.find('\n')) != std::string::npos) { job_line(jobbuf.substr(0, p)); jobbuf.erase(0, p + 1); }
}
static void start_install() {
    Disk &d = disks[cur_disk];
    std::string cmd = "/usr/sbin/tinyc-install run " + mode + " " + d.name + " ";
    if (mode == "alongside") cmd += sel_win->part + " " + std::to_string((long)size_slider->value() * 1024);
    else if (mode == "free") cmd += std::to_string(sel_free->start) + " " + std::to_string((long)size_slider->value() * 1024);
    else cmd += "- 0";
    cmd += keep_files->value() ? " 1" : " 0";
    cmd += " 2>&1";
    job = popen(cmd.c_str(), "r");
    if (!job) { job_line("ERR Couldn't start the installer."); return; }
    Fl::add_fd(fileno(job), job_read);
}

// ------------------------------------------------------------- moving between pages
static const char *titles[] = { "Looking at this computer...", "Before you start", "Where should tinyc go?",
                                "How much space for tinyc?", "Ready to install", "Installing tinyc" };
static void show_page(int p) {
    for (int i = 0; i < 6; i++) if (i == p) pages[i]->show(); else pages[i]->hide();
    page = p; title->copy_label(titles[p]);
    b_back->show(); b_next->show(); b_next->copy_label("Next"); b_next->activate();
    if (p <= 1) b_back->hide();
    if (p == 0) b_next->hide();
    if (p == 1) { if (backed_up->value()) b_next->activate(); else b_next->deactivate(); }
    if (p == 4) { b_next->copy_label("Install"); if (understand->value()) b_next->activate(); else b_next->deactivate(); }
    if (p == 5) { b_back->hide(); b_next->hide(); b_cancel->deactivate(); }
    win->redraw();
}
static void cb_next(Fl_Widget *, void *) {
    if (page == 1) { if (disks.empty()) return; fill_where(); show_page(2); }
    else if (page == 2) {
        mode = m_along->value() ? "alongside" : m_free->value() ? "free" : m_erase->value() ? "replace" : "";
        if (mode.empty()) return;
        if (mode == "replace") { fill_ready(); show_page(4); } else { fill_size(); show_page(3); }
    } else if (page == 3) { fill_ready(); show_page(4); }
    else if (page == 4) { show_page(5); start_install(); }
}
static void cb_back(Fl_Widget *, void *) {
    if (page == 4 && mode == "replace") show_page(2); else if (page > 1) show_page(page - 1);
}
static void cb_cancel(Fl_Widget *, void *) {
    if (page == 5 && !finished && !failed) return;          // never in the middle of it
    win->hide();
}
static void cb_restart(Fl_Widget *, void *) { execl("/usr/sbin/tinyc-power", "tinyc-power", "reboot", (char *)NULL); }
static void cb_toggle(Fl_Widget *, void *) {
    if (page == 1) { if (backed_up->value()) b_next->activate(); else b_next->deactivate(); }
    if (page == 4) { if (understand->value()) b_next->activate(); else b_next->deactivate(); }
}

// ------------------------------------------------------------- the scan, at the start
static FILE *scanp; static std::string scanbuf;
static void scan_read(int fd, void *) {
    char buf[1024]; ssize_t n = read(fd, buf, sizeof buf);
    if (n > 0) { scanbuf.append(buf, n); return; }
    Fl::remove_fd(fd); pclose(scanp);
    size_t a = 0, b; while ((b = scanbuf.find('\n', a)) != std::string::npos) { parse_line(scanbuf.substr(a, b - a)); a = b + 1; }
    if (source != "ok") {
        label(chk_text[0], "Install tinyc only works when tinyc is started from its USB stick.");
        show_page(1); for (int i = 1; i < 4; i++) chk_text[i]->hide(); backed_up->hide(); b_next->hide(); return;
    }
    if (disks.empty()) { show_page(1); label(chk_text[0], "There's no disk in this computer that tinyc could go on (only the USB stick)."); for (int i = 1; i < 4; i++) chk_text[i]->hide(); backed_up->hide(); b_next->hide(); return; }
    for (auto &d : disks) {
        std::string s = d.model + "  (" + gb(d.size) + ", " + d.name + ")";
        std::string esc; for (char c : s) { if (c == '/' || c == '\\' || c == '&' || c == '_') esc += '\\'; esc += c; }
        disk_choice->add(esc.c_str());
    }
    // start on the disk with Windows on it, else the biggest one
    cur_disk = 0; for (size_t i = 0; i < disks.size(); i++) if (!disks[i].wins.empty()) { cur_disk = i; break; }
    disk_choice->value(cur_disk);
    fill_checks(); show_page(1);
}

int main(int argc, char **argv) {
    tinyc_style();
    win = new Fl_Double_Window(W, H, "Install tinyc");
    title = text(20, 14, W - 40, 30, "", 18, FL_HELVETICA_BOLD);
    // 0: scanning
    pages[0] = new Fl_Group(0, 50, W, H - 100);
    text(20, 70, W - 40, 60, "Looking at this computer's disks. This takes a few seconds...");
    pages[0]->end();
    // 1: before you start
    pages[1] = new Fl_Group(0, 50, W, H - 100);
    text(20, 52, W - 40, 40, "tinyc will be put on this computer's own disk, so it starts without the USB stick. First, check these:");
    for (int i = 0; i < 4; i++) {
        chk_icon[i] = new Fl_Box(20, 98 + i * 52, 22, 22);
        chk_text[i] = text(48, 98 + i * 52, W - 70, 48, "");
    }
    backed_up = new Fl_Check_Button(20, 310, W - 40, 26, "I've backed up my important files (photos, documents...) somewhere else");
    backed_up->labelfont(FL_HELVETICA_BOLD); backed_up->callback(cb_toggle);
    Fl_Box *how = text(20, 342, W - 40, 60, "How to turn these off, and what could go wrong: see \"Installing tinyc\" in the docs, at ccossys.github.io/tinyc/install.html", 12);
    how->labelcolor(GREY);
    pages[1]->end();
    // 2: where
    pages[2] = new Fl_Group(0, 50, W, H - 100);
    disk_choice = new Fl_Choice(70, 56, W - 90, 26, "Disk:"); disk_choice->callback(cb_disk);
    m_along = new Fl_Round_Button(20, 96, W - 40, 24, "Next to Windows"); m_along->type(FL_RADIO_BUTTON); m_along->labelfont(FL_HELVETICA_BOLD);
    m_along_why = text(44, 120, W - 64, 56, "", 12);
    m_free = new Fl_Round_Button(20, 182, W - 40, 24, "In the empty space"); m_free->type(FL_RADIO_BUTTON); m_free->labelfont(FL_HELVETICA_BOLD);
    m_free_why = text(44, 206, W - 64, 44, "", 12);
    m_erase = new Fl_Round_Button(20, 256, W - 40, 24, "Erase the whole disk and use it all for tinyc"); m_erase->type(FL_RADIO_BUTTON); m_erase->labelfont(FL_HELVETICA_BOLD);
    m_erase_why = text(44, 280, W - 64, 56, "", 12); m_erase_why->labelcolor(RED);
    pages[2]->end();
    // 3: size
    pages[3] = new Fl_Group(0, 50, W, H - 100);
    text(20, 56, W - 40, 40, "Slide to choose how much of the disk tinyc gets (in GB). 16 GB or more is plenty for tinyc and lots of apps.");
    size_slider = new Fl_Hor_Value_Slider(20, 110, W - 40, 30); size_slider->callback(cb_size); size_slider->when(FL_WHEN_CHANGED);
    size_slider->selection_color(BLUE); size_slider->textsize(13);
    size_info = text(20, 156, W - 40, 80, "");
    pages[3]->end();
    // 4: ready
    pages[4] = new Fl_Group(0, 50, W, H - 100);
    summary = text(20, 56, W - 40, 230, "");
    keep_files = new Fl_Check_Button(20, 300, W - 40, 26, "Bring my files, settings and apps from this USB stick"); keep_files->value(1);
    understand = new Fl_Check_Button(20, 330, W - 40, 26, ""); understand->labelcolor(RED); understand->labelfont(FL_HELVETICA_BOLD); understand->callback(cb_toggle);
    pages[4]->end();
    // 5: installing
    pages[5] = new Fl_Group(0, 50, W, H - 100);
    status = text(20, 60, W - 40, 30, "Starting...", 14, FL_HELVETICA_BOLD);
    progress = new Fl_Progress(20, 96, W - 40, 22); progress->selection_color(BLUE); progress->minimum(0); progress->maximum(100);
    detail = text(20, 130, W - 40, 150, "Don't turn the computer off or pull out the USB stick.");
    b_restart = new Fl_Return_Button(20, 300, 160, 32, "Restart now"); b_restart->callback(cb_restart); b_restart->hide();
    pages[5]->end();
    // buttons
    b_cancel = new Fl_Button(20, H - 46, 100, 32, "Cancel"); b_cancel->callback(cb_cancel);
    b_back = new Fl_Button(W - 240, H - 46, 100, 32, "Back"); b_back->callback(cb_back);
    b_next = new Fl_Return_Button(W - 130, H - 46, 110, 32, "Next"); b_next->callback(cb_next);
    win->callback(cb_cancel);
    win->end();
    show_page(0);
    win->position((Fl::w() - W) / 2, (Fl::h() - H) / 2);
    Fl_PNG_Image icon("/usr/share/tinyc/icons/install_32x32.png"); if (icon.w() > 0) win->icon(&icon);
    win->show(argc, argv);
    scanp = popen("/usr/sbin/tinyc-install scan 2>/dev/null", "r");
    if (scanp) Fl::add_fd(fileno(scanp), scan_read);
    return Fl::run();
}
