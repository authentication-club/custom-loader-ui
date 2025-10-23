#include "../headers/includes.h"
#include "../imgui/imgui_impl_dx11.h"

void c_font::update()
{
    if (var->gui.dpi_changed)
    {
        var->gui.dpi = var->gui.stored_dpi / 100.f;

        ImFontConfig cfg = {};
        cfg.FontDataOwnedByAtlas = false;

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        for (auto& font_t : data)
        {
            font_t.font = io.Fonts->AddFontFromMemoryTTF(
                font_t.data.data(),
                static_cast<int>(font_t.data.size()),
                SCALE(font_t.size),
                &cfg,
                io.Fonts->GetGlyphRangesCyrillic()
            );
        }

        io.Fonts->Build();
        ImGui_ImplDX11_CreateDeviceObjects();

        var->gui.dpi_changed = false;
    }
}

ImFont* c_font::get(const std::vector<unsigned char>& font_bytes, float size)
{
    ImGuiIO& io = ImGui::GetIO();

    ImFont* fallback_font = nullptr;
    if (ImGui::GetCurrentContext())
        fallback_font = ImGui::GetFont();
    if (!fallback_font)
    {
        fallback_font = io.FontDefault;
        if (!fallback_font && !io.Fonts->Fonts.empty())
            fallback_font = io.Fonts->Fonts.front();
    }

    auto ensure_font_ready = [&](c_font::font_data& entry) -> ImFont*
        {
            if (!entry.font && !io.Fonts->Locked)
            {
                var->gui.dpi_changed = true;
                update();
            }
            return entry.font ? entry.font : fallback_font;
        };

    for (auto& font : data)
    {
        if (font.data == font_bytes && font.size == size)
            return ensure_font_ready(font);
    }

    add(font_bytes, size);
    var->gui.dpi_changed = true;

    auto& new_entry = data.back();
    if (!io.Fonts->Locked)
        update();

    return ensure_font_ready(new_entry);
}

void c_font::add(const std::vector<unsigned char>& font_data, float size)
{
    data.push_back({ font_data, size, nullptr });
}
