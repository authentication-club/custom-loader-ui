#include "headers/includes.h"
#include "headers/widgets.h"
#include "../loader_ui/loader_ui.h"

namespace {
	std::string format_expiration_remaining(const std::string& timestamp)
	{
		if (timestamp.empty())
			return "-";

		std::tm tm{};
		std::istringstream iss(timestamp);
		iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
		if (iss.fail())
			return timestamp;

		long microseconds = 0;
		if (iss.peek() == '.')
		{
			iss.get();
			std::string fractional;
			std::getline(iss, fractional);
			if (!fractional.empty())
			{
				while (fractional.size() < 6)
					fractional.push_back('0');
				try
				{
					microseconds = std::stol(fractional.substr(0, 6));
				}
				catch (...)
				{
					microseconds = 0;
				}
			}
		}

		time_t utc_time = _mkgmtime(&tm);
		if (utc_time == static_cast<time_t>(-1))
			return timestamp;

		auto expiry_tp = std::chrono::system_clock::from_time_t(utc_time) + std::chrono::microseconds(microseconds);
		auto now = std::chrono::system_clock::now();
		if (expiry_tp <= now)
			return "Expired";

		auto remaining = std::chrono::duration_cast<std::chrono::seconds>(expiry_tp - now);
		long long total_seconds = remaining.count();
		long long days = total_seconds / 86400;
		total_seconds %= 86400;
		long long hours = total_seconds / 3600;
		total_seconds %= 3600;
		long long minutes = total_seconds / 60;
		long long seconds = total_seconds % 60;

		std::ostringstream oss;
		oss << days << "d "
			<< std::setw(2) << std::setfill('0') << hours << "h "
			<< std::setw(2) << std::setfill('0') << minutes << "m "
			<< std::setw(2) << std::setfill('0') << seconds << "s";
		return oss.str();
	}
}

c_loader_ui* loader_ui = nullptr;
void c_gui::draw_login_page()
{
	gui->initialize();

	gui->push_var(style_var_window_shadow_size, SCALE(0, 0));
	gui->set_next_window_pos(ImVec2(0, 0));
	gui->set_next_window_size(SCALE(var->window.size));
	var->window.size = SCALE(430, 375);
	gui->begin("menu", nullptr, var->window.flags);
	{
		gui->set_style();
		gui->draw_decorations();
		gui->set_pos(SCALE(0, 0), pos_all);
		gui->begin_content("content", gui->window_size(), SCALE(elements->window.padding), SCALE(elements->widgets.spacing.x, elements->window.padding.y), window_flags_no_scroll_with_mouse | window_flags_no_scrollbar);
		{
			gui->begin_content(
				"log_reg_zone",
				SCALE(elements->log_reg_page.log_reg_width, 0),
				SCALE(elements->log_reg_page.padding),
				SCALE(0, elements->window.padding.y),
				window_flags_no_scroll_with_mouse | window_flags_no_scrollbar
			);
			{
				draw->shadow_circle(
					gui->window_drawlist(),
					gui->window_pos() + ImVec2(gui->window_size().x / 2,
						SCALE(elements->log_reg_page.shadow_radius + 4)),
					SCALE(elements->log_reg_page.shadow_radius),
					draw->get_clr(clr->main.accent, 0.1f),
					SCALE(100),
					ImVec2(0, 0),
					0,
					60
				);

				draw->text_clipped(
					gui->window_drawlist(),
					font->get(suisse_intl_medium_data, 16),
					gui->window_pos() + SCALE(0, elements->log_reg_page.shadow_radius * 2),
					gui->window_pos() + gui->window_size(),
					draw->get_clr(clr->main.text),
					"emperium",
					nullptr,
					nullptr,
					ImVec2(0.5f, 0.f)
				);

				gui->easing(
					elements->log_reg_page.window_height,
					var->gui.registration
					? (elements->textfield.size.y * 4 + elements->window.padding.y)
					: (elements->textfield.size.y * 3 + elements->window.padding.y),
					12.f,
					dynamic_easing
				);

				gui->set_pos(
					ImVec2(
						(gui->window_width() - SCALE(elements->textfield.size.x)) / 2,
						(gui->window_height() - SCALE(elements->log_reg_page.window_height)) / 2
					),
					pos_all
				);

				gui->begin_content(
					"registration_zone",
					SCALE(elements->textfield.size.x, 0),
					SCALE(0, 0),
					SCALE(0, elements->window.padding.y),
					window_flags_no_scroll_with_mouse | window_flags_no_scrollbar
				);
				{
					gui->begin_content(
						"fields_zone",
						SCALE(elements->textfield.size.x, elements->log_reg_page.window_height - elements->textfield.size.y),
						SCALE(0, 0),
						SCALE(0, elements->widgets.spacing.y),
						window_flags_no_scroll_with_mouse | window_flags_no_scrollbar
					);
					{

						static char license[100]{};
						static char username[100]{};
						static char password[100]{};

						if (var->gui.registration)
							widgets->text_field("license_field", "License", "d", license, sizeof(license));

						widgets->text_field("username_field", "Username", "B", username, sizeof(username));
						widgets->text_field("password_field", "Password", "C", password, sizeof(password));

						if (var->gui.registered && loader_ui)
						{
							if (var->gui.registration)
							{
								if (strlen(username) > 0 && strlen(password) > 0 && strlen(license) > 0)
								{
									loader_ui->handle_register_request(
										std::string(username),
										std::string(password),
										std::string(license));
									var->gui.registered = false;
								}
							}
							else
							{
								if (strlen(username) > 0 && strlen(password) > 0)
								{
									loader_ui->handle_login_request(std::string(username), std::string(password));
									var->gui.registered = false;
								}
							}
						}
					}
					gui->end_content();

					widgets->reg_log_buttons();
				}
				gui->end_content();
			}
			gui->end_content();
		}
		gui->end_content();
		gui->pop_var();

		bool bg{ false };
		if (gui->is_window_cond(GetCurrentContext()->NavWindow, { "dropdown" }))
			bg = true;
		gui->easing(elements->loading.window_alpha, bg || var->gui.loading ? 1.f : 0.f, 8.f, static_easing);

		gui->set_pos(SCALE(0, 0), pos_all);
		gui->push_var(style_var_alpha, elements->loading.window_alpha);
		gui->begin_content("back_alpha", SCALE(0, 0), SCALE(0, 0), SCALE(0, 0));
		{
			draw->rect_filled(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->window.background, 0.6), SCALE(var->window.rounding));
			gui->loading();
		}
		gui->end_content();
		gui->pop_var();

		gui->move_window(var->winapi.hwnd, var->winapi.rc);
	}
	gui->end();

	if (ImGui::IsKeyPressed(ImGuiKey_Equal) && ImGui::IsKeyDown(ImGuiKey_LeftAlt) && var->gui.stored_dpi < 200)
	{
		var->gui.stored_dpi += 10;
		var->gui.dpi_changed = true;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Minus) && ImGui::IsKeyDown(ImGuiKey_LeftAlt) && var->gui.stored_dpi > 80)
	{
		var->gui.stored_dpi -= 10;
		var->gui.dpi_changed = true;
	}

	if (var->gui.dpi != var->gui.stored_dpi / 100.f)
	{
		var->gui.dpi_changed = true;
		var->gui.update_size = true;
	}
}

void c_gui::draw_main_page()
{
	gui->initialize();

	gui->push_var(style_var_window_shadow_size, SCALE(0, 0));
	gui->set_next_window_pos(ImVec2(0, 0));
	gui->set_next_window_size(SCALE(var->window.size));
	gui->begin("menu", nullptr, var->window.flags);
	{
		gui->set_style();
		gui->draw_decorations();

		gui->easing(var->gui.stage_alpha, var->gui.active_stage == var->gui.stage_count ? 1.f : 0.f, 24.f, static_easing);
		if (var->gui.stage_alpha == 0.f)
			var->gui.active_stage = var->gui.stage_count;

		gui->easing(var->gui.content_alpha, var->gui.active_section == var->gui.section_count ? 1.f : 0.f, 24.f, static_easing);

		if (var->gui.content_alpha == 0.f)
			var->gui.active_section = var->gui.section_count;

		gui->easing(var->window.size.x, var->gui.stage_count > 0 ? var->window.new_width : var->window.default_size.x, 1600.f, static_easing);
		gui->easing(var->window.size.y, var->gui.stage_count > 1 ? (var->gui.section_count == 1 ? var->window.section_1_height : var->window.stage_2_height) : (var->gui.section_count == 1 ? var->window.section_1_height : var->window.default_size.y), 400.f, static_easing);

		gui->set_pos(SCALE(0, 0), pos_all);
		gui->begin_content("content", gui->window_size(), SCALE(elements->window.padding), SCALE(elements->widgets.spacing.x, elements->window.padding.y), window_flags_no_scroll_with_mouse | window_flags_no_scrollbar);
		{
			gui->push_var(style_var_alpha, var->gui.stage_alpha);
			if (var->gui.active_stage > 0)
			{
				widgets->top_bar("emperium", "");
				gui->sameline();
				widgets->settings_button();
			}
			gui->pop_var();
			gui->push_var(style_var_alpha, var->gui.content_alpha * var->gui.stage_alpha);
			if (var->gui.active_section == 1)
			{
				widgets->widgets_child("redeem_license", gui->language("Redeem License", "Redeem License", true), "R");
				{
					static char redeem_buffer[64]{};
					widgets->text_field("redeem_license_field", "License", "d", redeem_buffer, sizeof(redeem_buffer));

					gui->dummy(SCALE(0, elements->widgets.spacing.y));

					ImVec2 button_size = ImVec2(gui->content_avail().x, SCALE(elements->textfield.size.y));
					ImGui::PushStyleColor(ImGuiCol_Button, draw->get_clr(clr->main.accent));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, draw->get_clr(clr->main.accent, 0.9f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, draw->get_clr(clr->main.accent, 0.8f));
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, SCALE(elements->widgets.rounding));
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SCALE(elements->widgets.spacing));
					bool redeem_clicked = ImGui::Button("Redeem", button_size);
					ImGui::PopStyleVar(2);
					ImGui::PopStyleColor(3);

					if (redeem_clicked && loader_ui && redeem_buffer[0] != '\0')
					{
						loader_ui->handle_license_redeem(redeem_buffer);
						std::memset(redeem_buffer, 0, sizeof(redeem_buffer));
					}
				}
				widgets->widgets_end_child();
			}
			if (var->gui.active_section == 0)
			{
				gui->begin_content("s_g_content", SCALE(0, 0), SCALE(0, 0), SCALE(elements->window.padding));
				{
					const std::vector<c_loader_ui::product_view>* product_list = loader_ui ? &loader_ui->get_product_views() : nullptr;
					const size_t product_count = product_list ? product_list->size() : 0;

					if (var->gui.active_stage == 1)
					{
						widgets->games_child("subscriptions_group", gui->language("Subscriptions", "Subscriptions", true), "H", std::to_string(product_count));
						{
							if (product_count == 0)
							{
								ImGuiWindow* child_window = gui->get_window();
								if (child_window)
								{
									ImVec2 text_pos = child_window->DC.CursorPos + SCALE(elements->widgets.spacing);
									draw->text_clipped(
										child_window->DrawList,
										font->get(suisse_intl_medium_data, 16),
										text_pos,
										text_pos + ImVec2(child_window->ContentRegionRect.GetWidth(), SCALE(elements->game_card.height)),
										draw->get_clr(clr->main.text, 0.48f),
										"No subscriptions available",
										nullptr,
										nullptr,
										ImVec2(0.f, 0.f)
									);
								}
							}
							else
							{
								size_t index = 0;
								for (const auto& product : *product_list)
								{
									std::string widget_id = product.display_name.empty() ? ("subscription_card_" + std::to_string(index)) : (product.display_name + "_card");
									std::string subtitle = product.display_status;
									if (!product.expires_at.empty())
									{
										if (!subtitle.empty())
											subtitle += " | ";
										subtitle += format_expiration_remaining(product.expires_at);
									}

									std::string display_label = product.display_name;
									if (display_label.empty())
									{
										if (product.subscription)
										{
											display_label = !product.subscription->plan.empty() ? product.subscription->plan : product.subscription->plan_id;
										}
										if (display_label.empty())
											display_label = "Product";
									}

									bool ready = product.active;
									widgets->game_card(widget_id, display_label, subtitle, static_cast<int>(product.index), ready);
									if ((index + 1) % 3 != 0 && index + 1 < product_count)
										gui->sameline();
									++index;
								}
							}
						}
						widgets->games_end_child();
					}
					else if (var->gui.active_stage >= 2)
					{
						if (!product_list || product_count == 0)
						{
							var->gui.stage_count = 1;
						}
						else
						{
							size_t product_index = static_cast<size_t>(var->gui.active_stage - 2);
							if (product_index >= product_count)
							{
								var->gui.stage_count = 1;
							}
							else
							{
								const auto& product = (*product_list)[product_index];

								std::string detail_name = product.display_name;
								if (detail_name.empty())
								{
									if (product.subscription)
									{
										detail_name = !product.subscription->plan.empty() ? product.subscription->plan : product.subscription->plan_id;
									}
									if (detail_name.empty())
										detail_name = "Product";
								}

								std::vector<std::string> info_lines;
								const std::string status_line = "Status: " + (product.display_status.empty()
									? gui->language("Unknown", "Unknown")
									: product.display_status);
								info_lines.emplace_back(status_line);


								std::string desc_text;
								for (size_t i = 0; i < info_lines.size(); ++i)
								{
									if (i > 0)
										desc_text += '\n';
									desc_text += info_lines[i];
								}

								std::string plan_identifier = (product.subscription && !product.subscription->plan_id.empty())
									? product.subscription->plan_id
									: "-";
								std::string default_file_id = (product.subscription && !product.subscription->default_file_id.empty())
									? product.subscription->default_file_id
									: "-";
								std::string expires_text = !product.expires_at.empty()
									? format_expiration_remaining(product.expires_at)
									: "-";
								std::string status_text = !product.display_status.empty()
									? product.display_status
									: gui->language("Unknown", "Unknown");

								const bool has_default_file = product.subscription && !product.subscription->default_file_id.empty();
								const bool can_launch = product.active && has_default_file;
								std::string helper_text;
								if (!product.active) {
									helper_text = gui->language("Subscription inactive", "Subscription inactive");
								}
								else if (!has_default_file) {
									helper_text = gui->language("No default file configured", "No default file configured");
								}
								else {
									helper_text = gui->language("Ready to launch", "Ready to launch");
								}

								bool launch_clicked = widgets->product_page(
									product.video_player,
									product.image,
									detail_name,
									desc_text,
									plan_identifier,
									expires_text,
									status_text,
									default_file_id,
									can_launch,
									helper_text
								);

								if (launch_clicked)
								{
									if (can_launch && loader_ui)
									{
										loader_ui->handle_launch_request(product.subscription->default_file_id);
										var->gui.loading = true;
									}
								}
							}
						}
					}
				}
				gui->end_content();
			}
			gui->pop_var();
		}
		gui->end_content();
		gui->pop_var();

		bool bg{ false };
		if (gui->is_window_cond(GetCurrentContext()->NavWindow, { "dropdown" }))
			bg = true;
		gui->easing(elements->loading.window_alpha, bg || var->gui.loading ? 1.f : 0.f, 8.f, static_easing);

		gui->set_pos(SCALE(0, 0), pos_all);
		gui->push_var(style_var_alpha, elements->loading.window_alpha);
		gui->begin_content("back_alpha", SCALE(0, 0), SCALE(0, 0), SCALE(0, 0));
		{
			draw->rect_filled(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->window.background, 0.6), SCALE(var->window.rounding));
			gui->loading();
		}
		gui->end_content();
		gui->pop_var();

		gui->move_window(var->winapi.hwnd, var->winapi.rc);
	}
	gui->end();

	if (ImGui::IsKeyPressed(ImGuiKey_Equal) && ImGui::IsKeyDown(ImGuiKey_LeftAlt) && var->gui.stored_dpi < 200)
	{
		var->gui.stored_dpi += 10;
		var->gui.dpi_changed = true;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Minus) && ImGui::IsKeyDown(ImGuiKey_LeftAlt) && var->gui.stored_dpi > 80)
	{
		var->gui.stored_dpi -= 10;
		var->gui.dpi_changed = true;
	}

	if (var->gui.dpi != var->gui.stored_dpi / 100.f)
	{
		var->gui.dpi_changed = true;
		var->gui.update_size = true;
	}
}
