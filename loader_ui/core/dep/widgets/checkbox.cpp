#include"../headers/includes.h"
#include "../headers/widgets.h"

bool c_widgets::checkbox_ex(std::string_view widgets_id, std::string_view name, bool* callback)
{
    struct checkbox_state
    {
        float alpha[3]{ 0.f, 0.24f, 0.06f};
        float offset{ 0.f };
    };

    ImGuiWindow* window = gui->get_window();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(widgets_id.data());

    const float width = GetContentRegionAvail().x;
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total(pos, pos + ImVec2(width, SCALE(elements->checkbox.height)));
    const ImRect rect(ImVec2(total.Max.x - SCALE(elements->checkbox.rect_size.x), total.GetCenter().y - SCALE(elements->checkbox.rect_size.x) / 2), ImVec2(total.Max.x, total.GetCenter().y + SCALE(elements->checkbox.rect_size.x) / 2));

    gui->item_size(total, style.FramePadding.y);
    if (!gui->item_add(total, id))
        return false;

    bool hovered, held;
    bool pressed = gui->button_behavior(total, id, &hovered, &held);
    if (pressed)
        *callback = !(*callback);

    checkbox_state* state = gui->anim_container<checkbox_state>(id);

    gui->easing(state->alpha[0], *callback ? 1.f : 0.f, 8.f, static_easing);
    gui->easing(state->alpha[1], *callback ? 1.f : 0.24f, 8.f, static_easing);
    gui->easing(state->alpha[2], *callback ? 0.48f : 0.06f, 8.f, static_easing);
    gui->easing(state->offset, *callback ? rect.GetHeight() / 2 : 0.8f, 40.f, static_easing);

    draw->rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->main.text, 0.04), SCALE(elements->checkbox.rounding));
    draw->rect_filled(window->DrawList, rect.GetCenter() - ImVec2(state->offset, state->offset), rect.GetCenter() + ImVec2(state->offset, state->offset), draw->get_clr(clr->main.accent, state->alpha[0]), SCALE(elements->checkbox.rounding));
    draw->rect(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->main.text, state->alpha[2]), SCALE(elements->checkbox.rounding));
    
    draw->text_clipped(window->DrawList, font->get(suisse_intl_medium_data, 16), total.Min, total.Max, draw->get_clr(clr->main.text, state->alpha[1]), name.data(), gui->text_end(name.data()), NULL, ImVec2(0.f, 0.5f));
    draw->text_clipped(window->DrawList, font->get(icons_data, 7), rect.Min, rect.Max, draw->get_clr(clr->main.text, state->alpha[0]), "L", NULL, NULL, ImVec2(0.5f, 0.5f));

    return *callback;
}
