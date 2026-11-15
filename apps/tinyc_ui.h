// tinyc_ui.h - the shared look of tinyc's own apps (FLTK): plain, light, ccos blue
#pragma once
#include <FL/Fl.H>
static void tinyc_style() {
    Fl::scheme("gtk+");
    Fl::background(0xf3, 0xf5, 0xf8);
    Fl::background2(0xff, 0xff, 0xff);
    Fl::foreground(0x24, 0x26, 0x2a);
    Fl::set_color(FL_SELECTION_COLOR, 0x26, 0x74, 0xd8);
    Fl::set_font(FL_HELVETICA, "DejaVu Sans");
    Fl::set_font(FL_HELVETICA_BOLD, "DejaVu Sans:bold");
    Fl::set_font(FL_COURIER, "DejaVu Sans Mono");
    Fl::set_font(FL_COURIER_BOLD, "DejaVu Sans Mono:bold");
    FL_NORMAL_SIZE = 13;
}
