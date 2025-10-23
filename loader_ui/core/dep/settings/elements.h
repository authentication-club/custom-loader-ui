#pragma once
#include <string>
#include "imgui.h"

class c_elements
{
public:

	struct
	{
		ImVec2 padding{ 16, 16 };
	} window;

	struct
	{
		float log_reg_width{ 400 };
		float img_height{ 436 };
		ImVec2 padding{ 32, 32 };
		float shadow_radius{ 24 };
		float button_padding{ 14 };
		float button_spacing{ 7 };
		float window_height{ 144 };
	} log_reg_page;

	struct
	{
		float height{ 60 };
		ImVec2 padding{ 12, 12 };
		ImVec2 icon_zone_size{ 36, 36 };
	} info_card;

	struct
	{
		float rounding{ 8 };
		ImVec2 spacing{ 8, 8 };
	} widgets;

	struct
	{
		ImVec2 size{ 336, 40 };
		ImVec2 icon_zone_size{ 14, 14 };
		float padding{ 14 };
	} textfield;

	struct
	{
		ImVec2 rect_size{ 12, 12 };
		std::vector<std::string> games{ "Rust", "Valorant", "Apex Legends", "Fortnite","PUBG","COD Warzone"};
		std::vector<std::string> updates{ "Grenade Prediction", "Vanguard Bypass", "Soft Aim", "Anti Recoil" };
		float zone_padding{ 4 };
		float padding{ 2 };
		float rounding{ 4 };
		float text_zone_height{ 14 };
	} version_card;

	struct
	{
		float height{ 44 };
		ImVec2 padding{ 12, 12 };
		ImVec2 icon_size{ 20, 20 };
		float icon_2_zone_width{ 14 };
		float texts_spacing{ 4 };
	} top_bar;

	struct
	{
		float top_height{ 14 };
		float icon_zone_width{ 12 };
		float text_spacing{ 4 };
		ImVec2 padding{ 12, 12 };
	} child;

	struct
	{
		float height{ 196 };
		ImVec2 padding{ 12, 12 };
		float img_height{ 126 };
		float img_rounding{ 4 };
		ImVec2 rect_size{ 34, 34 };
		float text_spacing{ 4 };
		ImVec2 icon_size{ 12, 14 };
		std::vector<std::string> fps_games{ "Rust", "Valorant", "Apex Legends" };
		std::vector<std::string> fps_count{ "12.679", "8.124", "4.398" };
		std::vector<bool> fps_ready{ true, false, true };

		std::vector<std::string> royale_games{ "Fortnite", "PUBG", "COD Warzone" };
		std::vector<std::string> royale_count{ "9.681", "7.289", "3.216" };
		std::vector<bool> royale_ready{ true, false, false };
	} game_card;

	struct
	{
		ImVec2 size{ 462, 260 };
		float control_height{ 29 };
		float logo_height_1{ 19 };
		float logo_height_2{ 16 };
		float zone_padding{ 7 };
		float slider_height{ 6 };
		float line_padding{ 1 };
		float volume_width{ 50 };
		float volume_padding{ 4 };
		ImVec2 buttons_size{ 14, 14 };
	} player;

	struct
	{
		float back_button_height{  14 };
		float back_button_padding{ 4 };
		float game_zone_height{ 19 };
		ImVec2 img_size{ 16, 16 };
	} product_page;

	struct
	{
		float height{ 16 };
		ImVec2 rect_size{ 14, 14 };
		float rounding{ 4 };
	} checkbox;

	struct
	{
		float spacing{ 7 };
		float padding{ 14 };
		float but_spacing{ 4 };
		ImVec2 img_size{ 20, 14 };
	} selection;

	struct
	{
		float radius{ 10 };
		float notify_height{ 65 };
		float button_padding{ 22 };
		float window_alpha{ 0 };
	} loading;

};

inline std::unique_ptr<c_elements> elements = std::make_unique<c_elements>();
