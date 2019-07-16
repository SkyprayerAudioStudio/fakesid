#include "gui.hpp"
#include "foo.hpp"
#include <cstdarg>
#include <algorithm>
#include <array>


namespace gui {

namespace color {
    Color make(uint32_t c, uint8_t a = 255) {
        return { uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), a };
    }

    Color mix(Color a, Color b, float x) {
        float w = (1 - x);
        return {
            uint8_t(a.r * w + b.r * x),
            uint8_t(a.g * w + b.g * x),
            uint8_t(a.b * w + b.b * x),
            uint8_t(a.a * w + b.a * x),
        };
    }

    const Color button_normal     = make(0x505050);
    const Color button_active     = make(0x55a049);
    const Color button_hover      = make(0x94e089);

    const Color input_text_background = make(0x222222);
    const Color input_text_normal = make(0x505050);
    const Color input_text_hover  = button_hover;
    const Color input_text_active = button_active;

    const Color drag              = make(0x222222);
    const Color handle_normal     = button_active;
    const Color handle_active     = button_hover;

    const Color separator         = make(0x222222);

    const Color highlight[] = {
        {},
        make(0x555577),
        make(0x998855),
    };


    const Color note_normal       = handle_normal;
    const Color note_active       = handle_active;

} // namespace


namespace {


Vec text_size(char const* str) {
    Vec size = { 0, FONT_HEIGHT };
    int16_t width = 0;
    while (int c = *str++) {
        if (c == '\n') {
            size.y += FONT_HEIGHT + 5;
            width = 0;
            continue;
        }
        width += FONT_WIDTH;
        size.x = std::max(size.x, width);
    }
    return size;
}


class DrawContext : public render::DrawContext {
public:

    void rect(const Vec& pos, const Vec& size, const Color& c, ButtonStyle style=BS_NORMAL) {
        int s = 8;
        int o = 64;
        if (size.x < 16 || size.y < 16) {
            s = 4;
            o += 16;
        }

        Vec p0 = pos;
        Vec p1 = pos + Vec(s);
        Vec p2 = pos + size - Vec(s);
        Vec p3 = pos + size;
        Vec t0 = { 16 * style, o };
        Vec t1 = t0 + Vec(s);
        Vec t2 = t1;
        Vec t3 = t2 + Vec(s);

        render::Vertex vs[] = {
            { Vec(p0.x, p0.y), Vec(t0.x, t0.y), c },
            { Vec(p1.x, p0.y), Vec(t1.x, t0.y), c },
            { Vec(p2.x, p0.y), Vec(t2.x, t0.y), c },
            { Vec(p3.x, p0.y), Vec(t3.x, t0.y), c },
            { Vec(p0.x, p1.y), Vec(t0.x, t1.y), c },
            { Vec(p1.x, p1.y), Vec(t1.x, t1.y), c },
            { Vec(p2.x, p1.y), Vec(t2.x, t1.y), c },
            { Vec(p3.x, p1.y), Vec(t3.x, t1.y), c },
            { Vec(p0.x, p2.y), Vec(t0.x, t2.y), c },
            { Vec(p1.x, p2.y), Vec(t1.x, t2.y), c },
            { Vec(p2.x, p2.y), Vec(t2.x, t2.y), c },
            { Vec(p3.x, p2.y), Vec(t3.x, t2.y), c },
            { Vec(p0.x, p3.y), Vec(t0.x, t3.y), c },
            { Vec(p1.x, p3.y), Vec(t1.x, t3.y), c },
            { Vec(p2.x, p3.y), Vec(t2.x, t3.y), c },
            { Vec(p3.x, p3.y), Vec(t3.x, t3.y), c },
        };

        quad(vs[0], vs[1], vs[4], vs[5]);
        quad(vs[1], vs[2], vs[5], vs[6]);
        quad(vs[2], vs[3], vs[6], vs[7]);
        quad(vs[4], vs[5], vs[8], vs[9]);
        quad(vs[5], vs[6], vs[9], vs[10]);
        quad(vs[6], vs[7], vs[10], vs[11]);
        quad(vs[8], vs[9], vs[12], vs[13]);
        quad(vs[9], vs[10], vs[13], vs[14]);
        quad(vs[10], vs[11], vs[14], vs[15]);
    }

    void glyph(const Vec& pos, const Color& c, uint8_t g) {
        copy(pos,
             { FONT_WIDTH, FONT_HEIGHT },
             { g % 16 * FONT_WIDTH, g / 16 * FONT_HEIGHT },
             c);
    }
    void text(const Vec& pos, const char* text) {
        Vec p = pos;
        while (char c = *text++) {
            if (c == '\n') {
                p.y += FONT_HEIGHT + 5;
                p.x = pos.x;
                continue;
            }
            if (c > 32 || c < 128) glyph(p, { 255, 255, 255, 255 }, c);
            p.x += FONT_WIDTH;
        }
    }
};


Touch       m_touch;
Vec         m_cursor_min;
Vec         m_cursor_max;
Vec         m_min_item_size;

int         m_hold_count;
bool        m_hold;
Highlight   m_highlight;
bool        m_same_line;
void const* m_id;
void const* m_active_item;
char*       m_input_text_str = nullptr;
int         m_input_text_len;
int         m_input_text_pos;
int         m_input_cursor_blink = 0;
Align       m_align        = A_CENTER;
ButtonStyle m_button_style = BS_NORMAL;


DrawContext     m_dc;
gfx::Texture2D* m_texture;


std::array<char, 1024> m_text_buffer;
void print_to_text_buffer(const char* fmt, va_list args) {
    vsnprintf(m_text_buffer.data(), m_text_buffer.size(), fmt, args);
}
void print_to_text_buffer(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_to_text_buffer(fmt, args);
    va_end(args);
}



void const* get_id(void const* addr) {
    if (m_id) {
        addr = m_id;
        m_id = nullptr;
   }
   return addr;
}


Box item_box(Vec const& s) {
    Vec pos;
    Vec size = glm::max(s, m_min_item_size);
    if (m_same_line) {
        m_same_line = false;
        pos = Vec(m_cursor_max.x, m_cursor_min.y);
        if (m_cursor_max.y - m_cursor_min.y > size.y) {
            pos.y += (m_cursor_max.y - m_cursor_min.y - size.y) / 2;
        }
        m_cursor_max = glm::max(m_cursor_max, pos + size);
    }
    else {
        pos = Vec(m_cursor_min.x, m_cursor_max.y);
        m_cursor_min.y = m_cursor_max.y;
        m_cursor_max = pos + size;
    }
    return { pos, size };
}


Vec print_pos(Box const& box, Vec const& s, int padding=5) {
    switch (m_align) {
    case A_CENTER: return box.pos + Vec((box.size.x - s.x) / 2, (box.size.y - s.y) / 2);
    case A_LEFT:   return box.pos + Vec(padding, (box.size.y - s.y) / 2);
    default:       return box.pos + Vec(box.size.x - s.x - padding, box.size.y - s.y / 2);
    }
}


} // namespace


Touch const& touch() {
    return m_touch;
}


Vec cursor() {
    if (m_same_line) return Vec(m_cursor_max.x, m_cursor_min.y);
    else return Vec(m_cursor_min.x, m_cursor_max.y);
}

void cursor(Vec const& c) {
    if (m_same_line) {
        m_cursor_max.x = c.x;
        m_cursor_min.y = c.y;
    }
    else {
        m_cursor_min.x = c.x;
        m_cursor_max.y = c.y;
    }
}


void id(void const* addr) {
    if (!m_id) m_id = addr;
}


void same_line() {
    m_same_line = true;
}


void next_line() {
    m_same_line = false;
}


void align(Align a) {
    m_align = a;
}

void highlight(Highlight highlight) {
    m_highlight = highlight;
}

void button_style(ButtonStyle s) {
    m_button_style = s;
}

void min_item_size(Vec const& s) {
    m_min_item_size = s;
}


void begin_frame() {
    ++m_input_cursor_blink;

    m_cursor_min = { 0, 0 };
    m_cursor_max = { 0, 0 };
    m_same_line = false;
    if (!(m_touch.pressed | m_touch.prev_pressed)) {
        m_active_item = nullptr;
        m_hold_count = 0;
    }
    if (m_touch.just_pressed() && m_input_text_str) {
        m_input_text_str = nullptr;
        android::hide_keyboard();
    }

    m_dc.clear();
}


Box padding(Vec const& size) {
    return item_box(size);
}

void separator() {
    Box box;
    if (m_same_line) {
        m_min_item_size.x = 0;
        box = item_box({ SEPARATOR_WIDTH, m_cursor_max.y - m_cursor_min.y });
        m_same_line = true;
    }
    else {
        m_min_item_size.y = 0;
        box = item_box({ m_cursor_max.x - m_cursor_min.x, SEPARATOR_WIDTH });
    }
    m_dc.copy(box.pos, box.size - Vec(1), {}, {1, 1}, color::separator);
}


void text(char const* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    print_to_text_buffer(fmt, args);
    va_end(args);
    Vec s = text_size(m_text_buffer.data());
    Box box = item_box(s);
    m_dc.text(print_pos(box, s), m_text_buffer.data());
}


static bool button_helper(Box const& box, bool active) {
    enum { HOLD_TIME = 10 };
    m_hold = false;
    Color color = color::button_normal;
    bool clicked = false;
    if (m_active_item == nullptr && m_touch.box_touched(box)) {
        color = color::button_hover;
        if (box.contains(m_touch.prev_pos)) {
            if (++m_hold_count > HOLD_TIME) m_hold = true;
        }
        else m_hold_count = 0;
        if (m_touch.just_released()) clicked = true;
    }
    else {
        if (active) color = color::button_active;
        else if (m_highlight) color = color::highlight[m_highlight];
    }
    m_dc.rect(box.pos, box.size, color, m_button_style);
    return clicked;
}


bool button(char const* label, bool active) {
    Vec s = text_size(label);
    Box box = item_box(s);
    bool clicked = button_helper(box, active);
    m_dc.text(print_pos(box, s), label);
    return clicked;
}


bool button(Icon icon, bool active) {
    Vec s = Vec(16);
    Box box = item_box(s);
    bool clicked = button_helper(box, active);
    m_dc.copy(print_pos(box, s), s, { icon % 8 * 16, 96 + icon / 8 * 16 });
    return clicked;
}


bool hold() {
    if (m_hold) m_active_item = (void const*) -1;
    return m_hold;
}


void key(int key, int unicode) {
    if (!m_input_text_str) return;
    switch (key) {
    case KEYCODE_DEL:
        m_input_cursor_blink = 0;
        if (m_input_text_pos > 0) m_input_text_str[--m_input_text_pos] = '\0';
        return;
    case KEYCODE_ENTER:
        android::hide_keyboard();
        m_input_text_str = nullptr;
        return;
    default: break;
    }

    if (unicode == 0) return;
    if (m_input_text_pos >= m_input_text_len) return;

    m_input_cursor_blink = 0;
    int c = unicode;
    if (isalnum(c) || (m_input_text_pos > 0 && strchr(" _-.+()", c))) {
        m_input_text_str[m_input_text_pos++] = c;
    }
}


void input_text(char* str, int len) {
    Vec s = text_size(str);
    Box box = item_box(s + Vec(8, 0));

    //Color color = color::input_text_normal;
    Color color = color::input_text_normal;
    if (m_active_item == nullptr && m_touch.box_touched(box)) {
        color = color::input_text_hover;
        if (m_touch.just_released()) {
            android::show_keyboard();
            m_input_text_str = str;
            m_input_text_len = len;
            m_input_text_pos = strlen(m_input_text_str);
        }
    }
    if (m_input_text_str == str) {
        color = color::input_text_active;
    }

    m_dc.rect(box.pos, box.size, color::input_text_background, BS_NORMAL);
    m_dc.rect(box.pos, box.size, color, BS_FRAME);

    Vec p = box.pos + Vec(8, box.size.y / 2 - s.y / 2);
    m_dc.text(p, str);
    // cursor
    if (m_input_text_str == str && m_input_cursor_blink % 16 < 8) {
        m_dc.text(p + Vec(s.x, 0), "\x7f");
    }
}


bool drag_int(char const* label, char const* fmt, int& value, int min, int max, int page) {
    Vec s1 = text_size(label);
    print_to_text_buffer(fmt, value);
    Vec s2 = text_size(m_text_buffer.data());
    // padding
    if (s1.x > 0 && s2.x > 0) s1.x += 8;

    Box box = item_box(Vec(s1.x + s2.x, std::max(s1.y, s2.y)));
    int range = max - min;
    int handle_w = std::max(10, box.size.x * page / (range + page));
    int w = box.size.x - handle_w;
    int handle_x = range == 0 ? 0 : (value - min) * w / range;

    void const* id = get_id(&value);
    if (m_active_item == nullptr && m_touch.box_touched(box) && m_touch.just_pressed()) {
        m_active_item = id;
    }
    int old_value = value;
    if (m_active_item == id && range > 0) {
        int x = m_touch.pos.x - box.pos.x - handle_w / 2 + w / range / 2;
        value = glm::clamp(min + x * range / w, min, max);
    }

    m_dc.rect(box.pos, box.size, color::drag);
    Color c = m_active_item == id ? color::handle_active : color::handle_normal;
    m_dc.rect(box.pos + Vec(handle_x, 0), Vec(handle_w, box.size.y), c, BS_ROUND);

    m_dc.text(box.pos + Vec(5, box.size.y / 2 - s1.y / 2), label);
    m_dc.text(box.pos + Vec(box.size.x - s2.x - 5, box.size.y / 2 - s2.y / 2), m_text_buffer.data());

    return value != old_value;
}

bool vertical_drag_int(int& value, int min, int max, int page) {
    Box box = item_box({});
    int range = max - min;
    int handle_h = box.size.y * page / (range + page);
    int h = box.size.y - handle_h;
    int handle_y = range == 0 ? 0 : (value - min) * (box.size.y - handle_h) / range;

    void const* id = get_id(&value);
    if (m_active_item == nullptr && m_touch.box_touched(box) && m_touch.just_pressed()) {
        m_active_item = id;
    }
    int old_value = value;
    if (m_active_item == id && range > 0) {
        int y = m_touch.pos.y - box.pos.y - handle_h / 2 + h / range / 2;
        value = glm::clamp(min + y * range / h, min, max);
    }

    m_dc.rect(box.pos, box.size, color::drag);
    Color c = m_active_item == id ? color::handle_active : color::handle_normal;
    m_dc.rect(box.pos + Vec(0, handle_y), Vec(box.size.x, handle_h), c, BS_ROUND);

    return value != old_value;
}



bool clavier(uint8_t& n, int offset) {
    Box box = item_box({ 100, 20 });

    void const* id = get_id(&n);
    if (m_active_item == nullptr && m_touch.box_touched(box) && m_touch.just_pressed()) {
        m_active_item = id;
    }

    uint8_t old_n = n;

    int x0 = 0;
    bool just_pressed = m_touch.just_pressed();
    for (int i = 0; i < CLAVIER_WIDTH; ++i) {
        int x1 = (box.size.x - CLAVIER_WIDTH) * (i + 1) / CLAVIER_WIDTH + (i + 1);
        int nn = i + 1 + offset;
        Box b = {
            { box.pos.x + x0, box.pos.y },
            { x1 - x0, box.size.y },
        };
        bool touch = b.contains({ m_touch.pos.x, b.pos.y });
        if (m_active_item == id && touch) {
            if (just_pressed) {
                if (n == nn) n = 0;
                else if (n == 0) n = nn;
            }
            else if (n != 0) n = nn;
        }
        Color color = color::make(0x222222);
        if ((i + offset) % 12 == 0) color = color::make(0x333333);
        if ((1 << (i + offset) % 12) & 0b010101001010) color = color::make(0x111111);
        if (m_highlight) color = color::mix(color, color::highlight[m_highlight], 0.1);
        m_dc.rect(b.pos, b.size, color);

        if (n == nn) {
            if (m_active_item == id && touch) color = color::note_active;
            else color = color::note_normal;
            m_dc.rect(b.pos, b.size, color, BS_ROUND);
        }

        x0 = x1;
    }
    return n != old_n;
}

void jam(int index) {
    static const Color colors[] = {
        color::make(0x111111),
        color::make(0x222222),
        color::highlight[H_CURSOR],
    };
    Box box = item_box({});
    m_dc.rect(box.pos, box.size, colors[index], BS_JAM);
}

void init() {
    m_texture = android::load_texture("gui.png");
}
void free() {
    delete m_texture;
    m_texture = nullptr;
}

void touch(int x, int y, bool pressed) {
    m_touch.pos     = { x, y };
    m_touch.pressed = pressed;
}

void render(gfx::RenderTarget* rt) {
    render::draw(rt, m_dc.vertices(), m_texture);
    m_dc.clear();

    // update touch
    m_touch.prev_pos     = m_touch.pos;
    m_touch.prev_pressed = m_touch.pressed;
}


} // namespace
