#define IMGUI_DEFINE_MATH_OPERATORS
#include "custom_ui.h"
#include "loader_ui.h"
#include "../imgui_manager/imgui_manager.h"
#include "../dep/imgui/imgui_internal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace {
constexpr ImVec4 kAccentColor = ImVec4(0.78f, 0.52f, 0.98f, 1.0f);
constexpr ImVec4 kAccentSoft = ImVec4(0.62f, 0.40f, 0.88f, 0.65f);
constexpr ImVec4 kPanelBg = ImVec4(0.12f, 0.11f, 0.14f, 0.98f);
constexpr ImVec4 kPanelOutline = ImVec4(1.f, 1.f, 1.f, 0.06f);
constexpr ImVec4 kSurfaceDim = ImVec4(0.08f, 0.07f, 0.10f, 0.92f);
constexpr ImVec4 kSurfaceAlt = ImVec4(0.15f, 0.13f, 0.18f, 0.92f);
constexpr ImVec4 kTextPrimary = ImVec4(0.95f, 0.94f, 0.97f, 1.f);
constexpr ImVec4 kTextMuted = ImVec4(0.78f, 0.76f, 0.82f, 0.9f);
constexpr ImVec4 kSuccess = ImVec4(0.43f, 0.80f, 0.53f, 1.f);
constexpr ImVec4 kDanger = ImVec4(0.89f, 0.36f, 0.41f, 1.f);

ImVec2 centered_window_pos(const ImVec2& size) {
    auto* viewport = ImGui::GetMainViewport();
    const ImVec2 origin = viewport->GetCenter();
    return ImVec2(origin.x - size.x * 0.5f, origin.y - size.y * 0.5f);
}

bool input_text_labeled(const char* label, const char* hint, char* buffer, size_t buffer_size, ImGuiInputTextFlags flags = 0, bool password = false) {
    ImGui::PushID(label);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, kSurfaceDim);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kSurfaceAlt);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kAccentSoft);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.f, 10.f));
    bool edited = false;
    if (password) {
        edited = ImGui::InputTextWithHint("##input", hint, buffer, static_cast<int>(buffer_size), flags | ImGuiInputTextFlags_Password);
    } else {
        edited = ImGui::InputTextWithHint("##input", hint, buffer, static_cast<int>(buffer_size), flags);
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return edited;
}

} // namespace

CustomLoaderUi::CustomLoaderUi()
    : manager_(nullptr),
      active_panel_(ActivePanel::Login),
      style_initialized_(false),
      authenticated_(false),
      background_mode_(LOADER_UI_BACKGROUND_GRADIENT),
      background_color_(0.05f, 0.04f, 0.08f, 0.96f) {
    inputs_.clear();
}

void CustomLoaderUi::initialize(c_imgui_manager& manager) {
    manager_ = &manager;
    apply_theme_once(manager);
}

void CustomLoaderUi::apply_theme_once(c_imgui_manager&) {
    if (style_initialized_) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 0.f;
    style.WindowBorderSize = 0.f;
    style.WindowRounding = 18.f;
    style.FrameRounding = 12.f;
    style.PopupRounding = 10.f;
    style.ChildRounding = 16.f;
    style.ScrollbarSize = 10.f;
    style.GrabRounding = 10.f;
    style.GrabMinSize = 12.f;
    style.WindowPadding = ImVec2(24.f, 24.f);
    style.ItemSpacing = ImVec2(12.f, 12.f);
    style.ItemInnerSpacing = ImVec2(8.f, 6.f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.04f, 0.08f, 0.96f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.09f, 0.13f, 0.95f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.07f, 0.12f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.40f, 0.39f, 0.47f, 0.15f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.12f, 0.18f, 0.95f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.17f, 0.25f, 0.98f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.21f, 0.29f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.07f, 0.10f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.11f, 0.16f, 1.0f);
    colors[ImGuiCol_Button] = kAccentSoft;
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.82f, 0.58f, 1.0f, 0.90f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.74f, 0.52f, 0.96f, 1.0f);
    colors[ImGuiCol_Header] = kAccentSoft;
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.82f, 0.58f, 1.0f, 0.85f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.78f, 0.53f, 0.99f, 0.90f);
    colors[ImGuiCol_CheckMark] = kAccentColor;
    colors[ImGuiCol_SliderGrab] = kAccentColor;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.65f, 1.0f, 0.95f);
    colors[ImGuiCol_Text] = kTextPrimary;
    colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.54f, 0.63f, 0.6f);
    colors[ImGuiCol_ResizeGrip] = kAccentSoft;
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.85f, 0.63f, 1.0f, 0.95f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.90f, 0.70f, 1.0f, 1.0f);

    style_initialized_ = true;
}

void CustomLoaderUi::set_callbacks(const Callbacks& callbacks) {
    callbacks_ = callbacks;
}

void CustomLoaderUi::set_status(const std::string& status) {
    status_message_ = status;
    error_message_.clear();
}

void CustomLoaderUi::set_error(const std::string& error) {
    error_message_ = error;
    status_message_.clear();
}

void CustomLoaderUi::clear_feedback() {
    status_message_.clear();
    error_message_.clear();
}

void CustomLoaderUi::sync_view_flags(const ui_state& ui) {
    if (ui.show_main_window) {
        active_panel_ = ActivePanel::Dashboard;
    } else if (ui.show_register_window) {
        active_panel_ = ActivePanel::Register;
    } else {
        active_panel_ = ActivePanel::Login;
    }
    authenticated_ = ui.authenticated;
}

void CustomLoaderUi::set_authenticated_profile(const user_profile* profile) {
    authenticated_ = profile != nullptr;
    if (authenticated_) {
        active_panel_ = ActivePanel::Dashboard;
    }
}

void CustomLoaderUi::render(ui_state& ui, const user_profile& profile) {
    if (!manager_) {
        return;
    }

    apply_theme_once(*manager_);
    sync_view_flags(ui);

    ImGuiViewport* viewport = ImGui::GetMainViewport();

    if (background_mode_ == LOADER_UI_BACKGROUND_GRADIENT) {
        render_background();
    } else if (background_mode_ == LOADER_UI_BACKGROUND_SOLID) {
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList(viewport);
        const ImVec2 top_left = viewport->Pos;
        const ImVec2 bottom_right = top_left + viewport->Size;
        draw_list->AddRectFilled(top_left, bottom_right, ImGui::ColorConvertFloat4ToU32(background_color_));
    }

    switch (active_panel_) {
    case ActivePanel::Login:
        render_login_panel(ui);
        break;
    case ActivePanel::Register:
        render_register_panel(ui);
        break;
    case ActivePanel::Dashboard:
        render_dashboard(profile);
        break;
    }
}

void CustomLoaderUi::render_background() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##custom-loader-background", nullptr, flags)) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 top_left = viewport->Pos;
        const ImVec2 bottom_right = top_left + viewport->Size;

        draw_gradient_rect(draw_list, top_left, bottom_right,
            ImGui::GetColorU32(ImVec4(0.08f, 0.06f, 0.12f, 1.f)),
            ImGui::GetColorU32(ImVec4(0.05f, 0.04f, 0.10f, 1.f)),
            ImGui::GetColorU32(ImVec4(0.04f, 0.04f, 0.09f, 1.f)),
            ImGui::GetColorU32(ImVec4(0.06f, 0.05f, 0.12f, 1.f)));

        const ImVec2 center = viewport->GetCenter();
        const float radius = min(viewport->Size.x, viewport->Size.y) * 0.36f;
        draw_list->AddCircleFilled(center, radius, ImGui::GetColorU32(ImVec4(0.20f, 0.16f, 0.33f, 0.28f)), 64);
        draw_list->AddCircleFilled(center, radius * 0.62f, ImGui::GetColorU32(ImVec4(0.32f, 0.22f, 0.50f, 0.24f)), 64);

        ImVec2 ripple_center = ImVec2(center.x * 0.45f, center.y * 0.95f);
        draw_list->AddCircle(ripple_center, radius * 0.55f, ImGui::GetColorU32(ImVec4(kAccentColor.x, kAccentColor.y, kAccentColor.z, 0.35f)), 48, 1.3f);
        draw_list->AddCircle(ripple_center, radius * 0.75f, ImGui::GetColorU32(ImVec4(kAccentColor.x, kAccentColor.y, kAccentColor.z, 0.2f)), 48, 1.0f);
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
}

void CustomLoaderUi::render_header_bar(const user_profile& profile) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.f, 8.f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.11f, 0.18f, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.f);
    ImGui::BeginChild("##header-bar", ImVec2(0, 86.f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
    ImGui::TextWrapped("Welcome back, %s", profile.username.empty() ? "Guest" : profile.username.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(0.f, 120.f);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
    ImGui::Text("Environment");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.13f, 0.18f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.18f, 0.28f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.19f, 0.30f, 0.96f));
    if (ImGui::Button("Production")) {
        set_status("Environment switched to production");
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine(ImGui::GetWindowWidth() - 240.f);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
    ImGui::Text("IP: %s", profile.ip.empty() ? "0.0.0.0" : profile.ip.c_str());
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void CustomLoaderUi::render_subscription_cards(const user_profile& profile) {
    ImGui::PushID("subscription_cards");
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f, 16.f));

    ImGui::Columns(3, nullptr, false);
    size_t count = profile.subscriptions.size();
    if (count == 0) {
        draw_section_title("Subscriptions");
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
        ImGui::TextWrapped("No active subscriptions found. Once you authenticate, products and entitlements will appear here.");
        ImGui::PopStyleColor();
    } else {
        draw_section_title("Subscriptions");
        ImGui::Spacing();
        for (size_t index = 0; index < count; ++index) {
            const user_subscription* sub = profile.subscriptions[index];
            if (!sub) {
                continue;
            }

            ImGui::BeginChild(static_cast<ImGuiID>(index), ImVec2(0, 140.f), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
            ImGui::Text("%s", sub->plan.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
            ImGui::Text("Status: %s", sub->status.c_str());
            ImGui::Text("Expires: %s", sub->expires_at.c_str());
            ImGui::PopStyleColor();

            if (ImGui::Button("Open product portal", ImVec2(-1, 0))) {
                if (callbacks_.on_filestream) {
                    callbacks_.on_filestream(sub->plan);
                }
            }
            ImGui::EndChild();
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }
    ImGui::Columns(1);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

void CustomLoaderUi::render_game_library() {
    struct GameCard {
        const char* name;
        const char* category;
        const char* status;
        float uptime;
        bool ready;
    };

    constexpr std::array<GameCard, 6> games{ {
        {"Rust", "FPS", "Undetected", 99.1f, true},
        {"Valorant", "FPS", "Updating", 65.4f, false},
        {"Apex Legends", "FPS", "Undetected", 92.3f, true},
        {"Fortnite", "Battle Royale", "Undetected", 88.7f, true},
        {"PUBG", "Battle Royale", "Updating", 54.0f, false},
        {"COD Warzone", "Battle Royale", "Testing", 32.1f, false}
    } };

    draw_section_title("Game Library");
    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(18.f, 18.f));
    const float column_width = ImGui::GetContentRegionAvail().x / 3.f;

    for (size_t index = 0; index < games.size(); ++index) {
        const GameCard& card = games[index];
        ImGui::PushID(static_cast<int>(index));
        ImGui::BeginChild("##card", ImVec2(column_width - 12.f, 160.f), true);
        ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
        ImGui::Text("%s", card.name);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
        ImGui::Text("%s", card.category);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Text("Uptime %.1f%%", card.uptime);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, card.ready ? kSuccess : kDanger);
        ImGui::Text("%s", card.status);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));
        if (ImGui::Button(card.ready ? "Launch" : "Notify me", ImVec2(-1, 0))) {
            if (card.ready) {
                set_status(std::string("Launching ") + card.name + "...");
            } else {
                set_status(std::string("We'll notify you when ") + card.name + " is ready");
            }
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopID();

        if ((index + 1) % 3 != 0) {
            ImGui::SameLine(0.f, 18.f);
        }
    }
    ImGui::PopStyleVar();
}

void CustomLoaderUi::render_activity_feed() {
    draw_section_title("Activity Feed");
    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.f);
    ImGui::BeginChild("##activity-feed", ImVec2(0, 210.f), true);

    struct FeedEntry {
        const char* title;
        const char* description;
        const char* timestamp;
    };

    constexpr std::array<FeedEntry, 4> feed{ {
        {"Rust update deployed", "New bypass for kernel-level anti-cheat shipped successfully.", "6 minutes ago"},
        {"Valorant maintenance", "Auth servers currently applying patch 1.11. ETA 20 minutes.", "24 minutes ago"},
        {"New feature", "Added account-based skin changer for Apex Legends release channel.", "1 hour ago"},
        {"Security notice", "HWID rotation scheduled for tomorrow 18:00 UTC.", "Yesterday"}
    } };

    for (const auto& entry : feed) {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
        ImGui::Text("%s", entry.title);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
        ImGui::TextWrapped("%s", entry.description);
        ImGui::Text("%s", entry.timestamp);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void CustomLoaderUi::render_footer_actions() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushID("footer-actions");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(18.f, 10.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);

    if (ImGui::Button("Open Licensing", ImVec2(200.f, 0.f))) {
        if (callbacks_.on_license && inputs_.register_username[0] != '\0') {
            callbacks_.on_license(inputs_.register_username, inputs_.register_license);
        } else {
            set_error("Provide username and license to continue.");
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Request Filestream", ImVec2(220.f, 0.f))) {
        if (callbacks_.on_filestream) {
            callbacks_.on_filestream("default");
        }
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 180.f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.12f, 0.22f, 0.96f));
    if (ImGui::Button("Sign out", ImVec2(160.f, 0.f))) {
        authenticated_ = false;
        active_panel_ = ActivePanel::Login;
        clear_feedback();
        if (callbacks_.on_exit) {
            callbacks_.on_exit();
        }
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

void CustomLoaderUi::render_dashboard(const user_profile& profile) {
    const ImVec2 size(760.f, 520.f);
    const ImVec2 pos = centered_window_pos(size);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, kPanelBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26.f, 26.f));

    if (ImGui::Begin("##custom-dashboard", nullptr, flags)) {
        render_header_bar(profile);

        ImGui::BeginChild("##dashboard-scroll", ImVec2(0, -1), false, ImGuiWindowFlags_NoScrollbar);
        render_subscription_cards(profile);
        ImGui::Spacing();
        render_game_library();
        ImGui::Spacing();
        render_activity_feed();
        ImGui::Spacing();
        render_footer_actions();
        ImGui::EndChild();

        if (!status_message_.empty() || !error_message_.empty()) {
            ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 40.f);
            render_feedback_text();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void CustomLoaderUi::render_login_panel(ui_state& ui) {
    const ImVec2 size(384.f, 520.f);
    const ImVec2 pos = centered_window_pos(size);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, kPanelBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.f, 24.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.f, 8.f));

    if (ImGui::Begin("##custom-login-panel", nullptr, flags)) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 panel_min = ImGui::GetWindowPos();
        const ImVec2 panel_max = panel_min + ImGui::GetWindowSize();
        draw_list->AddRect(panel_min, panel_max, ImGui::GetColorU32(kPanelOutline), 24.f, 0, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, kAccentColor);
        ImGui::Text("Bootstrapper");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.f, 4.f));

        ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
        ImGui::Text("Welcome back");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
        ImGui::TextWrapped("Authenticate with your credentials to continue to the control panel.");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.f, 6.f));

        input_text_labeled("Username", "Enter username", inputs_.login_username, IM_ARRAYSIZE(inputs_.login_username));
        ImGui::Dummy(ImVec2(0.f, 6.f));
        input_text_labeled("Password", "Enter password", inputs_.login_password, IM_ARRAYSIZE(inputs_.login_password), 0, true);

        ImGui::Dummy(ImVec2(0.f, 10.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.f, 10.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
        if (ImGui::Button("Sign in", ImVec2(-1, 0))) {
            if (callbacks_.on_login) {
                callbacks_.on_login(inputs_.login_username, inputs_.login_password);
            }
        }
        ImGui::PopStyleVar(2);

        ImGui::Dummy(ImVec2(0.f, 8.f));
        render_feedback_text();

        ImGui::Dummy(ImVec2(0.f, 10.f));
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
        ImGui::Text("Need an account?");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, kAccentColor);
        if (ImGui::Selectable("Create one", false, 0, ImVec2(0, 20.f))) {
            active_panel_ = ActivePanel::Register;
            ui.show_login_window = false;
            ui.show_register_window = true;
            ui.authenticated = false;
        }
        ImGui::PopStyleColor();
    }

    ImGui::End();
    ImGui::PopStyleVar(); // ItemSpacing
    ImGui::PopStyleVar(); // WindowPadding
    ImGui::PopStyleColor();
}

void CustomLoaderUi::render_register_panel(ui_state& ui) {
    const ImVec2 size(440.f, 600.f);
    const ImVec2 pos = centered_window_pos(size);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, kPanelBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.f, 24.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12.f, 8.f));

    if (ImGui::Begin("##custom-register-panel", nullptr, flags)) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 panel_min = ImGui::GetWindowPos();
        const ImVec2 panel_max = panel_min + ImGui::GetWindowSize();
        draw_list->AddRect(panel_min, panel_max, ImGui::GetColorU32(kPanelOutline), 24.f, 0, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, kAccentColor);
        ImGui::Text("Bootstrapper");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
        ImGui::Text("Create account");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
        ImGui::TextWrapped("Provision a new account for the loader. Complete all fields and optionally provide your license key.");
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.f, 6.f));
        input_text_labeled("Email", "Enter email", inputs_.register_email, IM_ARRAYSIZE(inputs_.register_email));
        ImGui::Dummy(ImVec2(0.f, 6.f));
        input_text_labeled("Username", "Enter username", inputs_.register_username, IM_ARRAYSIZE(inputs_.register_username));
        ImGui::Dummy(ImVec2(0.f, 6.f));
        input_text_labeled("Password", "Set password", inputs_.register_password, IM_ARRAYSIZE(inputs_.register_password), 0, true);
        ImGui::Dummy(ImVec2(0.f, 6.f));
        input_text_labeled("License key", "Optional license key", inputs_.register_license, IM_ARRAYSIZE(inputs_.register_license));

        ImGui::Dummy(ImVec2(0.f, 10.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.f, 10.f));
        if (ImGui::Button("Register", ImVec2(-1, 0))) {
            if (callbacks_.on_register) {
                callbacks_.on_register(inputs_.register_username, inputs_.register_password, inputs_.register_license);
            }
            if (inputs_.register_license[0] != '\0' && callbacks_.on_license) {
                callbacks_.on_license(inputs_.register_username, inputs_.register_license);
            }
        }
        ImGui::PopStyleVar();

        ImGui::Dummy(ImVec2(0.f, 8.f));
        render_feedback_text();

        ImGui::Dummy(ImVec2(0.f, 10.f));
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
        ImGui::Text("Already have credentials?");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, kAccentColor);
        if (ImGui::Selectable("Return to login", false, 0, ImVec2(0, 20.f))) {
            active_panel_ = ActivePanel::Login;
            ui.show_register_window = false;
            ui.show_login_window = true;
            ui.authenticated = false;
        }
        ImGui::PopStyleColor();
    }

    ImGui::End();
    ImGui::PopStyleVar(); // ItemSpacing
    ImGui::PopStyleVar(); // WindowPadding
    ImGui::PopStyleColor();
}

void CustomLoaderUi::render_feedback_text() {
    if (status_message_.empty() && error_message_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMuted);
        ImGui::TextWrapped("Tip: you can drag this window outside the viewport thanks to multi-viewport support.");
        ImGui::PopStyleColor();
        return;
    }

    if (!status_message_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kSuccess);
        ImGui::TextWrapped("%s", status_message_.c_str());
        ImGui::PopStyleColor();
    }

    if (!error_message_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
        ImGui::TextWrapped("%s", error_message_.c_str());
        ImGui::PopStyleColor();
    }
}

void CustomLoaderUi::set_background(loader_ui_background_mode mode, const float* color) {
    background_mode_ = mode;
    if (color) {
        background_color_ = ImVec4(color[0], color[1], color[2], color[3]);
    }
}

void CustomLoaderUi::draw_section_title(const char* title, bool centered) {
    ImGui::PushStyleColor(ImGuiCol_Text, kTextPrimary);
    if (centered) {
        float width = ImGui::CalcTextSize(title).x;
        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    }
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
}

void CustomLoaderUi::draw_gradient_rect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, ImU32 col_tl, ImU32 col_tr, ImU32 col_br, ImU32 col_bl) {
    draw_list->AddRectFilledMultiColor(min, max, col_tl, col_tr, col_br, col_bl);
}
