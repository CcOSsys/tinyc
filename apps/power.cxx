// tinyc-shutdown off|reboot : "Are you sure?" before turning off or restarting (from the /o\ menu).
// Cancel is the default: Enter and Esc both cancel, only a click on the button goes ahead.
#include "tinyc_ui.h"
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <cstring>
#include <unistd.h>

static bool go = false;
static void cb_go(Fl_Widget *w, void *) { go = true; w->window()->hide(); }
static void cb_cancel(Fl_Widget *w, void *) { (w->window() ? w->window() : (Fl_Window *)w)->hide(); }

int main(int argc, char **argv) {
    bool off = argc < 2 || strcmp(argv[1], "reboot") != 0;
    tinyc_style();
    Fl_Double_Window *win = new Fl_Double_Window(340, 110, off ? "Shut down" : "Restart");
    Fl_Box *q = new Fl_Box(15, 15, 310, 40, off ? "Are you sure you want to shut down?" : "Are you sure you want to restart?");
    q->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    Fl_Button *b = new Fl_Button(105, 65, 110, 30, off ? "Shut down" : "Restart");
    b->callback(cb_go);
    Fl_Return_Button *c = new Fl_Return_Button(225, 65, 100, 30, "Cancel");   // Enter = Cancel
    c->callback(cb_cancel);
    win->callback(cb_cancel);                 // Esc and the window's X close it = Cancel
    win->end();
    win->position((Fl::w() - win->w()) / 2, (Fl::h() - win->h()) / 2);   // middle of the screen
    win->show();
    Fl::focus(c);
    Fl::run();
    if (!go) return 0;
    execl("/usr/sbin/tinyc-power", "tinyc-power", off ? "off" : "reboot", (char *)NULL);
    return 1;
}
