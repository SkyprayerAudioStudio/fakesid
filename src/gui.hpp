#pragma once
#include "render.hpp"


namespace gui {

    struct Box {
        bool contains(Vec const& p) const {
            return p.x >= pos.x && p.y >= pos.y &&
                   p.x < pos.x + size.x && p.y < pos.y + size.y;
        }
        Vec pos;
        Vec size;
    };


    enum Align { CENTER, LEFT, RIGHT };


    Vec  cursor();
    void cursor(Vec const& c);
    void id(void const* addr);
    void same_line();
    void next_line();
    void align(Align a);
    void min_item_size(Vec const& s = {});
    void begin_frame();
    Box  padding(Vec const& size);
    void separator();
    void text(char const* fmt, ...);
    void highlight();
    bool button(char const* label, bool active = false);
    bool hold();
    void input_text(char* str, int len);
    bool drag_int(char const* label, char const* fmt, int& value, int min, int max, int page = 1);
    bool vertical_drag_int(int& value, int min, int max, int page = 1);
    bool clavier(uint8_t& n, int offset, bool highlight);

    void touch(int x, int y);

    void init();
    void free();
    void render(gfx::RenderTarget* rt);

}
