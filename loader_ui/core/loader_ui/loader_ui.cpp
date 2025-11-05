#define IMGUI_DEFINE_MATH_OPERATORS
#include "loader_ui.h"
#include "../dep/headers/includes.h"
#include "../dep/data/fonts.h"
#include "../dep/headers/blur.h"
#include "../dep/imgui/imgui.h"
#include "../dep/imgui/imgui_impl_win32.h"
#include "../dep/imgui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <d3dx11.h>
#include <dwmapi.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>
#include <vector>
#include <iomanip>
#include <mutex>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <dxgitype.h>
#include <dxgiformat.h>
#include <wrl/client.h>
#include <gdiplus.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#include "../dep/thirdparty/stb_image.h"
#include <windows.h>
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")
extern c_loader_ui* loader_ui;
namespace {
    std::string to_lower_copy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    std::string format_hr(HRESULT hr) {
        std::ostringstream oss;
        oss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
            << static_cast<unsigned long>(hr);
        return oss.str();
    }

    void debug_log(const std::string&) {}

    std::string sanitize_filename(std::string_view value) {
        std::string result;
        result.reserve(value.size());
        for (char ch : value) {
            unsigned char c = static_cast<unsigned char>(ch);
            if (std::isalnum(c) || ch == '-' || ch == '_' || ch == '.') {
                result.push_back(static_cast<char>(c));
            } else {
                result.push_back('_');
            }
        }
        if (result.empty()) {
            result = "plan";
        }
        return result;
    }

    std::string extension_from_mime(std::string_view mime) {
        if (mime == "video/mp4" || mime == "video/mpeg") {
            return ".mp4";
        }
        if (mime == "video/webm") {
            return ".webm";
        }
        if (mime == "video/ogg" || mime == "video/ogv" || mime == "application/ogg") {
            return ".ogv";
        }
        return ".bin";
    }

    using Microsoft::WRL::ComPtr;
    using unique_hglobal = std::unique_ptr<void, decltype(&::GlobalFree)>;
    std::once_flag g_gdiplus_init_flag;
    ULONG_PTR g_gdiplus_token = 0;
    Gdiplus::Status g_gdiplus_status = Gdiplus::GenericError;

    Gdiplus::Status ensure_gdiplus() {
        std::call_once(g_gdiplus_init_flag, []() {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::Status status = Gdiplus::GdiplusStartup(&g_gdiplus_token, &input, nullptr);
            g_gdiplus_status = status;
            if (status != Gdiplus::Ok) {
                g_gdiplus_token = 0;
            }
        });
        return g_gdiplus_status;
    }

    ID3D11ShaderResourceView* create_texture_with_gdiplus(
        ID3D11Device* device,
        const unsigned char* data,
        size_t size,
        std::string_view debug_name
    ) {
        if (!device || !data || size == 0) { return nullptr; }

        if (ensure_gdiplus() != Gdiplus::Ok) { return nullptr; }

        unique_hglobal memory_holder(::GlobalAlloc(GMEM_MOVEABLE, size), ::GlobalFree);
        HGLOBAL h_memory = memory_holder.get();
        if (!h_memory) { return nullptr; }

        void* mem_ptr = ::GlobalLock(h_memory);
        if (!mem_ptr) { return nullptr; }
        std::memcpy(mem_ptr, data, size);
        ::GlobalUnlock(h_memory);

        IStream* stream = nullptr;
        HRESULT hr = ::CreateStreamOnHGlobal(h_memory, FALSE, &stream);
        if (FAILED(hr) || !stream) { return nullptr; }

        std::unique_ptr<Gdiplus::Bitmap> bitmap(Gdiplus::Bitmap::FromStream(stream, FALSE));
        if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) { stream->Release(); return nullptr; }

        const UINT width = bitmap->GetWidth();
        const UINT height = bitmap->GetHeight();
        if (width == 0 || height == 0) { return nullptr; }

        Gdiplus::Rect lock_rect(0, 0, width, height);
        Gdiplus::BitmapData bitmap_data{};
        if (bitmap->LockBits(&lock_rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmap_data) != Gdiplus::Ok) { stream->Release(); return nullptr; }

        const long stride = bitmap_data.Stride;
        const bool inverted = stride < 0;
        const size_t abs_stride = static_cast<size_t>(inverted ? -stride : stride);
        std::vector<unsigned char> pixel_data(static_cast<size_t>(width) * height * 4u);

        const unsigned char* src_base = static_cast<unsigned char*>(bitmap_data.Scan0);
        for (UINT y = 0; y < height; ++y) {
            const unsigned char* src_row = src_base + (inverted ? (height - 1 - y) : y) * abs_stride;
            unsigned char* dst_row = pixel_data.data() + static_cast<size_t>(y) * width * 4;
            for (UINT x = 0; x < width; ++x) {
                const unsigned char* src_px = src_row + x * 4;
                dst_row[x * 4 + 0] = src_px[2];
                dst_row[x * 4 + 1] = src_px[1];
                dst_row[x * 4 + 2] = src_px[0];
                dst_row[x * 4 + 3] = src_px[3];
            }
        }

        bitmap->UnlockBits(&bitmap_data);
        stream->Release();

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init_data{};
        init_data.pSysMem = pixel_data.data();
        init_data.SysMemPitch = static_cast<UINT>(width * 4);

        ComPtr<ID3D11Texture2D> texture;
        hr = device->CreateTexture2D(&desc, &init_data, &texture); if (FAILED(hr)) { return nullptr; }

        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = 1;

        ID3D11ShaderResourceView* srv = nullptr;
        hr = device->CreateShaderResourceView(texture.Get(), nullptr, &srv); if (FAILED(hr)) { return nullptr; }

        return srv;
    }

    bool looks_like_hex_escaped(const std::vector<unsigned char>& data) {
        return data.size() >= 4 && data[0] == '\\' && (data[1] == 'x' || data[1] == 'X');
    }

    unsigned char hex_value(unsigned char c) {
        if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(10 + (c - 'a'));
        if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(10 + (c - 'A'));
        return 0xFF;
    }

    std::vector<unsigned char> decode_hex_escapes(const std::vector<unsigned char>& data, bool& converted) {
        converted = false;

        if (data.size() >= 4 && data[0] == '\\' && (data[1] == 'x' || data[1] == 'X')) {
            std::vector<unsigned char> output;
            output.reserve((data.size() - 2) / 2);
            unsigned char high = 0xFF;
            for (size_t i = 2; i < data.size(); ++i) {
                unsigned char ch = data[i];
                if (std::isspace(ch)) {
                    continue;
                }

                unsigned char value = hex_value(ch);
                if (value == 0xFF) {
                    output.clear();
                    break;
                }

                if (high == 0xFF) {
                    high = value;
                }
                else {
                    output.push_back(static_cast<unsigned char>((high << 4) | value));
                    high = 0xFF;
                }
            }

            if (!output.empty() && high == 0xFF) {
                converted = true;
                return output;
            }
        }

        std::vector<unsigned char> output;
        output.reserve(data.size());

        for (size_t i = 0; i < data.size();) {
            unsigned char ch = data[i];
            if (ch == '\\' && (i + 3) < data.size() && (data[i + 1] == 'x' || data[i + 1] == 'X')) {
                unsigned char hi = hex_value(data[i + 2]);
                unsigned char lo = hex_value(data[i + 3]);
                if (hi != 0xFF && lo != 0xFF) {
                    output.push_back(static_cast<unsigned char>((hi << 4) | lo));
                    i += 4;
                    converted = true;
                    continue;
                }
            }
            output.push_back(ch);
            ++i;
        }

        return output;
    }
    ID3D11ShaderResourceView* create_texture_with_stbi(
        ID3D11Device* device,
        const unsigned char* data,
        size_t size,
        std::string_view debug_name
    ) {
        if (!device || !data || size == 0) {
            return nullptr;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* decoded = stbi_load_from_memory(
            data,
            static_cast<int>(size),
            &width,
            &height,
            &channels,
            STBI_rgb_alpha
        );
        if (!decoded) {
            const char* reason = stbi_failure_reason();
            return nullptr;
        }

        std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(decoded, stbi_image_free);

        if (width <= 0 || height <= 0) {
            return nullptr;
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init_data{};
        init_data.pSysMem = pixels.get();
        init_data.SysMemPitch = static_cast<UINT>(width * 4);

        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&desc, &init_data, &texture);
        if (FAILED(hr)) {
            return nullptr;
        }

        ID3D11ShaderResourceView* srv = nullptr;
        hr = device->CreateShaderResourceView(texture.Get(), nullptr, &srv);
        if (FAILED(hr)) {
            return nullptr;
        }
        return srv;
    }

    ID3D11ShaderResourceView* create_solid_color_icon(ID3D11Device* device, const ImVec4& color) {
        if (!device) {
            return nullptr;
        }

        constexpr UINT icon_size = 96;
        std::vector<unsigned char> pixel_data(static_cast<size_t>(icon_size) * icon_size * 4u);

        auto to_byte = [](float channel) -> unsigned char {
            channel = std::clamp(channel, 0.0f, 1.0f);
            return static_cast<unsigned char>(channel * 255.0f + 0.5f);
        };

        const unsigned char r = to_byte(color.x);
        const unsigned char g = to_byte(color.y);
        const unsigned char b = to_byte(color.z);
        const unsigned char a = to_byte(color.w);

        for (size_t i = 0; i < pixel_data.size(); i += 4) {
            pixel_data[i + 0] = r;
            pixel_data[i + 1] = g;
            pixel_data[i + 2] = b;
            pixel_data[i + 3] = a;
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = icon_size;
        desc.Height = icon_size;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init_data{};
        init_data.pSysMem = pixel_data.data();
        init_data.SysMemPitch = icon_size * 4;

        ID3D11Texture2D* texture = nullptr;
        HRESULT hr = device->CreateTexture2D(&desc, &init_data, &texture);
        if (FAILED(hr)) {
            return nullptr;
        }

        ID3D11ShaderResourceView* srv = nullptr;
        hr = device->CreateShaderResourceView(texture, nullptr, &srv);
        texture->Release();
        if (FAILED(hr)) {
            return nullptr;
        }

        return srv;
    }
}
c_loader_ui::c_loader_ui() : should_close(false), initialized(false), products_dirty(false) {
    std::error_code ec;
    if (const char* local_appdata = std::getenv("LOCALAPPDATA")) {
        video_cache_directory = std::filesystem::path(local_appdata) / "auth_plan_videos";
    } else {
        auto fallback = std::filesystem::temp_directory_path(ec);
        if (!ec && !fallback.empty()) {
            video_cache_directory = fallback / "auth_plan_videos";
        } else {
            video_cache_directory.clear();
        }
    }
}
c_loader_ui::~c_loader_ui() { shutdown(); }

bool c_loader_ui::initialize(const ui_config& cfg) {
    if (initialized) { return true; }
    config = cfg;
    initialized = true;
    return true;
}

void c_loader_ui::shutdown() {
    if (!initialized) { return; }
    release_product_views();
    release_fallback_icons();
    user = user_profile{};
    user.subscriptions.clear();
    subscription_storage.clear();
    state = ui_state{};
    initialized = false;
}

bool c_loader_ui::should_run() const {
    if (!initialized) { return false; }

    return !should_close;
}

void c_loader_ui::update() {
    if (!initialized) { return; }
}

static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void c_loader_ui::render() 
{
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    var->winapi.hwnd = ::CreateWindowW(wc.lpszClassName, L"Loader", WS_POPUP, (GetSystemMetrics(SM_CXSCREEN) / 2) - (SCALE(var->window.size.x) / 2), (GetSystemMetrics(SM_CYSCREEN) / 2) - (SCALE(var->window.size.y) / 2), SCALE(var->window.size.x), SCALE(var->window.size.y), NULL, NULL, wc.hInstance, NULL);

    SetLayeredWindowAttributes(var->winapi.hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(var->winapi.hwnd, &margins);
    GetWindowRect(var->winapi.hwnd, &var->winapi.rc);

    var->winapi.is_fullscreen = false;
    ZeroMemory(&var->winapi.restore_rect, sizeof(RECT));

    if (!CreateDeviceD3D(var->winapi.hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }

    ::ShowWindow(var->winapi.hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(var->winapi.hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(var->winapi.hwnd);
    ImGui_ImplDX11_Init(var->winapi.device_dx11, var->winapi.device_context);

    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;

    io.Fonts->AddFontDefault();

    io.Fonts->AddFontDefault(&font_config);
    io.Fonts->GetGlyphRangesCyrillic();

    io.Fonts->Build();

    ImVec4 clear_color = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);

    D3DX11_IMAGE_LOAD_INFO info; ID3DX11ThreadPump* pump{ nullptr };

    initialize_fallback_icons();

    loader_ui = this;
    bool done = false;
    static ImVec2 last_size = var->window.size;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_SwapChainOccluded && var->winapi.swap_chain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            var->winapi.swap_chain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        font->update();

        if (products_dirty && var->winapi.device_dx11) {
            rebuild_product_views();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (state.show_login_window)
        {
			gui->draw_login_page();
        }

        if (state.show_main_window)
        {
			gui->draw_main_page();
        }
        if (GetAsyncKeyState(VK_ESCAPE) & 0x1)
            SendMessage(var->winapi.hwnd, WM_CLOSE, 0, 0);

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0 };
        var->winapi.device_context->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        var->winapi.device_context->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = var->winapi.swap_chain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    loader_ui = nullptr;
    var->gui.rust_player.cleanup_video();
    var->gui.apex_player.cleanup_video();
    var->gui.fortnite_player.cleanup_video();
    var->gui.pubg_player.cleanup_video();
    var->gui.cod_player.cleanup_video();
    var->gui.valorant_player.cleanup_video();

    release_product_views();
    release_blur_resources();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(var->winapi.hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}


void c_loader_ui::set_authenticated(bool auth, user_profile* new_profile) {
    state.authenticated = auth;

    if (!state.authenticated) {
        subscription_storage.clear();
        user = user_profile{};
        user.subscriptions.clear();
        show_login();
        return;
    }

    if (new_profile != nullptr) {
        user.username = new_profile->username;
        user.email = new_profile->email;
        user.ip = new_profile->ip;

        subscription_storage.clear();
        user.subscriptions.clear();
        subscription_storage.reserve(new_profile->subscriptions.size());
        for (auto* source : new_profile->subscriptions) {
            if (!source) {
                continue;
            }
            auto copy = std::make_unique<user_subscription>(*source);
            user.subscriptions.push_back(copy.get());
            subscription_storage.emplace_back(std::move(copy));
        }
    } else {
        subscription_storage.clear();
        user.subscriptions.clear();
    }

    products_dirty = true;
    show_main();
}


void c_loader_ui::set_status_message(const std::string& message) {
    state.status_message = message;
    state.error_message.clear();
}

void c_loader_ui::set_error_message(const std::string& message) {
    state.error_message = message;
    state.status_message.clear();
}

void c_loader_ui::set_loading(bool active) {
    var->gui.loading = active;
    if (!active) {
        var->gui.update_size = true;
    }
}

void c_loader_ui::show_login() {
    state.show_login_window = true;
    state.show_register_window = false;
    state.show_main_window = false;

    var->gui.stage_count = 0;
    var->gui.active_stage = 0;
    var->gui.section_count = 0;
    var->gui.active_section = 0;
    var->gui.stage_alpha = 0.f;
    var->gui.content_alpha = 0.f;
    var->gui.registration = false;
    var->gui.loading = false;
    var->gui.update_size = true;
    release_product_views();
    products_dirty = false;
}

void c_loader_ui::show_register() {
    state.show_login_window = false;
    state.show_register_window = true;
    state.show_main_window = false;

    var->gui.stage_count = 0;
    var->gui.active_stage = 0;
    var->gui.section_count = 0;
    var->gui.active_section = 0;
    var->gui.stage_alpha = 0.f;
    var->gui.content_alpha = 0.f;
    var->gui.registration = true;
    var->gui.loading = false;
    var->gui.update_size = true;
    release_product_views();
    products_dirty = false;
}

void c_loader_ui::show_main() {
    state.show_login_window = false;
    state.show_register_window = false;
    state.show_main_window = true;

    var->gui.stage_count = 1;
    var->gui.active_stage = var->gui.stage_count;
    var->gui.section_count = 0;
    var->gui.active_section = var->gui.section_count;
    var->gui.stage_alpha = 1.f;
    var->gui.content_alpha = 1.f;
    var->gui.registration = false;
    var->gui.loading = false;
    var->gui.update_size = true;
    products_dirty = true;
}

void c_loader_ui::set_login_callback(LoginCallback callback) {
    login_callback = callback;
}

void c_loader_ui::set_register_callback(RegisterCallback callback) {
    register_callback = callback;
}

void c_loader_ui::set_license_callback(LicenseCallback callback) {
    license_callback = callback;
}

void c_loader_ui::set_exit_callback(ExitCallback callback) {
    exit_callback = callback;
}

void c_loader_ui::set_filestream_callback(FilestreamCallback callback) {
    filestream_callback = callback;
}

void c_loader_ui::handle_login_request(const std::string& username, const std::string& password) {
    if (login_callback) {
        login_callback(username, password);
    }
}

void c_loader_ui::handle_register_request(const std::string& username, const std::string& password, const std::string& license) {
    if (register_callback) {
        register_callback(username, password, license);
    }
}

void c_loader_ui::handle_launch_request(const std::string& file_id) {
    if (filestream_callback && !file_id.empty()) {
        filestream_callback(file_id);
    }
}

void c_loader_ui::handle_license_redeem(const std::string& license) {
    if (license_callback && !user.username.empty() && !license.empty()) {
        license_callback(user.username, license);
    }
}

const std::vector<c_loader_ui::product_view>& c_loader_ui::get_product_views() const {
    return products;
}

void c_loader_ui::release_product_views() {
    for (auto& player : video_players) {
        if (player) {
            player->cleanup_video();
        }
    }
    video_players.clear();

    for (auto& path : video_cache_paths) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    video_cache_paths.clear();

    for (auto* sub : user.subscriptions) {
        if (sub) {
            sub->product_video_path.clear();
        }
    }

    for (auto& view : products) {
        if (view.owns_image && view.image) {
            view.image->Release();
            view.image = nullptr;
        }
    }
    products.clear();
    var->gui.subscription_textures.clear();
}

void c_loader_ui::initialize_fallback_icons() {
    ID3D11Device* device = var->winapi.device_dx11;
    if (!device) {
        return;
    }

    auto ensure_icon = [&](ID3D11ShaderResourceView*& slot,
                           ID3D11ShaderResourceView*& fallback_slot,
                           const ImVec4& color,
                           std::string_view name) {
        if (slot) {
            if (fallback_slot && slot != fallback_slot) {
                fallback_slot->Release();
                fallback_slot = nullptr;
            }
            return;
        }

        fallback_slot = create_solid_color_icon(device, color);
        if (fallback_slot) {
            slot = fallback_slot;

        }
        else {

        }
    };
}

void c_loader_ui::release_fallback_icons() {
    auto release_icon = [&](ID3D11ShaderResourceView*& slot,
                            ID3D11ShaderResourceView*& fallback_slot,
                            std::string_view name) {
        if (!fallback_slot) {
            return;
        }

        if (slot == fallback_slot) {
            slot = nullptr;
        }

        fallback_slot->Release();
        fallback_slot = nullptr;
    };
}

void c_loader_ui::rebuild_product_views() {
    if (!var->winapi.device_dx11) {
        return;
    }

    release_product_views();

    if (user.subscriptions.empty()) {
        products_dirty = false;
        return;
    }

    initialize_fallback_icons();

    products.reserve(user.subscriptions.size());
    var->gui.subscription_textures.reserve(user.subscriptions.size());
    video_players.reserve(user.subscriptions.size());

    for (size_t raw_index = 0; raw_index < user.subscriptions.size(); ++raw_index) {
        user_subscription* sub = user.subscriptions[raw_index];
        if (!sub) {
            continue;
        }

        product_view view;
        view.index = products.size();
        view.subscription = sub;
        view.display_name = !sub->plan.empty() ? sub->plan : sub->plan_id;
        view.display_status = sub->status;
        view.expires_at = sub->expires_at;
        view.active = to_lower_copy(sub->status) == "active";
        view.video_player = nullptr;

        ID3D11ShaderResourceView* texture = nullptr;
        bool owns_image = false;

        debug_log("plan '" + view.display_name + "' image_bytes=" + std::to_string(sub->product_image.size()) +
                  " video_bytes=" + std::to_string(sub->product_video.size()) +
                  " video_path=" + (sub->product_video_path.empty() ? "<none>" : sub->product_video_path) +
                  " video_mime=" + (sub->product_video_mime.empty() ? "<none>" : sub->product_video_mime));

        if (!sub->product_image.empty()) {
            if (sub->product_image.size() >= 8) {
                std::ostringstream hex;
                hex << std::hex << std::uppercase << std::setfill('0');
                size_t sample = std::min<size_t>(16, sub->product_image.size());
                for (size_t i = 0; i < sample; ++i) {
                    hex << std::setw(2) << static_cast<int>(sub->product_image[i]);
                    if (i + 1 < sample) {
                        hex << ' ';
                    }
                }
            }
            bool had_escaped_bytes = false;
            if (looks_like_hex_escaped(sub->product_image)) {
                auto decoded = decode_hex_escapes(sub->product_image, had_escaped_bytes);
                if (had_escaped_bytes && !decoded.empty()) {
                    sub->product_image = std::move(decoded);

                    if (sub->product_image.size() >= 8) {
                        std::ostringstream hex_after;
                        hex_after << std::hex << std::uppercase << std::setfill('0');
                        size_t sample_after = std::min<size_t>(16, sub->product_image.size());
                        for (size_t i = 0; i < sample_after; ++i) {
                            hex_after << std::setw(2) << static_cast<int>(sub->product_image[i]);
                            if (i + 1 < sample_after) {
                                hex_after << ' ';
                            }
                        }
                    }
                }
            }

            D3DX11_IMAGE_LOAD_INFO load_info = {};
            ID3DX11ThreadPump* pump = nullptr;
            HRESULT hr = D3DX11CreateShaderResourceViewFromMemory(
                var->winapi.device_dx11,
                sub->product_image.data(),
                static_cast<SIZE_T>(sub->product_image.size()),
                &load_info,
                pump,
                &texture,
                nullptr
            );

            if (SUCCEEDED(hr)) {
                owns_image = true;
                debug_log("    D3DX11CreateShaderResourceViewFromMemory succeeded");
            } else {
                texture = nullptr;
                debug_log("    D3DX11CreateShaderResourceViewFromMemory failed hr=" + format_hr(hr));
            }

            if (!texture) {
                texture = create_texture_with_gdiplus(
                    var->winapi.device_dx11,
                    sub->product_image.data(),
                    sub->product_image.size(),
                    view.display_name
                );
                if (texture) {
                    owns_image = true;
                    debug_log("    create_texture_with_gdiplus succeeded");
                }

                if (!texture) {
                    texture = create_texture_with_stbi(
                        var->winapi.device_dx11,
                        sub->product_image.data(),
                        sub->product_image.size(),
                        view.display_name
                    );
                    if (texture) {
                        owns_image = true;
                        debug_log("    create_texture_with_stbi succeeded");
                    }
                }
            }
        } else {
            debug_log("    no product image bytes available");
        }

        if (!texture) {
            owns_image = false;
            debug_log("    using fallback texture");
        } else {
            debug_log("    final texture acquired owns_image=" + std::string(owns_image ? "true" : "false"));
        }

        if (!sub->product_video.empty()) {
            bool video_had_hex = false;
            if (looks_like_hex_escaped(sub->product_video)) {
                auto decoded_video = decode_hex_escapes(sub->product_video, video_had_hex);
                if (video_had_hex && !decoded_video.empty()) {
                    debug_log("    detected hex-escaped video payload; decoding to binary");
                    sub->product_video = std::move(decoded_video);
                }
            }

            std::ostringstream sample_hex;
            size_t sample_len = std::min<size_t>(sub->product_video.size(), 16);
            for (size_t i = 0; i < sample_len; ++i) {
                sample_hex << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                           << static_cast<int>(sub->product_video[i]);
                if (i + 1 < sample_len) {
                    sample_hex << ' ';
                }
            }
            debug_log("video sample bytes: " + sample_hex.str());
        }

        // Prepare cached video file if one is available
        if (sub->product_video_path.empty() && !sub->product_video.empty()) {
            if (!video_cache_directory.empty()) {
                std::error_code dir_ec;
                std::filesystem::create_directories(video_cache_directory, dir_ec);
                if (dir_ec) {
                    debug_log("failed to create video cache directory: " + dir_ec.message());
                } else {
                    std::string base_name = !sub->plan_id.empty() ? sub->plan_id : view.display_name;
                    std::string filename = sanitize_filename(base_name);
                    filename += "_";
                    filename += std::to_string(view.index);
                    filename += extension_from_mime(sub->product_video_mime);

                    std::filesystem::path file_path = video_cache_directory / filename;
                    debug_log("writing video cache file " + file_path.string());
                    std::ofstream video_out(file_path, std::ios::binary | std::ios::trunc);
                    if (video_out.is_open()) {
                        video_out.write(reinterpret_cast<const char*>(sub->product_video.data()),
                                        static_cast<std::streamsize>(sub->product_video.size()));
                        if (video_out.good()) {
                            video_out.close();
                            sub->product_video_path = file_path.string();
                            video_cache_paths.push_back(file_path);
                            debug_log("video cache file written successfully");
                        } else {
                            debug_log("failed to write entire video cache file");
                            video_out.close();
                            std::error_code remove_ec;
                            std::filesystem::remove(file_path, remove_ec);
                        }
                    } else {
                        debug_log("failed to open video cache file for writing");
                    }
                }
            } else {
                debug_log("video cache directory unavailable");
            }
        }

        if (!sub->product_video_path.empty() && var->winapi.device_dx11 && var->winapi.device_context) {
            auto player_shared = std::make_shared<c_video_player>();
            player_shared->g_pd3dDevice = var->winapi.device_dx11;
            player_shared->g_pd3dDeviceContext = var->winapi.device_context;
            if (player_shared->init_video(sub->product_video_path.c_str())) {
                player_shared->state.volume.store(0.0f);
                view.video_player = player_shared.get();
                video_players.emplace_back(std::move(player_shared));
                debug_log("    video player initialized");
            } else {
                debug_log("    failed to initialize video player, removing cache file");
                std::error_code remove_ec;
                std::filesystem::remove(sub->product_video_path, remove_ec);
                sub->product_video_path.clear();
            }
        } else if (sub->product_video_path.empty() && !sub->product_video.empty()) {
            debug_log("    video bytes present but no cache path");
        }

        view.image = texture;
        view.owns_image = owns_image;

        products.emplace_back(std::move(view));
        var->gui.subscription_textures.push_back(products.back().image);
    }

    if (var->gui.stage_count >= 2) {
        size_t selected_index = static_cast<size_t>(var->gui.stage_count - 2);
        if (selected_index >= products.size()) {
            var->gui.stage_count = 1;
            var->gui.active_stage = 1;
        }
    }

    products_dirty = false;
}

void c_loader_ui::close() {
    should_close = true;
}

static void(*g_login_callback)(const char*, const char*) = nullptr;
static void(*g_register_callback)(const char*, const char*, const char*) = nullptr;
static void(*g_license_callback)(const char*, const char*) = nullptr;
static void(*g_exit_callback)() = nullptr;
static void (*g_filestream_callback)(const char*) = nullptr;
static void(*g_auth_mode_callback)(bool) = nullptr;

extern "C" {
    LOADER_UI_API c_loader_ui* create_loader_ui() {
        return new c_loader_ui();
    }

    LOADER_UI_API void destroy_loader_ui(c_loader_ui* ui) {
        delete ui;
    }

    LOADER_UI_API bool ui_initialize(c_loader_ui* ui, const char* title) {
        if (!ui) return false;

        ui_config config;
        config.title = title ? title : "Bootstrapper";

        return ui->initialize(config);
    }

    LOADER_UI_API void ui_shutdown(c_loader_ui* ui) {
        if (ui) ui->shutdown();
    }

    LOADER_UI_API bool ui_should_run(c_loader_ui* ui) {
        return ui ? ui->should_run() : false;
    }

    LOADER_UI_API void ui_update(c_loader_ui* ui) {
        if (ui) ui->update();
    }

    LOADER_UI_API void ui_render(c_loader_ui* ui) {
        if (ui) ui->render();
    }

    LOADER_UI_API void ui_set_authenticated(c_loader_ui* ui, bool auth, user_profile* new_profile) {
        if (ui) {
            ui->set_authenticated(auth, new_profile);
        }
    }

    LOADER_UI_API void ui_set_status_message(c_loader_ui* ui, const char* message) {
        if (ui) ui->set_status_message(message ? message : "");
    }

    LOADER_UI_API void ui_set_error_message(c_loader_ui* ui, const char* message) {
        if (ui) ui->set_error_message(message ? message : "");
    }

    LOADER_UI_API void ui_set_loading(c_loader_ui* ui, bool loading) {
        if (ui) ui->set_loading(loading);
    }

    LOADER_UI_API void ui_set_loading_progress(c_loader_ui* ui, float progress) {
        if (ui) ui->set_loading_progress(progress);
    }

    LOADER_UI_API void ui_close(c_loader_ui* ui) {
        if (ui) ui->close();
    }

    LOADER_UI_API void ui_set_local_account(c_loader_ui* ui, const char* username) {
        if (ui && username) {
            ui->set_local_account_username(username);
        }
    }

    LOADER_UI_API void ui_set_license_only_mode(c_loader_ui* ui, bool enabled) {
        if (ui) {
            ui->set_license_only_mode(enabled);
        }
    }

    LOADER_UI_API void ui_set_auth_mode_callback(c_loader_ui* ui, void(*callback)(bool)) {
        if (!ui) return;

        g_auth_mode_callback = callback;

        if (callback) {
            ui->set_auth_mode_callback([](bool enabled) {
                if (g_auth_mode_callback) {
                    g_auth_mode_callback(enabled);
                }
                });
        }
        else {
            ui->set_auth_mode_callback(nullptr);
        }
    }

    LOADER_UI_API void ui_set_login_callback(c_loader_ui* ui, void(*callback)(const char*, const char*)) {
        if (!ui) return;

        g_login_callback = callback;

        if (callback) {
            ui->set_login_callback([](const std::string& username, const std::string& password) {
                if (g_login_callback) {
                    g_login_callback(username.c_str(), password.c_str());
                }
                });
        }
        else {
            ui->set_login_callback(nullptr);
        }
    }

    LOADER_UI_API void ui_set_register_callback(c_loader_ui* ui, void(*callback)(const char*, const char*, const char*)) {
        if (!ui) return;

        g_register_callback = callback;

        if (callback) {
            ui->set_register_callback([](const std::string& username, const std::string& password, const std::string& license) {
                if (g_register_callback) {
                    g_register_callback(username.c_str(), password.c_str(), license.c_str());
                }
                });
        }
        else {
            ui->set_register_callback(nullptr);
        }
    }

    LOADER_UI_API void ui_set_license_callback(c_loader_ui* ui, void(*callback)(const char*, const char*)) {
        if (!ui) return;

        g_license_callback = callback;

        if (callback) {
            ui->set_license_callback([](const std::string& username, const std::string& license) {
                if (g_license_callback) {
                    g_license_callback(username.c_str(), license.c_str());
                }
                });
        }
        else {
            ui->set_license_callback(nullptr);
        }
    }

    LOADER_UI_API void ui_set_exit_callback(c_loader_ui* ui, void(*callback)()) {
        if (!ui) return;

        g_exit_callback = callback;

        if (callback) {
            ui->set_exit_callback([]() {
                if (g_exit_callback) {
                    g_exit_callback();
                }
                });
        }
        else {
            ui->set_exit_callback(nullptr);
        }
    }


    LOADER_UI_API void ui_set_filestream_callback(
        c_loader_ui* ui,
        void(*callback)(const char*)
    ) {
        if (!ui) return;

        g_filestream_callback = callback;

        if (callback) {
            ui->set_filestream_callback([](const std::string& file_id)
                {
                    if (g_filestream_callback) {
                        g_filestream_callback(file_id.c_str());
                    }
                });
        }
        else {
            ui->set_filestream_callback(nullptr);
        }
    }
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &var->winapi.swap_chain, &var->winapi.device_dx11, &featureLevel, &var->winapi.device_context);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &var->winapi.swap_chain, &var->winapi.device_dx11, &featureLevel, &var->winapi.device_context);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (var->winapi.swap_chain) { var->winapi.swap_chain->Release(); var->winapi.swap_chain = nullptr; }
    if (var->winapi.device_context) { var->winapi.device_context->Release(); var->winapi.device_context = nullptr; }
    if (var->winapi.device_dx11) { var->winapi.device_dx11->Release(); var->winapi.device_dx11 = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    var->winapi.swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    var->winapi.device_dx11->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && var->winapi.is_fullscreen)
        {
            var->winapi.is_fullscreen = false;
            SetWindowPos(hWnd, HWND_TOP, var->winapi.restore_rect.left, var->winapi.restore_rect.top, var->winapi.restore_rect.right - var->winapi.restore_rect.left, var->winapi.restore_rect.bottom - var->winapi.restore_rect.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            return 0;
        }
        break;

    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}












