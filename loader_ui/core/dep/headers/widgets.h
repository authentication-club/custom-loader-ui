#pragma once
#include "includes.h"

#define IMGUI_DEFINE_MATH_OPERATORS

class c_widgets
{
public:

    void log_reg_page();

    void info_card(std::string_view widgets_id, std::string_view icon, std::string_view name, std::string_view desc, float width);

    bool text_field(std::string_view widgets_id, std::string_view name, std::string_view icon, char* buf, int size);

    void version_card(std::string_view widgets_id, std::string_view name, std::string_view update, int img_id);

    bool reg_log_buttons();

    bool settings_button();

    void top_bar(std::string_view url, std::string_view date);

    void games_child(std::string_view widgets_id, std::string_view name, std::string_view icon, std::string_view count);
        
    void games_end_child();

    bool game_card(std::string_view widgets_id, std::string_view name, std::string_view count, int img_id, bool ready = true);

    bool product_page(
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
    );

    bool back_button();

    bool launch_button();

    bool update_button();

    bool checkbox_ex(std::string_view widgets_id, std::string_view name, bool* callback);

    void widgets_child(std::string_view widgets_id, std::string_view name, std::string_view icon);

    void widgets_end_child();

    void selection_buttons(std::string_view widgets_id, std::string_view name, std::string_view p1, std::string_view p2, int& variable);

};

inline std::unique_ptr<c_widgets> widgets = std::make_unique<c_widgets>();

enum notify_type
{
    success = 0,
    warning = 1,
    error = 2
};

struct notify_state
{
    int notify_id;
    std::string_view text;
    notify_type type{ success };

    ImVec2 window_size{ 0, 0 };
    float notify_alpha{ 0 };
    bool active_notify{ true };
    float notify_timer{ 0 };
    float notify_pos{ 0 };
};

class c_notify
{
public:
    void setup_notify();

    void add_notify(std::string_view text, notify_type type);

private:
    ImVec2 render_notify(int cur_notify_value, float notify_alpha, float notify_percentage, float notify_pos, std::string_view text, notify_type type);

    float notify_time{ 15 };
    int notify_count{ 0 };

    float notify_spacing{ 20 };
    ImVec2 notify_padding{ 20, 20 };

    std::vector<notify_state> notifications;

};

inline std::unique_ptr<c_notify> notify = std::make_unique<c_notify>();
