// tinyc Notepad - a plain text editor.   notepad [FILE]
#include "tinyc_ui.h"
#include "tinyc_icons.h"
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <string>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

static Fl_Double_Window *win;
static Fl_Text_Editor *ed;
static Fl_Text_Buffer *buf;
static std::string path;
static bool changed = false, loading = false, wrap = true;
static std::string last_find;

static std::string docs() {
    const char *h = getenv("HOME"); std::string d = std::string(h ? h : "/root") + "/Documents";
    mkdir(d.c_str(), 0755); return d;
}
static void set_title() {
    static std::string t;
    const char *n = path.empty() ? "Untitled" : strrchr(path.c_str(), '/') ? strrchr(path.c_str(), '/') + 1 : path.c_str();
    t = std::string(changed ? "*" : "") + n + " - Notepad";
    win->copy_label(t.c_str());
}
static void on_change(int, int ins, int del, int, const char *, void *) {
    if (loading || (!ins && !del)) return;
    if (!changed) { changed = true; set_title(); }
}
static bool save_to(const std::string &p) {
    if (buf->savefile(p.c_str())) { fl_alert("Couldn't save %s.", p.c_str()); return false; }
    path = p; changed = false; set_title(); return true;
}
static bool save_as() {
    std::string start = path.empty() ? docs() + "/Untitled.txt" : path;
    const char *p = fl_file_chooser("Save as", "Text files (*.txt)\t*", start.c_str());
    if (!p) return false;
    struct stat st;
    if (std::string(p) != path && stat(p, &st) == 0 && fl_choice("%s already exists. Replace it?", "Cancel", "Replace", NULL, p) != 1) return false;
    return save_to(p);
}
static bool save() { return path.empty() ? save_as() : save_to(path); }
static bool ok_to_lose() {             // true = go on (saved, or don't care)
    if (!changed) return true;
    int r = fl_choice("Save the changes to %s?", "Cancel", "Save", "Don't save",
                      path.empty() ? "Untitled" : strrchr(path.c_str(), '/') + 1);
    if (r == 0) return false;
    if (r == 1) return save();
    return true;
}
static void load(const char *p) {
    loading = true;
    struct stat st;
    if (stat(p, &st) == 0 && buf->loadfile(p)) fl_alert("Couldn't open %s.", p);
    else if (stat(p, &st) != 0) buf->text("");
    loading = false;
    path = p; changed = false; set_title();
}
static void cb_new(Fl_Widget *, void *) { if (!ok_to_lose()) return; loading = true; buf->text(""); loading = false; path.clear(); changed = false; set_title(); }
static void cb_open(Fl_Widget *, void *) {
    if (!ok_to_lose()) return;
    const char *p = fl_file_chooser("Open", "Text files (*.txt)\t*", (docs() + "/").c_str());
    if (p) load(p);
}
static void cb_save(Fl_Widget *, void *) { save(); }
static void cb_saveas(Fl_Widget *, void *) { save_as(); }
static void cb_quit(Fl_Widget *, void *) { if (ok_to_lose()) { win->hide(); } }
static void cb_undo(Fl_Widget *, void *) { Fl_Text_Editor::kf_undo(0, ed); }
static void cb_cut(Fl_Widget *, void *) { Fl_Text_Editor::kf_cut(0, ed); }
static void cb_copy(Fl_Widget *, void *) { Fl_Text_Editor::kf_copy(0, ed); }
static void cb_paste(Fl_Widget *, void *) { Fl_Text_Editor::kf_paste(0, ed); }
static void cb_all(Fl_Widget *, void *) { buf->select(0, buf->length()); }
static void find_next() {
    if (last_find.empty()) return;
    int pos = ed->insert_position(), found;
    if (buf->search_forward(pos, last_find.c_str(), &found) || buf->search_forward(0, last_find.c_str(), &found)) {
        buf->select(found, found + (int)last_find.size());
        ed->insert_position(found + (int)last_find.size()); ed->show_insert_position();
    } else fl_message("Can't find \"%s\".", last_find.c_str());
}
static void cb_find(Fl_Widget *, void *) {
    const char *s = fl_input("Find:", last_find.c_str());
    if (s && *s) { last_find = s; find_next(); }
}
static void cb_findnext(Fl_Widget *, void *) { if (last_find.empty()) cb_find(0, 0); else find_next(); }
static void cb_wrap(Fl_Widget *, void *) {
    wrap = !wrap; ed->wrap_mode(wrap ? Fl_Text_Display::WRAP_AT_BOUNDS : Fl_Text_Display::WRAP_NONE, 0);
}
static void cb_font(Fl_Widget *, void *v) {
    int d = (int)(long)v, s = ed->textsize() + d; if (s < 9) s = 9; if (s > 32) s = 32;
    ed->textsize(s); ed->redraw();
}
static void cb_about(Fl_Widget *, void *) { fl_message("Notepad\ntinyc's plain text editor."); }
static void cb_close(Fl_Widget *, void *) { cb_quit(0, 0); }

int main(int argc, char **argv) {
    tinyc_style();
    win = new Fl_Double_Window(720, 500, "Notepad");
    Fl_Menu_Bar *m = new Fl_Menu_Bar(0, 0, 720, 26);
    m->add("&File/&New", FL_CTRL + 'n', cb_new);
    m->add("&File/&Open...", FL_CTRL + 'o', cb_open);
    m->add("&File/&Save", FL_CTRL + 's', cb_save);
    m->add("&File/Save &as...", FL_CTRL + FL_SHIFT + 's', cb_saveas, 0, FL_MENU_DIVIDER);
    m->add("&File/&Quit", FL_CTRL + 'q', cb_quit);
    m->add("&Edit/&Undo", FL_CTRL + 'z', cb_undo, 0, FL_MENU_DIVIDER);
    m->add("&Edit/Cu&t", FL_CTRL + 'x', cb_cut);
    m->add("&Edit/&Copy", FL_CTRL + 'c', cb_copy);
    m->add("&Edit/&Paste", FL_CTRL + 'v', cb_paste, 0, FL_MENU_DIVIDER);
    m->add("&Edit/Select &all", FL_CTRL + 'a', cb_all, 0, FL_MENU_DIVIDER);
    m->add("&Edit/&Find...", FL_CTRL + 'f', cb_find);
    m->add("&Edit/Find &next", FL_F + 3, cb_findnext);
    m->add("&View/&Word wrap", 0, cb_wrap, 0, FL_MENU_TOGGLE | FL_MENU_VALUE);
    m->add("&View/&Bigger text", FL_CTRL + '=', cb_font, (void *)2);
    m->add("&View/&Smaller text", FL_CTRL + '-', cb_font, (void *)-2);
    m->add("&Help/&About Notepad", 0, cb_about);
    buf = new Fl_Text_Buffer();
    ed = new Fl_Text_Editor(0, 26, 720, 474);
    ed->buffer(buf);
    ed->textfont(FL_COURIER); ed->textsize(14);
    ed->box(FL_FLAT_BOX); ed->color(FL_WHITE);
    ed->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    ed->linenumber_width(0);
    buf->add_modify_callback(on_change, 0);
    win->resizable(ed);
    win->callback(cb_close);
    win->end();
    if (argc > 1) load(argv[1]); else set_title();
    { static Fl_RGB_Image ic(icon_notepad, 32, 32, 4); win->icon(&ic); }
    win->show(1, argv);
    Fl::focus(ed);
    return Fl::run();
}
