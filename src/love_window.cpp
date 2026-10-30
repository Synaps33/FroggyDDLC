#include "love_window.h"

int LoveWindow::s_width = 320;
int LoveWindow::s_height = 240;
std::string LoveWindow::s_title = "Balatro";
bool LoveWindow::s_open = true;

void LoveWindow::init(int w, int h, const char *title) {
    s_width = w;
    s_height = h;
    if (title) s_title = title;
    s_open = true;
}

void LoveWindow::setMode(int w, int h) {
    s_width = w;
    s_height = h;
}
