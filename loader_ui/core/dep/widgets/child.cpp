#include"../headers/includes.h"
#include "../headers/widgets.h"

void c_widgets::games_child(std::string_view widgets_id, std::string_view name, std::string_view icon, std::string_view count)
{
    struct child_state
    {
        float height{ 0 };
        float width{ 0 };
    };

    ImGuiWindow* window = gui->get_window();
    const ImVec2 pos = window->DC.CursorPos;

    child_state* state = gui->anim_container<child_state>(window->GetID(widgets_id.data()));

    state->width = gui->content_avail().x;

    draw->text_clipped(window->DrawList, font->get(icons_data, 10), pos, pos + SCALE(elements->child.icon_zone_width, elements->child.top_height), draw->get_clr(clr->main.text, 0.24), icon.data(), NULL, NULL, ImVec2(0.5f, 0.5f));
    draw->text_clipped(window->DrawList, font->get(suisse_intl_semi_bold_data, 13), pos + SCALE(elements->child.icon_zone_width + elements->child.text_spacing, 0), pos + ImVec2(state->width, SCALE(elements->child.top_height)), draw->get_clr(clr->main.text, 0.48), name.data(), gui->text_end(name.data()), NULL, ImVec2(0.f, 0.5f));
    draw->text_clipped(window->DrawList, font->get(suisse_intl_semi_bold_data, 13), pos, pos + ImVec2(state->width, SCALE(elements->child.top_height)), draw->get_clr(clr->main.text, 0.24), count.data(), gui->text_end(count.data()), NULL, ImVec2(1.f, 0.5f));
    draw->text_clipped(window->DrawList, font->get(icons_data, 8), pos, pos + ImVec2(state->width - gui->text_size(font->get(suisse_intl_semi_bold_data, 13), count.data()).x - SCALE(elements->child.text_spacing), SCALE(elements->child.top_height)), draw->get_clr(clr->main.text, 0.24), "E", NULL, NULL, ImVec2(1.f, 0.5f));

    gui->push_var(style_var_window_padding, SCALE(0, 0));
    gui->set_next_window_pos(pos + ImVec2(0, SCALE(elements->widgets.spacing.y + elements->child.top_height)));
    gui->begin_def_child(widgets_id.data(), ImVec2(state->width, state->height), 0, window_flags_always_use_window_padding | window_flags_no_move | window_flags_nav_flattened | window_flags_no_saved_settings | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse | window_flags_no_background);
    gui->push_var(style_var_item_spacing, SCALE(elements->widgets.spacing));

    state->height = gui->get_window()->ContentSize.y;
}

void c_widgets::games_end_child()
{
    gui->pop_var();
    gui->end_def_child();
    gui->pop_var();
}

void c_widgets::widgets_child(std::string_view widgets_id, std::string_view name, std::string_view icon)
{
    struct child_state
    {
        float height{ 0 };
        float width{ 0 };
        bool clicked{ false };
        float rotate{ 0.f };
    };

    ImGuiWindow* window = gui->get_window();
    const ImVec2 pos = window->DC.CursorPos;
    ImGuiContext& g = *GImGui;

    child_state* state = gui->anim_container<child_state>(window->GetID(widgets_id.data()));
    const ImRect icon_rect(pos + ImVec2(state->width - SCALE(elements->child.padding.x), 0), pos + ImVec2(state->width, SCALE(elements->child.top_height)));

    state->width = gui->content_avail().x;

    if (icon_rect.Contains(g.IO.MousePos) && g.IO.MouseClicked[0])
        state->clicked = true;
    if (state->rotate >= 0.99)
        state->clicked = false;
    
    gui->easing(state->rotate, state->clicked ? 1.f : -0.5f, 8.f, static_easing);

    draw->text_clipped(window->DrawList, font->get(icons_data, 10), pos, pos + SCALE(elements->child.icon_zone_width, elements->child.top_height), draw->get_clr(clr->main.text, 0.24), icon.data(), NULL, NULL, ImVec2(0.5f, 0.5f));
    draw->text_clipped(window->DrawList, font->get(suisse_intl_semi_bold_data, 13), pos + SCALE(elements->child.icon_zone_width + elements->child.text_spacing, 0), pos + ImVec2(state->width, SCALE(elements->child.top_height)), draw->get_clr(clr->main.text, 0.48), name.data(), gui->text_end(name.data()), NULL, ImVec2(0.f, 0.5f));
    draw->rotate_start(window->DrawList);
    draw->text_clipped(window->DrawList, font->get(icons_data, 10), icon_rect.Min, icon_rect.Max, draw->get_clr(clr->main.text, 0.24), "R", NULL, NULL, ImVec2(0.5f, 0.5f));
    draw->rotate_end(window->DrawList, -180 * state->rotate, icon_rect.GetCenter());

    gui->push_var(style_var_window_padding, SCALE(elements->child.padding));
    gui->set_next_window_pos(pos + ImVec2(0, SCALE(elements->widgets.spacing.y + elements->child.top_height)));
    gui->begin_def_child(widgets_id.data(), ImVec2(state->width, state->height), 0, window_flags_always_use_window_padding | window_flags_no_move | window_flags_nav_flattened | window_flags_no_saved_settings | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse | window_flags_no_background);
    gui->push_var(style_var_item_spacing, SCALE(0, elements->widgets.spacing.y));
    draw->rect_filled(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->main.text, 0.02), SCALE(elements->widgets.rounding));
    draw->rect(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->main.text, 0.04), SCALE(elements->widgets.rounding));

    state->height = gui->get_window()->ContentSize.y + SCALE(elements->child.padding.y * 2);
}

void c_widgets::widgets_end_child()
{
    gui->pop_var();
    gui->end_def_child();
    gui->pop_var();
}