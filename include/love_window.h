#ifndef LOVE_WINDOW_H
#define LOVE_WINDOW_H

#include <string>

class LoveWindow {
public:
    static void init(int w = 320, int h = 240, const char *title = "Balatro");
    static int getWidth() { return s_width; }
    static int getHeight() { return s_height; }
    static void setMode(int w, int h);
    static const char *getTitle() { return s_title.c_str(); }
    static bool isOpen() { return s_open; }

private:
    static int s_width;
    static int s_height;
    static std::string s_title;
    static bool s_open;
};

#endif // LOVE_WINDOW_H
