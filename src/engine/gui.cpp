#include "gui.h"
#include "shader.h"

#include <cassert>
#include <GLFW/glfw3.h> // For GLFW_KEY_* bindings

static bool operator == (const widget_id & a, const widget_id & b) { return a.root == b.root && a.path == b.path; }

void gui_state::begin_frame()
{
    clicked = false;
    scroll = {};
    key = mods = 0;
}

void gui_state::on_scroll(GLFWwindow * window, double x, double y) 
{ 
    cursor_window = window;
    scroll += int2(double2(x,y)*100.0); 
}

void gui_state::on_mouse_button(GLFWwindow * window, int button, int action, int mods)
{
    cursor_window = window;
    if(button == GLFW_MOUSE_BUTTON_LEFT)
    {
        down = action == GLFW_PRESS;
        if(down) clicked = true;
    }
}

void gui_state::delete_text_entry_selection()
{
    auto a = std::min(text_entry_cursor, text_entry_mark), b = std::max(text_entry_cursor, text_entry_mark);
    text_entry.erase(text_entry.begin() + a, text_entry.begin() + b);
    text_entry_mark = text_entry_cursor = a;
}

void gui_state::on_key(GLFWwindow * window, int key, int action, int mods)
{
    cursor_window = window;
    auto set_text_entry_cursor = [this,mods](size_t cursor)
    {
        text_entry_cursor = cursor;
        if(!(mods & GLFW_MOD_SHIFT)) text_entry_mark = cursor;
    };

    if(action != GLFW_RELEASE)
    {
        if(!text_entry_id.path.empty() && focus_id == text_entry_id && action != GLFW_RELEASE) 
        {
            // Consume ordinary key presses related to text entry
            switch(key)
            {
            case GLFW_KEY_LEFT: if(text_entry_cursor > 0) set_text_entry_cursor(utf8::prev(text_entry.c_str() + text_entry_cursor) - text_entry.c_str()); return;
            case GLFW_KEY_RIGHT: if(text_entry_cursor < text_entry.size()) set_text_entry_cursor(utf8::next(text_entry.c_str() + text_entry_cursor) - text_entry.c_str()); return;
            case GLFW_KEY_HOME: set_text_entry_cursor(0); return;
            case GLFW_KEY_END: set_text_entry_cursor(text_entry.size()); return;
            case GLFW_KEY_DELETE: 
                if(text_entry_cursor != text_entry_mark) delete_text_entry_selection();
                else if(text_entry_cursor < text_entry.size()) 
                {
                    auto other = utf8::next(text_entry.c_str() + text_entry_cursor) - text_entry.c_str();
                    text_entry.erase(text_entry.begin() + text_entry_cursor, text_entry.begin() + other);
                    text_entry_mark = text_entry_cursor;
                }
                return;
            case GLFW_KEY_BACKSPACE: 
                if(text_entry_cursor != text_entry_mark) delete_text_entry_selection();
                else if(text_entry_cursor > 0) 
                {
                    auto other = utf8::prev(text_entry.c_str() + text_entry_cursor) - text_entry.c_str();
                    text_entry.erase(text_entry.begin() + other, text_entry.begin() + text_entry_cursor);
                    text_entry_mark = text_entry_cursor = exactly(other);
                }
                return;
            }

            // Consume CTRL + key presses related to text entry
            if(mods & GLFW_MOD_CONTROL) switch(key)
            {
            case GLFW_KEY_A:
                text_entry_mark = 0;
                text_entry_cursor = text_entry.size();
                return;
            case GLFW_KEY_C:
                glfwSetClipboardString(window, text_entry.substr(std::min(text_entry_cursor,text_entry_mark), std::abs((int)text_entry_cursor - (int)text_entry_mark)).c_str());
                return;
            case GLFW_KEY_X:
                glfwSetClipboardString(window, text_entry.substr(std::min(text_entry_cursor,text_entry_mark), std::abs((int)text_entry_cursor - (int)text_entry_mark)).c_str());
                delete_text_entry_selection();
                return;
            case GLFW_KEY_V:
                if(text_entry_cursor != text_entry_mark) delete_text_entry_selection();
                auto s = glfwGetClipboardString(window);
                text_entry.insert(text_entry_cursor, s);
                text_entry_mark = text_entry_cursor += strlen(s);
                return;
            }

            assert(utf8::is_valid(text_entry));
        }

        this->key = key;
        this->mods = mods;
    }
}

void gui_state::on_char(GLFWwindow * window, uint32_t codepoint)
{
    cursor_window = window;
    if(!text_entry_id.path.empty() && focus_id == text_entry_id) 
    {
        if(text_entry_cursor != text_entry_mark) delete_text_entry_selection();
        auto units = utf8::units(codepoint);
        text_entry.insert(text_entry_cursor, units.data());
        text_entry_mark = text_entry_cursor += strlen(units.data());
        assert(utf8::is_valid(text_entry));
    }
}

/////////
// gui //
/////////

gui::gui(gui_state & state, canvas & canvas, const gui_style & style, GLFWwindow * window) : 
    state{state}, buf{canvas}, style{style}, window{window}, current_layer{-1}
{
    double2 pos;
    glfwGetCursorPos(window, &pos.x, &pos.y);
    local_cursor = int2(pos);

    current_id_prefix.root = window;
    current_id_prefix.path.clear();

    ctype = cursor_type::arrow;

    begin_overlay();
}

void gui::begin_overlay() 
{ 
    rect<int> scissor;
    glfwGetFramebufferSize(window, &scissor.x1, &scissor.y1);
    scissor_stack.push_back(scissor);
    buf.set_target(++current_layer, scissor_stack.back(), 0);
}
void gui::end_overlay() 
{ 
    scissor_stack.pop_back();
    buf.set_target(--current_layer, scissor_stack.back(), 0);
}
void gui::begin_scissor(const rect<int> & r) 
{ 
    scissor_stack.push_back(scissor_stack.back().intersected_with(r));
    buf.set_target(current_layer, scissor_stack.back(), 0);
}
void gui::end_scissor()
{ 
    scissor_stack.pop_back();
    buf.set_target(current_layer, scissor_stack.back(), 0);
}

void gui::draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color) { return buf.draw_line(p0, p1, width, color); }
void gui::draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color) { return buf.draw_bezier_curve(p0, p1, p2, p3, width, color); }
void gui::draw_wire_rect(const rect<int> & r, int width, const float4 & color) { return buf.draw_wire_rect(r, width, color); }
void gui::draw_rect(const rect<int> & r, const float4 & color) { return buf.draw_rect(r, color); }
void gui::draw_circle(const int2 & center, int radius, const float4 & color) { return buf.draw_circle(center, radius, color); }
void gui::draw_rounded_rect(const rect<int> & r, int corner_radius, const float4 & color) { return buf.draw_rounded_rect(r, corner_radius, color); }
void gui::draw_partial_rounded_rect(const rect<int> & r, int corner_radius, corner_flags corners, const float4 & color) { return buf.draw_partial_rounded_rect(r, corner_radius, corners, color); }
void gui::draw_convex_polygon(array_view<ui_vertex> vertices) { return buf.draw_convex_polygon(vertices); }
void gui::draw_sprite(const rect<int> & r, const float4 & color, const rect<float> & texcoords) { return buf.draw_sprite(r, color, texcoords); }
void gui::draw_sprite_sheet(const int2 & p) { return buf.draw_sprite_sheet(p); }
void gui::draw_glyph(const int2 & pos, const float4 & color, const font_face & font, uint32_t codepoint) { return buf.draw_glyph(pos, color, font, codepoint); }
void gui::draw_shadowed_glyph(const int2 & pos, const float4 & color, const font_face & font, uint32_t codepoint) { return buf.draw_shadowed_glyph(pos, color, font, codepoint); }
void gui::draw_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text) { return buf.draw_text(pos, color, font, text); }
void gui::draw_text(const int2 & coords, const float4 & color, std::string_view text) { buf.draw_text(coords, color, style.def_font, text); }
void gui::draw_shadowed_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text) { return buf.draw_shadowed_text(pos, color, font, text); }
void gui::draw_shadowed_text(const int2 & coords, const float4 & color, std::string_view text) { buf.draw_shadowed_text(coords, color, style.def_font, text); }

bool gui::is_mouse_clicked() const { return state.clicked && current_id_prefix.root == state.cursor_window; }
bool gui::is_mouse_down() const { return state.down && current_id_prefix.root == state.cursor_window; }

bool gui::clickable_widget(const rect<int> & bounds)
{
    if(is_mouse_clicked() && is_cursor_over(bounds))
    {
        consume_click();
        return true;
    }
    return false;
}

bool gui::draggable_widget(int id, int2 dims, int2 & pos)
{
    if(is_focused(id))
    {
        if(is_mouse_down())
        {
            if(const int2 new_pos = local_cursor - state.clicked_offset; new_pos != pos)
            {
                pos = new_pos;
                return true;
            }
        }
        else clear_focus();
    }
    else if(clickable_widget({pos, pos+dims}))
    {
        state.clicked_offset = local_cursor - pos;
        set_focus(id);
    }
    return false;
}

bool gui::is_cursor_over(const rect<int> & r) const 
{ 
    return r.intersected_with(scissor_stack.back()).contains(local_cursor); 
}

static bool ids_equal(const std::vector<int> & a_prefix, int a_suffix, const std::vector<int> & b)
{
    if(a_prefix.size() + 1 != b.size()) return false;
    for(size_t i=0; i<a_prefix.size(); ++i) if(a_prefix[i] != b[i]) return false;
    return a_suffix == b.back();
}

bool gui::is_focused(int id) const 
{ 
    if(current_id_prefix.root != state.focus_id.root) return false;
    if(current_id_prefix.path.size() + 1 != state.focus_id.path.size()) return false;
    for(size_t i=0; i<current_id_prefix.path.size(); ++i) if(current_id_prefix.path[i] != state.focus_id.path[i]) return false;
    return id == state.focus_id.path.back();
}

bool gui::is_group_focused(int id) const
{
    if(current_id_prefix.root != state.focus_id.root) return false;
    if(current_id_prefix.path.size() + 2 > state.focus_id.path.size()) return false;
    for(size_t i=0; i<current_id_prefix.path.size(); ++i) if(current_id_prefix.path[i] != state.focus_id.path[i]) return false;
    return id == state.focus_id.path[current_id_prefix.path.size()];
}

void gui::clear_focus() { state.focus_id.root = nullptr; state.focus_id.path.clear(); }
void gui::set_focus(int id)
{
    state.focus_id = current_id_prefix;
    state.focus_id.path.push_back(id);
}

void gui::begin_text_entry(int id, const char * contents, bool selected)
{
    set_focus(id);
    state.text_entry_id = state.focus_id;
    state.text_entry = contents ? contents : "";
    state.text_entry_cursor = selected ? state.text_entry.size() : 0;
    state.text_entry_mark = 0;
    assert(utf8::is_valid(state.text_entry));

    // HACK: Avoid modifying the selection in subsequent calls to show_text_entry() this frame
    if(selected)
    {
        state.clicked = false;
        state.down = false;
    }
}

void gui::show_text_entry(const float4 & color, const rect<int> & rect)
{
    const auto w_cursor = style.def_font.get_text_width({state.text_entry.c_str(), state.text_entry_cursor});
    const auto w_mark = style.def_font.get_text_width({state.text_entry.c_str(), state.text_entry_mark});
    if(w_cursor != w_mark) draw_rect({rect.x0+std::min(w_cursor,w_mark), rect.y0, rect.x0+std::max(w_cursor,w_mark), rect.y0+style.def_font.line_height}, style.selection_background);
    draw_text({rect.x0,rect.y0}, color, state.text_entry);
    draw_rect({rect.x0+w_cursor, rect.y0, rect.x0+w_cursor+1, rect.y0+style.def_font.line_height}, style.active_text);

    if(state.down) state.text_entry_cursor = style.def_font.get_cursor_pos(state.text_entry, local_cursor.x - rect.x0);
    if(is_cursor_over(rect)) 
    {
        ctype = cursor_type::ibeam;
        if(state.clicked) 
        {
            state.text_entry_mark = state.text_entry_cursor;
            consume_click();
        }
    }
}

///////////
// Menus //
///////////

void gui::begin_menu(int id, const rect<int> & r)
{
    draw_rect(r, style.edit_background);

    menu_stack.clear();
    menu_stack.push_back({{r.x0+10, r.y0, r.x0+10, r.y1}, true});

    begin_group(id);
}

rect<int> gui::get_next_menu_item_rect(rect<int> & r, std::string_view caption)
{
    if(menu_stack.size() == 1)
    {
        const rect<int> item = {r.x1, r.y0 + (r.height() - style.def_font.line_height) / 2, r.x1 + style.def_font.get_text_width(caption), r.y0 + (r.height() + style.def_font.line_height) / 2};
        r.x1 = item.x1 + 30;
        return item;
    }
    else
    {
        const rect<int> item = {r.x0 + 4, r.y1, r.x0 + 196, r.y1 + style.def_font.line_height};
        r.x1 = std::max(r.x1, item.x1);
        r.y1 = item.y1 + 4;
        return item;    
    }
}

void gui::begin_popup(int id, std::string_view caption)
{
    auto & f = menu_stack.back();
    const rect<int> item = get_next_menu_item_rect(f.r, caption);

    if(f.open)
    {
        if(is_cursor_over(item)) draw_rect(item, {0.5f,0.5f,0,1});

        if(menu_stack.size() > 1)
        {
            draw_shadowed_text({item.x0+20, item.y0}, {1,1,1,1}, caption);
            draw_shadowed_text({item.x0+180, item.y0}, {1,1,1,1}, utf8::units(0xf0da).data());
        }
        else draw_shadowed_text({item.x0, item.y0}, {1,1,1,1}, caption);

        if(state.clicked && is_cursor_over(item))
        {
            set_focus(id);
            consume_click();
        }
    }
    
    if(menu_stack.size() == 1) menu_stack.push_back({{item.x0, item.y1, item.x0+200, item.y1+4}, is_focused(id) || is_group_focused(id)});
    else menu_stack.push_back({{item.x1-6, item.y0-1, item.x1+194, item.y0+3}, is_focused(id) || is_group_focused(id)});

    begin_overlay();
    begin_overlay();
    begin_group(id);
}

void gui::menu_seperator()
{
    if(menu_stack.size() < 2) return;
    auto & f = menu_stack.back();
    if(f.open) draw_rect({f.r.x0 + 4, f.r.y1 + 1, f.r.x0 + 196, f.r.y1 + 2}, {0.5,0.5,0.5,1});
    f.r.y1 += 6;
}

template<int N> struct fixed_string_buffer
{
    char buffer[N] {};
    int used {0};
    template<int M> void append(const char (& string)[M])
    {
        assert(string[M-1] == 0);
        assert(used + M-1 < N);
        memcpy(buffer + used, string, M-1);
        used += M-1;
    }
    void append(char ch)
    {
        assert(used + 1 < N);
        buffer[used] = ch;
        ++used;
    }
};

bool gui::menu_item(std::string_view caption, int mods, int key, uint32_t icon)
{
    if(key && key == state.key && mods == state.mods) return true;

    auto & f = menu_stack.back();
    const rect<int> item = get_next_menu_item_rect(f.r, caption);

    if(f.open)
    {
        if(is_cursor_over(item)) draw_rect(item, {0.5f,0.5f,0,1});
        if(icon) buf.draw_shadowed_glyph({item.x0, item.y0}, {1,1,1,1}, style.icon_font, icon);
        draw_shadowed_text({item.x0+20, item.y0}, {1,1,1,1}, caption);

        if(key)
        {
            fixed_string_buffer<64> hotkey_text; // Should not need more than 5+6+4+6+11+1=33 bytes
            if(mods & GLFW_MOD_CONTROL) hotkey_text.append("Ctrl+");
            if(mods & GLFW_MOD_SHIFT  ) hotkey_text.append("Shift+");
            if(mods & GLFW_MOD_ALT    ) hotkey_text.append("Alt+");
            if(mods & GLFW_MOD_SUPER  ) hotkey_text.append("Super+");
            if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z) hotkey_text.append(static_cast<char>('A' + key - GLFW_KEY_A));
            else if(key >= GLFW_KEY_0 && key <= GLFW_KEY_9) hotkey_text.append(static_cast<char>('0' + key - GLFW_KEY_0));
            else if(key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) 
            { 
                hotkey_text.append('F');
                if(key >= GLFW_KEY_F10) hotkey_text.append(static_cast<char>('0' + (key - 289)/10));
                hotkey_text.append(static_cast<char>('0' + (key - 289)%10));
            }
            else switch(key)
            {
            case GLFW_KEY_SPACE:        hotkey_text.append("Space");       break;
            case GLFW_KEY_APOSTROPHE:   hotkey_text.append('\'');          break;
            case GLFW_KEY_COMMA:        hotkey_text.append(',');           break;
            case GLFW_KEY_MINUS:        hotkey_text.append('-');           break;
            case GLFW_KEY_PERIOD:       hotkey_text.append('.');           break;
            case GLFW_KEY_SLASH:        hotkey_text.append('/');           break;
            case GLFW_KEY_SEMICOLON:    hotkey_text.append(';');           break;
            case GLFW_KEY_EQUAL:        hotkey_text.append('=');           break;
            case GLFW_KEY_LEFT_BRACKET: hotkey_text.append('[');           break;
            case GLFW_KEY_BACKSLASH:    hotkey_text.append('\\');          break;
            case GLFW_KEY_RIGHT_BRACKET:hotkey_text.append(']');           break;
            case GLFW_KEY_GRAVE_ACCENT: hotkey_text.append('`');           break;
            case GLFW_KEY_ESCAPE:       hotkey_text.append("Escape");      break;
            case GLFW_KEY_ENTER:        hotkey_text.append("Enter");       break;
            case GLFW_KEY_TAB:          hotkey_text.append("Tab");         break;
            case GLFW_KEY_BACKSPACE:    hotkey_text.append("Backspace");   break;
            case GLFW_KEY_INSERT:       hotkey_text.append("Insert");      break;
            case GLFW_KEY_DELETE:       hotkey_text.append("Delete");      break;
            case GLFW_KEY_RIGHT:        hotkey_text.append("Right");       break;
            case GLFW_KEY_LEFT:         hotkey_text.append("Left");        break;
            case GLFW_KEY_DOWN:         hotkey_text.append("Down");        break;
            case GLFW_KEY_UP:           hotkey_text.append("Up");          break;
            case GLFW_KEY_PAGE_UP:      hotkey_text.append("PageUp");      break;
            case GLFW_KEY_PAGE_DOWN:    hotkey_text.append("PageDown");    break;
            case GLFW_KEY_HOME:         hotkey_text.append("Home");        break;
            case GLFW_KEY_END:          hotkey_text.append("End");         break;
            case GLFW_KEY_CAPS_LOCK:    hotkey_text.append("CapsLock");    break;
            case GLFW_KEY_SCROLL_LOCK:  hotkey_text.append("ScrollLock");  break;
            case GLFW_KEY_NUM_LOCK:     hotkey_text.append("NumLock");     break;
            case GLFW_KEY_PRINT_SCREEN: hotkey_text.append("PrintScreen"); break;
            case GLFW_KEY_PAUSE:        hotkey_text.append("Pause");       break;
            default: throw std::logic_error("unsupported hotkey");
            }
            hotkey_text.append(0);
            draw_shadowed_text({item.x0 + 100, item.y0}, {1,1,1,1}, hotkey_text.buffer);
        }
        if(state.clicked && is_cursor_over(item))
        {
            clear_focus();
            consume_click();
            return true;
        }
    }

    return false;
}

void gui::end_popup()
{
    end_group();
    end_overlay();
    if(menu_stack.back().open)
    {
        const auto & r = menu_stack.back().r;
        draw_rect(r, {0.5f,0.5f,0.5f,1});
        draw_rect({r.x0+1, r.y0+1, r.x1-1, r.y1-1}, {0.2f,0.2f,0.2f,1});
    }
    end_overlay();
    menu_stack.pop_back();
}

void gui::end_menu()
{
    end_group();
}

/////////////////////////////////////
// Standard widget implementations //
/////////////////////////////////////

bool edit(gui & g, int id, const rect<int> & r, std::string & value)
{
    g.draw_rounded_rect(r, 3, g.get_style().edit_background);
    if(g.is_cursor_over(r))
    {
        g.set_cursor_type(cursor_type::ibeam);
        if(g.is_mouse_clicked() && !g.is_focused(id)) g.begin_text_entry(id, value.c_str());
    }

    if(g.is_focused(id))
    {
        g.show_text_entry(g.get_style().active_text, r.shrink(1));
        if(g.get_text_entry() != value)
        {
            value = g.get_text_entry();
            return true;
        }
    }
    else g.draw_text({r.x0+1,r.y0+1}, g.get_style().passive_text, value);
    return false;
}

template<class T, class F> bool edit_number(gui & g, int id, const rect<int> & r, T & value, const char * format, F parse)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), format, value);
    g.draw_rounded_rect(r, 3, g.get_style().edit_background);
    if(g.is_cursor_over(r))
    {
        g.set_cursor_type(cursor_type::ibeam);
        if(g.is_mouse_clicked() && !g.is_focused(id)) g.begin_text_entry(id, buffer, true);
    }

    if(g.is_focused(id))
    {
        char * end = 0;
        errno = 0;
        auto val = parse(g.get_text_entry().data(), &end);
        if(!errno && val == static_cast<T>(val) && end == g.get_text_entry().data() + g.get_text_entry().size())
        {
            g.show_text_entry(g.get_style().active_text, r.shrink(1));
            if(val != value)
            {
                value = static_cast<T>(val);
                return true;
            }
        }
        else g.show_text_entry(g.get_style().invalid_text, r.shrink(1));
    }
    else g.draw_text({r.x0+1,r.y0+1}, g.get_style().passive_text, buffer);
    return false;
}
bool edit(gui & g, int id, const rect<int> & r, int & value) { return edit_number(g, id, r, value, "%d", [](const char * s, char ** e) { return std::strtol(s, e, 10); }); }
bool edit(gui & g, int id, const rect<int> & r, unsigned & value) { return edit_number(g, id, r, value, "%d", [](const char * s, char ** e) { return std::strtoll(s, e, 10); }); }
bool edit(gui & g, int id, const rect<int> & r, float & value) { return edit_number(g, id, r, value, "%f", [](const char * s, char ** e) { return std::strtof(s, e); }); }
bool edit(gui & g, int id, const rect<int> & r, double & value) { return edit_number(g, id, r, value, "%f", [](const char * s, char ** e) { return std::strtod(s, e); }); }

static std::pair<rect<int>, rect<int>> splitter(gui & g, int id, const rect<int> & r, int & split_value, int (int2::*e), int (rect<int>::*e0), int (rect<int>::*e1), cursor_type ctype)
{
    // If the user is currently dragging the splitter, compute a new split value
    if(g.is_focused(id))
    {
        if(g.is_mouse_down())
        {
            if(g.get_cursor().*e < (r.*e0 + r.*e1) / 2) split_value = std::max(g.get_cursor().*e - r.*e0 - 2, 32);
            else split_value = std::min(g.get_cursor().*e - r.*e1 + 2, -32);
        }
        else g.clear_focus();
    }

    // Compute bounds for subregion a, the splitter itself, and subregion b
    rect<int> a=r, s=r, b=r;
    if(split_value > 0) b.*e0 = s.*e1 = (s.*e0 = a.*e1 = r.*e0 + split_value) + 4;
    else a.*e1 = s.*e0 = (s.*e1 = b.*e0 = r.*e1 + split_value) - 4;

    // Handle mouseover and click behavior for the splitter
    if(g.is_cursor_over(s))
    {      
        g.set_cursor_type(ctype);
        if(g.is_mouse_clicked())
        {
            g.set_focus(id);
            g.consume_click();
        }
    }

    // Return the two subregions
    return {a, b};
}
std::pair<rect<int>, rect<int>> hsplitter(gui & g, int id, const rect<int> & r, int & split_value) { return splitter(g, id, r, split_value, &int2::x, &rect<int>::x0, &rect<int>::x1, cursor_type::hresize); }
std::pair<rect<int>, rect<int>> vsplitter(gui & g, int id, const rect<int> & r, int & split_value) { return splitter(g, id, r, split_value, &int2::y, &rect<int>::y0, &rect<int>::y1, cursor_type::vresize); }

void vscroll(gui & g, int id, const rect<int> & r, int slider_size, int range_size, int & value)
{
    value = std::min(value, range_size - slider_size);
    value = std::max(value, 0);
    if(slider_size >= range_size || range_size == 0) return;
    const int track_height = r.height();
    g.draw_rounded_rect(r, 4, {0.25f,0.25f,0.25f,1});
    g.draw_rounded_rect({r.x0, r.y0+value*track_height/range_size, r.x1, r.y0+(value+slider_size)*track_height/range_size}, 4, {0.75f,0.75f,0.75f,1});
}

static bool is_subsequence(std::string_view seq, std::string_view sub)
{
    for(; !sub.empty(); sub.remove_prefix(1))
    {
        bool match = false;
        for(; !seq.empty(); seq.remove_prefix(1))
        {
            if(toupper(sub.front()) == toupper(seq.front()))
            {
                match = true;
                seq.remove_prefix(1);
                break;
            }
        }
        if(!match) return false;
    }
    return true;
}

bool combobox(gui & g, int id, const rect<int> & r, int num_items, function_view<std::string_view(int)> get_label, int & index)
{
    static int scroll_value = 0; // TODO: Fix this

    bool retval = false;
    g.draw_rounded_rect(r, 3, g.get_style().edit_background);
    if(g.is_focused(id))
    {    
        g.show_text_entry({1,1,0,1}, r.shrink(1));
        if(g.get_text_entry().empty() && index >= 0 && index < num_items) g.draw_text({r.x0+1,r.y0+1}, {1,1,1,0.5f}, get_label(index));

        rect<int> r2 = {r.x0,r.y1,r.x1,r.y1};

        int2 p = {r2.x0, r2.y0 - scroll_value};
        int client_height = 0;

        auto r3 = r2.adjusted(0, 0, 0, g.get_style().def_font.line_height*11/2);
        g.begin_overlay();
        g.begin_scissor(r3);
        g.begin_overlay();

        for(int i=0; i<num_items; ++i)
        {
            std::string_view label = get_label(i);
            if(is_subsequence(label, g.get_text_entry()))
            {
                g.draw_text(p, i == index ? g.get_style().active_text : g.get_style().passive_text, label);

                if(g.is_mouse_clicked() && g.is_cursor_over({r.x0,p.y,r.x1,p.y+g.get_style().def_font.line_height}))
                {
                    g.clear_focus();
                    g.consume_click();
                    index = i;
                    retval = true;
                }

                p.y += g.get_style().def_font.line_height;
                client_height += g.get_style().def_font.line_height;
            }
        }

        g.end_overlay();
        r2.y1 = std::min(p.y + g.get_style().def_font.line_height, r3.y1);
        g.draw_rect(r2, g.get_style().popup_background);

        scroll_value -= g.get_scroll().y;
        vscroll(g, 100, {r2.x1-10,r2.y0,r2.x1,r2.y1}, r2.y1-r2.y0, client_height, scroll_value);

        g.end_scissor();
        g.end_overlay();

        // If the user clicks outside of the combobox, remove focus
        if(g.is_mouse_clicked() && !g.is_cursor_over(r) && !g.is_cursor_over(r2)) g.clear_focus();
    }
    else 
    {
        if(index >= 0 && index < num_items) g.draw_text({r.x0+1,r.y0+1}, {1,1,1,1}, get_label(index));

        if(g.is_mouse_clicked() && g.is_cursor_over(r))
        {
            g.begin_text_entry(id);
            g.consume_click();            
        }
    }
    return retval;
}

bool icon_combobox(gui & g, int id, const rect<int> & r, int num_items, function_view<std::string_view(int)> get_label, function_view<void(int, const rect<int> &)> draw_icon, int & index)
{
    static int scroll_value = 0; // TODO: Fix this

    bool retval = false;
    g.draw_rounded_rect(r, 3, g.get_style().edit_background);
    if(g.is_focused(id))
    {    
        g.show_text_entry({1,1,0,1}, r.shrink(1));
        if(g.get_text_entry().empty() && index >= 0 && index < num_items) g.draw_text({r.x0+1,r.y0+1}, {1,1,1,0.5f}, get_label(index));

        const int ICON_SIZE = 48, ICON_PADDING = 16, HORIZONTAL_SPACING = ICON_SIZE + ICON_PADDING*2, VERTICAL_SPACING = ICON_SIZE + g.get_style().def_font.line_height + 12;
        rect<int> r2 {r.x0, r.y1, r.x1, r.y1 + 8};
        int2 p {r2.x0, r2.y1 - scroll_value};
        int client_height = 0;

        auto r3 = r2.adjusted(0, 0, 0, VERTICAL_SPACING*7/2);
        g.begin_overlay();
        g.begin_scissor(r2);
        g.begin_overlay();

        for(int i=0; i<num_items; ++i)
        {
            if(p.x > r2.x0 && p.x + HORIZONTAL_SPACING >= r2.x1)
            {
                p.x = r2.x0;
                p.y += VERTICAL_SPACING;
                client_height += VERTICAL_SPACING;            
            }

            std::string_view label = get_label(i);
            if(is_subsequence(label, g.get_text_entry()))
            {
                draw_icon(i, {p+int2(ICON_PADDING,0), p+ICON_SIZE+int2(ICON_PADDING,0)});
                g.draw_text(p+int2((HORIZONTAL_SPACING - g.get_style().def_font.get_text_width(label))/2, ICON_SIZE+4), i == index ? g.get_style().active_text : g.get_style().passive_text, label);

                if(g.is_mouse_clicked() && g.is_cursor_over({p,p+int2(HORIZONTAL_SPACING,VERTICAL_SPACING)})) 
                {
                    g.clear_focus();
                    g.consume_click();
                    index = i;
                    retval = true;
                }

                p.x += HORIZONTAL_SPACING;
            }
        }

        g.end_overlay();
        r2.y1 = std::min(p.y + VERTICAL_SPACING, r3.y1);
        g.draw_rect(r2, g.get_style().popup_background);

        scroll_value -= g.get_scroll().y;
        vscroll(g, 99, {r2.x1-10,r2.y0,r2.x1,r2.y1}, r2.y1-r2.y0, client_height, scroll_value);

        g.end_scissor();
        g.end_overlay();

        // If the user clicks outside of the combobox, remove focus
        if(g.is_mouse_clicked() && !g.is_cursor_over(r) && !g.is_cursor_over(r2)) g.clear_focus();
    }
    else
    {
        if(index >= 0 && index < num_items) g.draw_text({r.x0+1,r.y0+1}, {1,1,1,1}, get_label(index));

        if(g.is_mouse_clicked() && g.is_cursor_over(r))
        {
            g.begin_text_entry(id);
            g.consume_click();            
        }
    }
    return retval;
}

void gui::focus_window() 
{ 
    if(state.cursor_window != current_id_prefix.root)
    {
        state.cursor_window = current_id_prefix.root;
        state.clicked = false;
        state.down = false;
        glfwFocusWindow(state.cursor_window); 
    }
}

rect<int> tabbed_container(gui & g, rect<int> bounds, array_view<std::string_view> captions, size_t & active_tab)
{
    auto cap_bounds = bounds.take_y0(g.get_style().def_font.line_height + 4);
    g.draw_wire_rect(bounds, 1, g.get_style().frame_color);

    for(size_t i=0; i<captions.size(); ++i)
    {
        bool active = active_tab == i;
        auto r = cap_bounds.take_x0(g.get_style().def_font.get_text_width(captions[i]) + 24);
        if(g.is_mouse_clicked() && g.is_cursor_over(r))
        {
            g.consume_click();
            active_tab = i;
        }

        g.draw_partial_rounded_rect(r, 10, top_left_corner|top_right_corner, g.get_style().frame_color);
        g.draw_partial_rounded_rect(r.adjusted(1, 1, -1, active ? 1 : 0), 9, top_left_corner|top_right_corner, active ? g.get_style().edit_background : g.get_style().popup_background);
        g.draw_shadowed_text({r.x0 + 11, r.y0 + 3}, active ? g.get_style().active_text : g.get_style().passive_text, captions[i]);
    }
   
    return bounds.shrink(1);
}

///////////////////////////////////////
// Platform specific implementations //
///////////////////////////////////////

#ifdef _WIN32
#undef APIENTRY
#include <Windows.h>
bool gui::is_foreign_mouse_down() const { return (GetKeyState(VK_LBUTTON) & 0x100) != 0; }
#else
#error Platform specific functions not implemented for this platform. Pull requests are welcome!
#endif