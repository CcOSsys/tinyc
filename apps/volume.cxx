// tinyc-volume - a small volume slider that opens above the taskbar (from the speaker icon)
#include "tinyc_ui.h"
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static Fl_Value_Slider *s;
static Fl_Check_Button *mute;
static void read_now() {
    FILE *p = popen("amixer get Master 2>/dev/null", "r"); char l[256]; int pct = -1, off = 0;
    if (p) {
        while (fgets(l, sizeof l, p)) { char *a = strchr(l, '['); if (!a) continue; if (pct < 0) sscanf(a, "[%d%%]", &pct); if (strstr(l, "[off]")) off = 1; }
        pclose(p);
    }
    s->value(pct < 0 ? 0 : pct); mute->value(off);
}
static void cb_slide(Fl_Widget *, void *) {
    int pct = (int)s->value();
    std::string c = "amixer -q set Master " + std::to_string(pct) + "% unmute"; system(c.c_str());
    mute->value(0);
}
static void cb_mute(Fl_Widget *, void *) { system(mute->value() ? "amixer -q set Master mute" : "amixer -q set Master unmute"); }

class Pop : public Fl_Double_Window {
public:
    Pop(int w, int h) : Fl_Double_Window(w, h) {}
    int handle(int e) override {
        if (e == FL_UNFOCUS || (e == FL_KEYDOWN && Fl::event_key() == FL_Escape)) { hide(); return 1; }
        return Fl_Double_Window::handle(e);
    }
};

int main(int argc, char **argv) {
    tinyc_style();
    Pop *w = new Pop(80, 230);
    w->border(0);
    Fl_Box *frame = new Fl_Box(0, 0, 80, 230); frame->box(FL_BORDER_BOX); frame->color(FL_BACKGROUND_COLOR);
    Fl_Box *t = new Fl_Box(0, 4, 80, 20, "Volume"); t->labelfont(FL_HELVETICA_BOLD);
    s = new Fl_Value_Slider(25, 28, 30, 160); s->bounds(100, 0); s->step(5); s->precision(0);   // loud at the top
    s->selection_color(fl_rgb_color(0x26, 0x74, 0xd8)); s->textsize(11);
    s->callback(cb_slide); s->when(FL_WHEN_CHANGED);
    mute = new Fl_Check_Button(10, 196, 66, 26, "Mute"); mute->callback(cb_mute);
    w->end();
    read_now();
    // above the mouse (the speaker icon is in the taskbar at the bottom)
    int mx, my; Fl::get_mouse(mx, my);
    int x = mx - 40, y = my - 240;
    if (x + 80 > Fl::w()) x = Fl::w() - 82; if (x < 0) x = 0; if (y < 0) y = my + 10;
    w->position(x, y);
    w->set_non_modal();
    w->show(argc, argv);
    Fl::focus(s);
    return Fl::run();
}
