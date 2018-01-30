 // A small immediate mode GUI framework
#pragma once
#include "sprite.h" // For sprite_sheet, font_face, canvas

// GUI state which persists from frame to frame
struct GLFWwindow;
enum class cursor_type { arrow, hresize, vresize, ibeam };
struct widget_id { GLFWwindow * root; std::vector<int> path; };
class gui_state
{
    friend class gui;

    // Focus state
    widget_id focus_id;                 // The ID of the widget which is in focus (see clear_focus()/set_focus()/is_focused())
    int2 clicked_offset;                // The offset of the cursor within the focused widget at the time the widget was clicked

    // Text entry state
    widget_id text_entry_id;            // The ID of the widget currently being used for text entry
    std::string text_entry;             // The current text in the active text entry widget
    size_t text_entry_cursor;           // The location of the cursor in the text entry widget
    size_t text_entry_mark;             // The location of the start of a selection in the text entry widget

    // Input state
    GLFWwindow * cursor_window;         // The window which the cursor is currently over
    bool clicked, down;                 // True if the mouse was clicked during this frame, and if the mouse is currently down
    int2 scroll;                        // Scroll amount in the current frame
    int key, mods;                      // Key pressed during this frame, and corresponding mods

    void delete_text_entry_selection();
public:
    // Facilities for injecting input events
    void begin_frame();
    void on_scroll(GLFWwindow * window, double x, double y);
    void on_mouse_button(GLFWwindow * window, int button, int action, int mods);
    void on_key(GLFWwindow * window, int key, int action, int mods);
    void on_char(GLFWwindow * window, uint32_t codepoint);
};

// Immediate mode GUI context
struct gui_style
{
    const font_face & def_font;                               // default font used for text
    const font_face & icon_font;                              // default font used for icons

    float4 panel_background     {0.10f, 0.10f, 0.10f, 1.00f}; // background of a panel on which controls are situated
    float4 popup_background     {0.15f, 0.15f, 0.15f, 1.00f}; // background of a popup menu or overlay
    float4 edit_background      {0.20f, 0.20f, 0.20f, 1.00f}; // background of a field which can be modified
    float4 selection_background {1.00f, 1.00f, 0.00f, 0.50f}; // mouseover background in a list, selection background in a text entry field
    float4 frame_color          {0.40f, 0.40f, 0.40f, 1.00f}; // color used for thin borders and details

    float4 passive_text         {0.65f, 0.65f, 0.65f, 1.00f}; // labels, unselected items in a list, unfocused text entry fields
    float4 active_text          {1.00f, 1.00f, 1.00f, 1.00f}; // selected items in a list, focused text entry fields
    float4 invalid_text         {1.00f, 0.00f, 0.00f, 1.00f}; // text entry fields which fail to parse    
};
class gui
{   
    struct menu_stack_frame { rect<int> r; bool open; };
    gui_state & state;
    canvas & buf;
    gui_style style;
    GLFWwindow * window;                // The GLFWwindow whose GUI we are generating
    int2 local_cursor;                  // The current cursor position local to this window

    int current_layer;
    std::vector<rect<int>> scissor_stack;
    std::vector<menu_stack_frame> menu_stack;    
    widget_id current_id_prefix;        // The prefix applied to IDs in the current widget group (see begin_group()/end_group())
    cursor_type ctype;                  // Which cursor icon to display for this frame
public:
    gui(gui_state & state, canvas & canvas, const gui_style & style, GLFWwindow * window);

    // The gui context has an associated "style" struct which is used to control various visual elements
    const gui_style & get_style() const { return style; }

    // Widgets drawn inside an overlay are drawn on top of other widgets, and are not clipped by previously defined scissor rects
    void begin_overlay();
    void end_overlay();

    // Widgets drawn inside a scissor are clipped to the specified rectangle
    void begin_scissor(const rect<int> & r);
    void end_scissor();

    // Draw commands are forwarded to the underlying canvas
    void draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color);
    void draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color);
    void draw_wire_rect(const rect<int> & r, int width, const float4 & color);

    void draw_rect(const rect<int> & r, const float4 & color);
    void draw_circle(const int2 & center, int radius, const float4 & color);
    void draw_rounded_rect(const rect<int> & r, int corner_radius, const float4 & color);
    void draw_partial_rounded_rect(const rect<int> & r, int corner_radius, corner_flags corners, const float4 & color);
    void draw_convex_polygon(array_view<ui_vertex> vertices);

    void draw_sprite(const rect<int> & r, const float4 & color, const rect<float> & texcoords);
    void draw_sprite_sheet(const int2 & p);

    void draw_glyph(const int2 & pos, const float4 & color, const font_face & font, uint32_t codepoint);
    void draw_shadowed_glyph(const int2 & pos, const float4 & color, const font_face & font, uint32_t codepoint);
    void draw_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text);
    void draw_text(const int2 & pos, const float4 & color, std::string_view text);
    void draw_shadowed_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text);
    void draw_shadowed_text(const int2 & pos, const float4 & color, std::string_view text);
    void draw_image(const rect<int> & bounds, const float4 & color, rhi::image & image);

    // Facilities for consuming mouse clicks
    GLFWwindow * get_cursor_window() const { return state.cursor_window; }
    const int2 & get_cursor() const { return local_cursor; } // NOTE: Prefer is_cursor_over(...) for doing mouseover checks
    bool is_mouse_clicked() const; // True if the mouse has been clicked on this window in this frame
    bool is_mouse_down() const; // True if the mouse button is held down and the original click was on this window
    bool is_cursor_over(const rect<int> & r) const;
    void consume_click() { state.clicked = false; }
    int2 get_scroll() const { return state.scroll; }

    // Facilities for consuming mouse input which originated in other windows
    void focus_window();
    bool is_foreign_mouse_down() const; // True if the mouse button is held down, regardless of which window currently has focus

    // Facilities for manipulating and querying which widget (identified by unique integer IDs) have focus
    void begin_group(int id) { current_id_prefix.path.push_back(id); }
    void end_group() { current_id_prefix.path.pop_back(); }
    bool is_focused(int id) const;          // Return true if set_focus(id) has been called and clear_focus() has not been called
    bool is_group_focused(int id) const;    // Return true if any widget between a begin_group(id)/end_group() call has focused
    void clear_focus();
    void set_focus(int id);

    // Facilities for implementing widgets that require some form of text entry
    void begin_text_entry(int id, const char * contents = nullptr, bool selected = false);
    void show_text_entry(const float4 & color, const rect<int> & rect);
    const std::string & get_text_entry() const { return state.text_entry; }

    // Facilities for creating menus
    void begin_menu(int id, const rect<int> & r);
    rect<int> get_next_menu_item_rect(rect<int> & r, std::string_view caption);
    void begin_popup(int id, std::string_view caption);
    void menu_seperator();
    bool menu_item(std::string_view caption, int mods, int key, uint32_t icon);
    void end_popup();
    void end_menu();

    // Facilities for drawing widgets
    cursor_type get_cursor_type() const { return ctype; }
    void set_cursor_type(cursor_type type) { ctype = type; }

    // Standard widgets
    bool clickable_widget(const rect<int> & bounds);        // Returns true if clicked, and consumes the click
    bool draggable_widget(int id, int2 dims, int2 & pos);   // Returns true if dragged, and modifies pos accordingly
};

// A text entry field allowing the user to edit the value of a utf-8 encoded string.
bool edit(gui & g, int id, const rect<int> & r, std::string & value);

// A text entry field allowing the user to edit the value of a number.
// TODO: Expand this out to the full set of C/C++ integer types
bool edit(gui & g, int id, const rect<int> & r, int & value);
bool edit(gui & g, int id, const rect<int> & r, unsigned & value);
bool edit(gui & g, int id, const rect<int> & r, float & value);
bool edit(gui & g, int id, const rect<int> & r, double & value);

// A text entry field allowing the user to edit the value of a vector, by editing individual components
template<class T, int M> bool edit(gui & g, int id, const rect<int> & r, linalg::vec<T,M> & value)
{
    bool retval = false;
    const int x0 = r.x0, x1 = r.x1 - (M-1)*4;
    g.begin_group(id);
    for(int i=0; i<M; ++i) retval |= edit(g, i, {(x0*(M-i)+x1*i)/M + i*4, r.y0, (x0*(M-1-i)+x1*(i+1))/M + i*4, r.y1}, value[i]);
    g.end_group();
    return retval;
}

// A user-draggable splitter widget which divides a single region into a left and right subregion. When split_value 
// is positive, it defines the width in pixels of the left subregion, when negative, its magnitude defines the width
// in pixels of the right subregion, with the other region occupying whatever space is left over.
std::pair<rect<int>, rect<int>> hsplitter(gui & g, int id, const rect<int> & r, int & split_value);

// A user-draggable splitter widget which divides a single region into a top and bottom subregion. When split_value 
// is positive, it defines the height in pixels of the top subregion, when negative, its magnitude defines the height
// in pixels of the bottom subregion, with the other region occupying whatever space is left over.
std::pair<rect<int>, rect<int>> vsplitter(gui & g, int id, const rect<int> & r, int & split_value);

bool combobox(gui & g, int id, const rect<int> & r, int num_items, function_view<std::string_view(int)> get_label, int & index);
template<class T, class F> 
bool combobox(gui & g, int id, const rect<int> & r, array_view<T> items, F get_label, T & value)
{
    int index = std::find(items.begin(), items.end(), value) - items.begin();
    bool b = combobox(g, id, r, items.size(), [=](int i) { return get_label(items[i]); }, index);
    if(b) value = items[index];
    return b;
}

bool icon_combobox(gui & g, int id, const rect<int> & r, int num_items, function_view<std::string_view(int)> get_label, function_view<void(int, const rect<int> &)> draw_icon, int & index);
template<class T, class F, class G>
bool icon_combobox(gui & g, int id, const rect<int> & r, array_view<T> items, F get_label, G draw_icon, T & value)
{
    int index = std::find(items.begin(), items.end(), value) - items.begin();
    bool b = icon_combobox(g, id, r, items.size(), [=](int i) { return get_label(items[i]); }, [=](int i, const rect<int> & r) { draw_icon(items[i], r); }, index);
    if(b) value = items[index];
    return b;
}

rect<int> tabbed_container(gui & g, rect<int> bounds, array_view<std::string_view> captions, size_t & active_tab);