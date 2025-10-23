#include "../headers/includes.h"
#include "../headers/widgets.h"
#include "../../loader_ui/loader_ui.h"


bool c_widgets::product_page(
    c_video_player* player,
    ID3D11ShaderResourceView* image,
    std::string_view name,
    std::string_view desc,
    std::string_view plan_id,
    std::string_view expires,
    std::string_view status,
    std::string_view default_file,
    bool can_launch,
    std::string_view helper_text
)
{
    (void)default_file;
    bool launch_clicked = false;

    gui->begin_content(
        "product_desc_content",
        ImVec2(
            gui->content_max().x - SCALE(elements->player.size.x + elements->window.padding.x),
            gui->content_avail().y
        ),
        SCALE(0, 0),
        SCALE(elements->widgets.spacing),
        window_flags_no_scrollbar | window_flags_no_scroll_with_mouse
    );
    {
        ImGuiContext& g = *GImGui;

        widgets->back_button();

        const ImRect game_rect(
            ImVec2(g.LastItemData.Rect.Min.x, g.LastItemData.Rect.Max.y + SCALE(elements->widgets.spacing.y)),
            ImVec2(
                g.LastItemData.Rect.Min.x + gui->content_avail().x,
                g.LastItemData.Rect.Max.y + SCALE(elements->widgets.spacing.y + elements->product_page.game_zone_height)
            )
        );

        const ImRect desc_rect(
            ImVec2(game_rect.Min.x, game_rect.Max.y + SCALE(elements->product_page.back_button_padding)),
            gui->window_pos() + gui->window_size() - SCALE(0, elements->widgets.spacing.y * 2 + elements->info_card.height * 2)
        );

        if (image)
        {
            draw->image_rounded(
                gui->window_drawlist(),
                image,
                ImVec2(game_rect.Min.x, game_rect.GetCenter().y - SCALE(elements->product_page.img_size.y / 2)),
                ImVec2(game_rect.Min.x + SCALE(elements->product_page.img_size.x), game_rect.GetCenter().y + SCALE(elements->product_page.img_size.y / 2)),
                ImVec2(0, 0),
                ImVec2(1, 1),
                draw->get_clr({ 1.f, 1.f, 1.f, 1.f }),
                SCALE(elements->version_card.rounding)
            );
        }
        else
        {
            draw->rect_filled(
                gui->window_drawlist(),
                ImVec2(game_rect.Min.x, game_rect.GetCenter().y - SCALE(elements->product_page.img_size.y / 2)),
                ImVec2(game_rect.Min.x + SCALE(elements->product_page.img_size.x), game_rect.GetCenter().y + SCALE(elements->product_page.img_size.y / 2)),
                draw->get_clr(clr->main.text, 0.08f),
                SCALE(elements->version_card.rounding)
            );
        }

        draw->text_clipped(
            gui->window_drawlist(),
            font->get(suisse_intl_medium_data, 19),
            game_rect.Min + SCALE(elements->product_page.img_size.x + elements->product_page.back_button_padding, 0),
            game_rect.Max,
            draw->get_clr(clr->main.text),
            name.data(),
            nullptr,
            nullptr,
            ImVec2(0.f, 0.5f)
        );

        std::string desc_str(desc);
        std::stringstream desc_stream(desc_str);
        std::string paragraph;
        const float line_height = gui->text_size(font->get(suisse_intl_medium_data, 13), "A").y;
        ImVec2 desc_pos = desc_rect.Min;

        while (std::getline(desc_stream, paragraph))
        {
            const auto wrapped = gui->wrap_text(
                font->get(suisse_intl_medium_data, 13),
                desc_rect.Max.x - desc_rect.Min.x,
                paragraph
            );
            for (const auto& line : wrapped)
            {
                draw->text_clipped(
                    gui->window_drawlist(),
                    font->get(suisse_intl_medium_data, 13),
                    desc_pos,
                    desc_rect.Max,
                    draw->get_clr(clr->main.text, 0.48f),
                    line.c_str()
                );
                desc_pos.y += line_height;
            }
            desc_pos.y += SCALE(elements->widgets.spacing.y * 0.5f);
        }

        gui->set_screen_pos(ImVec2(desc_rect.Min.x, desc_rect.Max.y + SCALE(elements->widgets.spacing.y)), pos_all);

        float card_width = (gui->window_size().x - SCALE(elements->widgets.spacing.x)) / 2.0f;

        gui->begin_group();
        {
            const std::string status_card_id = std::string("status_card_") + std::string(plan_id);
            const std::string expires_card_id = std::string("expires_card_") + std::string(plan_id);
            widgets->info_card(status_card_id, "b", gui->language("Status", "Status"), status.data(), card_width);
            gui->sameline();
            widgets->info_card(expires_card_id, "W", gui->language("Expires", "Expires"), expires.data(), card_width);
        }
        gui->end_group();
    }
    gui->end_content();

    gui->sameline();
    gui->begin_content(
        "player_content",
        ImVec2(SCALE(elements->player.size.x), gui->content_avail().y),
        SCALE(0, 0),
        SCALE(elements->widgets.spacing),
        window_flags_no_scrollbar | window_flags_no_scroll_with_mouse
    );
    {
        if (player)
        {
            std::string video_id = "product_video";
            video_id += "_" + std::to_string(reinterpret_cast<uintptr_t>(player));
            player->render(video_id, SCALE(var->window.size), name);
        }
        else
        {
            draw->text_clipped(
                gui->window_drawlist(),
                font->get(suisse_intl_medium_data, 13),
                gui->window_pos() + SCALE(elements->widgets.spacing),
                gui->window_pos() + gui->window_size(),
                draw->get_clr(clr->main.text, 0.32f),
                gui->language("No preview video uploaded", "No preview video uploaded").c_str(),
                nullptr,
                nullptr,
                ImVec2(0.f, 0.f)
            );
            gui->dummy(SCALE(0, elements->widgets.spacing.y * 1.5f));
        }

        gui->set_screen_pos(GImGui->LastItemData.Rect.Max.y + SCALE(elements->window.padding.y), pos_y);

        if (widgets->launch_button() && can_launch)
            launch_clicked = true;

        gui->dummy(SCALE(0, elements->widgets.spacing.y));
        if (!helper_text.empty())
        {
            draw->text_clipped(
                gui->window_drawlist(),
                font->get(suisse_intl_medium_data, 13),
                gui->window_pos(),
                gui->window_pos() + gui->window_size(),
                draw->get_clr(clr->main.text, 0.48f),
                helper_text.data(),
                nullptr,
                nullptr,
                ImVec2(0.f, 0.f)
            );
        }
    }
    gui->end_content();

    return launch_clicked;
}
