#include"../headers/includes.h"
#include "../headers/widgets.h"

void c_widgets::info_card(std::string_view widgets_id, std::string_view icon, std::string_view name, std::string_view desc, float width)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(widgets_id.data());

    ImVec2 pos = window->DC.CursorPos;

    const ImRect total(pos, pos + ImVec2(width, SCALE(elements->info_card.height)));
    const ImRect icon_zone(total.Min + SCALE(elements->info_card.padding), total.Min + SCALE(elements->info_card.padding + elements->info_card.icon_zone_size));
    ItemSize(total, style.FramePadding.y);
    if (!ItemAdd(total, id))
        return;

    draw->rect_filled(window->DrawList, total.Min, total.Max, draw->get_clr(clr->main.text, 0.02), SCALE(elements->widgets.rounding));
    draw->rect(window->DrawList, total.Min, total.Max, draw->get_clr(clr->main.text, 0.04), SCALE(elements->widgets.rounding));
    draw->text_clipped(window->DrawList, font->get(icons_data, (icon == "E" ? 26 : 30)), icon_zone.Min, icon_zone.Max, draw->get_clr(clr->main.accent), icon.data(), NULL, NULL, ImVec2(0.5f, 0.5f));

    draw->text_clipped(window->DrawList, font->get(suisse_intl_medium_data, 16), ImVec2(icon_zone.Max.x + SCALE(elements->widgets.spacing.x), icon_zone.Min.y + SCALE(2)), total.Max, draw->get_clr(clr->main.text), name.data(), gui->text_end(name.data()));
    draw->text_clipped(window->DrawList, font->get(suisse_intl_medium_data, 14), ImVec2(icon_zone.Max.x + SCALE(elements->widgets.spacing.x), icon_zone.Min.y), ImVec2(total.Max.x, icon_zone.Max.y + SCALE(2)), draw->get_clr(clr->main.text, 0.72), desc.data(), gui->text_end(desc.data()), NULL, ImVec2(0.f, 1.f));
};

void c_widgets::version_card(std::string_view widgets_id, std::string_view name, std::string_view update, int img_id)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(widgets_id.data());

    ImVec2 pos = window->DC.CursorPos;

    const ImRect total(pos, pos + ImVec2(gui->content_avail().x, SCALE(elements->version_card.rect_size.y)));
    const ImRect img_zone(total.Min, total.Min + SCALE(elements->version_card.rect_size));
    ItemSize(total, style.FramePadding.y);
    if (!ItemAdd(total, id))
        return;

    draw->image_rounded(window->DrawList, var->gui.img_for_versions[img_id], img_zone.Min, img_zone.Max, ImVec2(0, 0), ImVec2(1, 1), draw->get_clr({1.f, 1.f, 1.f, 1.f}), SCALE(elements->version_card.rounding));
    draw->text_clipped(window->DrawList, font->get(suisse_intl_regular_data, 14), ImVec2(img_zone.Max.x + SCALE(elements->version_card.padding), total.Min.y - SCALE(1)), total.Max, draw->get_clr(clr->main.text, 0.48), ("[" + std::string(name) + "]").data(), NULL, NULL, ImVec2(0.f, 0.5f));
    draw->text_clipped(window->DrawList, font->get(suisse_intl_regular_data, 14), ImVec2(img_zone.Max.x + SCALE(elements->version_card.padding * 2) + gui->text_size(font->get(suisse_intl_regular_data, 14), ("[" + std::string(name) + "]").data()).x, total.Min.y - SCALE(1)), total.Max, draw->get_clr(clr->main.text, 0.72), update.data(), NULL, NULL, ImVec2(0.f, 0.5f));

};

void c_widgets::top_bar(std::string_view url, std::string_view date)
{
    struct top_bar_state
    {
        float alpha{ 0.f };
        bool clicked{ false };
        ImVec4 icon{ clr->main.accent };
    };

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID("top_bar");

    top_bar_state* state = gui->anim_container<top_bar_state>(id);

    ImVec2 pos = window->DC.CursorPos;

    const ImRect total(pos, pos + ImVec2(gui->content_max().x - SCALE(elements->widgets.spacing.x + elements->window.padding.x + elements->top_bar.height), SCALE(elements->top_bar.height)));
    const ImRect icon_zone(total.Min, total.Min + SCALE(elements->top_bar.padding * 2 + elements->top_bar.icon_size));
    ItemSize(total, style.FramePadding.y);
    if (!ItemAdd(total, id))
        return;

    if (total.Contains(g.IO.MousePos) && g.IO.MouseClicked[0])
    {
        state->clicked = true;
    }
    if (state->alpha <= 0.41)
    {
        state->clicked = false;
        gui->open_url(url.data());
    }

    gui->easing(state->alpha, state->clicked ? 0.4f : 1.f, 2.f, static_easing);
    gui->easing(state->icon, state->clicked ? clr->main.text.Value : clr->main.accent.Value, 12.f, dynamic_easing);

    draw->rect_filled(window->DrawList, total.Min, total.Max, draw->get_clr(clr->main.text, 0.02), SCALE(elements->widgets.rounding));
    draw->rect(window->DrawList, total.Min, total.Max, draw->get_clr(clr->main.text, 0.04), SCALE(elements->widgets.rounding), 0, SCALE(1));
    draw->text_clipped(window->DrawList, font->get(icons_data, 17), icon_zone.Min, icon_zone.Max, draw->get_clr(state->icon, state->alpha), "A", NULL, NULL, ImVec2(0.5f, 0.5f));
    
    draw->circle_filled(window->DrawList, ImVec2(icon_zone.Max.x, total.GetCenter().y), SCALE(1), draw->get_clr(clr->main.text, 0.12), 30);

    draw->text_clipped(window->DrawList, font->get(icons_data, 12), ImVec2(icon_zone.Max.x + SCALE(elements->top_bar.padding.x), total.Min.y), ImVec2(icon_zone.Max.x + SCALE(elements->top_bar.padding.x + elements->top_bar.icon_2_zone_width), total.Max.y), draw->get_clr(clr->main.accent), "B", NULL, NULL, ImVec2(0.5f, 0.5f));
    draw->text_clipped(window->DrawList, font->get(suisse_intl_medium_data, 16), ImVec2(icon_zone.Max.x + SCALE(elements->top_bar.padding.x + elements->top_bar.icon_2_zone_width + elements->top_bar.texts_spacing), total.Min.y - SCALE(1)), total.Max, draw->get_clr(clr->main.text), url.data(), NULL, NULL, ImVec2(0.f, 0.5f));
    draw->text_clipped(window->DrawList, font->get(suisse_intl_medium_data, 16), total.Min - SCALE(0, 1), total.Max - SCALE(elements->top_bar.padding.x, 0), draw->get_clr(clr->main.text, 0.48), date.data(), NULL, NULL, ImVec2(1.f, 0.5f));
    draw->text_clipped(window->DrawList, font->get(icons_data, 12), ImVec2(total.Max.x - SCALE(elements->top_bar.padding.x + elements->top_bar.texts_spacing + elements->top_bar.icon_2_zone_width) - gui->text_size(font->get(suisse_intl_medium_data, 16), date.data()).x, total.Min.y), ImVec2(total.Max.x - SCALE(elements->top_bar.padding.x + elements->top_bar.texts_spacing) - gui->text_size(font->get(suisse_intl_medium_data, 16), date.data()).x, total.Max.y), draw->get_clr(clr->main.text, 0.24), "V", NULL, NULL, ImVec2(0.5f, 0.5f));
};

bool c_widgets::game_card(std::string_view widgets_id, std::string_view name, std::string_view count, int img_id, bool ready)
{
    struct game_card_state
    {
        float alpha[2]{ 1.f, 0.f };
        bool clicked{ false };
    };

    ImGuiWindow* window = gui->get_window();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(widgets_id.data());

    const ImVec2 pos = window->DC.CursorPos;

    const ImRect total(pos, pos + ImVec2((gui->content_max().x - SCALE(elements->widgets.spacing.x * 2)) / 3, SCALE(elements->game_card.height)));
    const ImRect button(total.Max - SCALE(elements->game_card.padding + elements->game_card.rect_size), total.Max - SCALE(elements->game_card.padding));
    const ImRect img_zone(total.Min + SCALE(elements->game_card.padding), ImVec2(total.Max.x - SCALE(elements->game_card.padding.x), total.Min.y + SCALE(elements->game_card.padding.y + elements->game_card.img_height)));
    
    gui->item_size(total, style.FramePadding.y);
    if (!gui->item_add(total, id))
        return false;

    bool hovered = gui->item_hoverable(button, id, g.LastItemData.InFlags);
    bool pressed = hovered && g.IO.MouseClicked[0];
    game_card_state* state = gui->anim_container<game_card_state>(id);

    if (pressed)
        state->clicked = true;
    if (state->alpha[0] <= 0.11)
    {
        state->clicked = false;
        if (ready)
            var->gui.stage_count = 2 + img_id;
        if(!ready)
            gui->open_url("https://www.authentication.club/");
    }

    gui->easing(state->alpha[0], state->clicked ? 0.1f : 1.f, 6.f, static_easing);
    gui->easing(state->alpha[1], state->clicked ? 0.6f : 0.f, 6.f, static_easing);

    draw->rect_filled(window->DrawList, total.Min, total.Max, draw->get_clr(clr->main.text, 0.02), SCALE(elements->widgets.rounding));
    draw->rect(window->DrawList, total.Min, total.Max, draw->get_clr(clr->main.text, 0.04), SCALE(elements->widgets.rounding));

    ID3D11ShaderResourceView* card_texture = nullptr;
    if (img_id >= 0 && static_cast<size_t>(img_id) < var->gui.subscription_textures.size()) {
        card_texture = var->gui.subscription_textures[static_cast<size_t>(img_id)];
    }
    if (!card_texture && img_id >= 0 && img_id < IM_ARRAYSIZE(var->gui.games)) {
        card_texture = var->gui.games[img_id];
    }

    if (card_texture) {
        draw->image_rounded(window->DrawList, card_texture, img_zone.Min, img_zone.Max, ImVec2(0, 0), ImVec2(1, 1), draw->get_clr({ 1.f, 1.f, 1.f, 1.f }), SCALE(elements->game_card.img_rounding));
    }
    else {
        draw->rect_filled(window->DrawList, img_zone.Min, img_zone.Max, draw->get_clr(clr->main.text, 0.05), SCALE(elements->game_card.img_rounding));
    }
    if(ready)
    {
        draw->rect_filled(window->DrawList, button.Min, button.Max, draw->get_clr(clr->main.accent, state->alpha[0]), SCALE(elements->game_card.img_rounding));
        draw->rect(window->DrawList, button.Min, button.Max, draw->get_clr(clr->main.text, 0.48), SCALE(elements->game_card.img_rounding), SCALE(1));
        draw->text_clipped(window->DrawList, font->get(icons_data, 11), button.Min, button.Max, draw->get_clr(clr->main.text), "E", NULL, NULL, ImVec2(0.5f, 0.5f));
        draw->text_clipped(window->DrawList, font->get(suisse_intl_medium_data, 16), ImVec2(total.Min.x + SCALE(elements->game_card.padding.x), button.Min.y), total.Max, draw->get_clr(clr->main.text), name.data(), gui->text_end(name.data()));
        draw->text_clipped(window->DrawList, font->get(icons_data, 8), ImVec2(total.Min.x + SCALE(elements->game_card.padding.x), button.Max.y - SCALE(elements->game_card.icon_size.y)), ImVec2(total.Min.x + SCALE(elements->game_card.padding.x + elements->game_card.icon_size.x), button.Max.y), draw->get_clr(clr->main.text, 0.48), "E", NULL, NULL, ImVec2(0.5f, 0.5f));
        draw->text_clipped(window->DrawList, font->get(suisse_intl_medium_data, 14), ImVec2(total.Min.x + SCALE(elements->game_card.padding.x + elements->game_card.icon_size.x + elements->game_card.text_spacing), button.Max.y - SCALE(elements->game_card.icon_size.y)), button.Max, draw->get_clr(clr->main.text, 0.72), count.data(), NULL, NULL, ImVec2(0.f, 0.5f));
        draw->rect(window->DrawList, img_zone.Min, img_zone.Max, draw->get_clr(clr->main.text, 0.12), SCALE(elements->game_card.img_rounding));
    }
    else
    {
        draw->rect_filled(window->DrawList, button.Min, button.Max, draw->get_clr(clr->main.text, 0.04), SCALE(elements->game_card.img_rounding));
        draw->rect_filled(window->DrawList, button.Min, button.Max, draw->get_clr(clr->main.accent, state->alpha[1]), SCALE(elements->game_card.img_rounding));

        draw->rect(window->DrawList, button.Min, button.Max, draw->get_clr(clr->main.text, 0.06), SCALE(elements->game_card.img_rounding), SCALE(1));
        draw->text_clipped(window->DrawList, font->get(icons_data, 13), button.Min, button.Max, draw->get_clr(clr->main.text, 0.24), "F", NULL, NULL, ImVec2(0.5f, 0.5f));
        draw->text_clipped(window->DrawList, font->get(suisse_intl_medium_data, 16), ImVec2(total.Min.x + SCALE(elements->game_card.padding.x), button.Min.y), total.Max, draw->get_clr(clr->main.text, 0.72), name.data(), gui->text_end(name.data()));
        draw->text_clipped(window->DrawList, font->get(icons_data, 8), ImVec2(total.Min.x + SCALE(elements->game_card.padding.x), button.Max.y - SCALE(elements->game_card.icon_size.y)), ImVec2(total.Min.x + SCALE(elements->game_card.padding.x + elements->game_card.icon_size.x), button.Max.y), draw->get_clr(clr->main.text, 0.24), "E", NULL, NULL, ImVec2(0.5f, 0.5f));
        draw->text_clipped(window->DrawList, font->get(suisse_intl_medium_data, 14), ImVec2(total.Min.x + SCALE(elements->game_card.padding.x + elements->game_card.icon_size.x + elements->game_card.text_spacing), button.Max.y - SCALE(elements->game_card.icon_size.y)), button.Max, draw->get_clr(clr->main.text, 0.48), count.data(), NULL, NULL, ImVec2(0.f, 0.5f));
    
        
        gui->set_next_window_pos(img_zone.Min);
        gui->begin_def_child(std::string(widgets_id) + "blur", img_zone.GetSize() + SCALE(2, 0), 0, window_flags_always_use_window_padding | window_flags_no_move | window_flags_nav_flattened | window_flags_no_saved_settings | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse | window_flags_no_background);
        {
            draw_background_blur(gui->window_drawlist(), var->winapi.device_dx11, var->winapi.device_context, SCALE(elements->game_card.img_rounding));
            draw->rect_filled(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->window.background, 0.72), SCALE(elements->game_card.img_rounding));
            draw->image_rounded(gui->window_drawlist(), var->gui.decoration[1], gui->window_pos(), gui->window_pos() + gui->window_size(), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr({ 1.f, 1.f, 1.f, 1.f }), SCALE(elements->game_card.img_rounding));
            draw->rect(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->main.text, 0.04), SCALE(elements->game_card.img_rounding));
            draw->text_clipped(gui->window_drawlist(), font->get(suisse_intl_medium_data, 16), gui->window_pos() + ImVec2(gui->text_size(font->get(icons_data, 12), "K").x + SCALE(6), 0) / 2, gui->window_pos() + gui->window_size() + ImVec2(gui->text_size(font->get(icons_data, 12), "K").x + SCALE(6), 0) / 2, draw->get_clr(clr->main.text, 0.24), "Purchase", NULL, NULL, ImVec2(0.5f, 0.5f));
            draw->text_clipped(gui->window_drawlist(), font->get(icons_data, 12), gui->window_pos() - ImVec2(gui->text_size(font->get(suisse_intl_medium_data, 16), "Purchase").x + SCALE(6), 0) / 2, gui->window_pos() + gui->window_size() - ImVec2(gui->text_size(font->get(suisse_intl_medium_data, 16), "Purchase").x + SCALE(6), 0) / 2, draw->get_clr(clr->main.text, 0.12), "K", NULL, NULL, ImVec2(0.5f, 0.5f));
        }
        gui->end_def_child();
    }
    gui->set_screen_pos(total.Min, pos_all);
    gui->dummy(total.GetSize());

    return state->clicked;
}
