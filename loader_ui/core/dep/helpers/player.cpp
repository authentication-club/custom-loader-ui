#include "../headers/includes.h"
#include <iomanip>
#include <fstream>
#include <mutex>

namespace {
    void player_debug_log(const std::string&) {}
}

int c_video_player::audio_callback(const void* input, void* output, unsigned long frame_count, const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags status_flags, void* user_data)
{
    video_state* state = static_cast<video_state*>(user_data);
    auto* out = static_cast<uint8_t*>(output);
    const size_t bytes_needed = frame_count * state->out_channels * sizeof(int16_t);
    size_t bytes_written = 0;

    if (!state->playing.load() || state->seeking.load())
    {
        memset(output, 0, bytes_needed);
        return paContinue;
    }

    std::unique_lock<std::mutex> lock(state->audio_frame_mutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        memset(output, 0, bytes_needed);
        return paContinue;
    }

    while (!state->audio_frame_queue.empty() && bytes_written < bytes_needed)
    {
        AVFrame* frame = state->audio_frame_queue.front();
        const size_t bytes_available = frame->nb_samples * state->out_channels * sizeof(int16_t);
        const size_t bytes_to_copy = min(bytes_available, bytes_needed - bytes_written);

        memcpy(out + bytes_written, frame->data[0], bytes_to_copy);
        bytes_written += bytes_to_copy;

        const size_t samples_copied = bytes_to_copy / (state->out_channels * sizeof(int16_t));
        if (samples_copied < static_cast<size_t>(frame->nb_samples))
        {
            const size_t bytes_remaining = (frame->nb_samples - samples_copied) * state->out_channels * sizeof(int16_t);
            memmove(frame->data[0], frame->data[0] + bytes_to_copy, bytes_remaining);
            frame->nb_samples -= samples_copied;
        }
        else
        {
            av_frame_free(&frame);
            state->audio_frame_queue.pop_front();
        }
    }

    if (bytes_written < bytes_needed)
        memset(out + bytes_written, 0, bytes_needed - bytes_written);

    float volume = state->volume.load();
    if (volume != 1.0f)
    {
        int16_t* samples = static_cast<int16_t*>(output);
        size_t total_samples = frame_count * state->out_channels;

        for (size_t i = 0; i < total_samples; i++)
        {
            float sample = samples[i] * volume;

            if (sample > 32767.0f) sample = 32767.0f;
            else if (sample < -32768.0f) sample = -32768.0f;
            samples[i] = static_cast<int16_t>(sample);
        }
    }

    return paContinue;
}

void c_video_player::video_decode_thread()
{
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    bool video_reached = false;
    bool audio_reached = false;

    while (!state.quit.load())
    {
        if (!state.playing.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (state.seeking.load())
        {
            video_reached = false;
            audio_reached = false;
        }

        {
            std::unique_lock<std::mutex> lock(state.frame_mutex);
            if (state.frame_queue.size() > 5)
            {
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }

        {
            std::unique_lock<std::mutex> lock(state.audio_frame_mutex);
            if (state.audio_frame_queue.size() > 10)
            {
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }

        int ret;
        {
            std::lock_guard<std::mutex> lock(state.decode_mutex);
            ret = av_read_frame(state.format_ctx, packet);
        }

        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
                state.eof.store(true);
            av_packet_unref(packet);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (packet->stream_index == state.video_stream)
        {
            {
                std::lock_guard<std::mutex> lock(state.decode_mutex);
                ret = avcodec_send_packet(state.video_codec_ctx, packet);
            }
            av_packet_unref(packet);

            if (ret < 0 && ret != AVERROR(EAGAIN))
                continue;

            while (ret >= 0)
            {
                {
                    std::lock_guard<std::mutex> lock(state.decode_mutex);
                    ret = avcodec_receive_frame(state.video_codec_ctx, frame);
                }

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0)
                    break;

                AVFrame* rgba_frame = av_frame_alloc();
                rgba_frame->format = AV_PIX_FMT_RGBA;
                rgba_frame->width = state.width;
                rgba_frame->height = state.height;

                if (av_frame_get_buffer(rgba_frame, 0) < 0)
                {
                    av_frame_free(&rgba_frame);
                    av_frame_unref(frame);
                    break;
                }

                sws_scale(state.sws_ctx, frame->data, frame->linesize, 0, frame->height, rgba_frame->data, rgba_frame->linesize);

                double pts = (frame->pts != AV_NOPTS_VALUE) ? frame->pts * av_q2d(state.format_ctx->streams[state.video_stream]->time_base) : frame->best_effort_timestamp * av_q2d(state.format_ctx->streams[state.video_stream]->time_base);

                if (state.seeking.load())
                {
                    if (pts < state.seek_target.load())
                    {
                        av_frame_free(&rgba_frame);
                        av_frame_unref(frame);
                        continue;
                    }
                    else if (!video_reached)
                    {
                        video_reached = true;
                        state.seek_completed++;
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(state.frame_mutex);
                    state.frame_queue.push_back(rgba_frame);
                }
                state.current_pts.store(pts);
                av_frame_unref(frame);
            }
        }
        else if (packet->stream_index == state.audio_stream)
        {
            {
                std::lock_guard<std::mutex> lock(state.decode_mutex);
                ret = avcodec_send_packet(state.audio_codec_ctx, packet);
            }
            av_packet_unref(packet);

            if (ret < 0 && ret != AVERROR(EAGAIN))
                continue;

            while (ret >= 0)
            {
                AVFrame* audio_frame = av_frame_alloc();
                {
                    std::lock_guard<std::mutex> lock(state.decode_mutex);
                    ret = avcodec_receive_frame(state.audio_codec_ctx, audio_frame);
                }

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    av_frame_free(&audio_frame);
                    break;
                }
                if (ret < 0)
                {
                    av_frame_free(&audio_frame);
                    break;
                }

                AVFrame* converted = av_frame_alloc();
                converted->format = state.out_sample_fmt;
                av_channel_layout_copy(&converted->ch_layout, &state.out_ch_layout);
                converted->sample_rate = state.out_sample_rate;
                converted->nb_samples = audio_frame->nb_samples;

                if (swr_convert_frame(state.swr_ctx, converted, audio_frame) != 0)
                {
                    av_frame_free(&converted);
                    av_frame_free(&audio_frame);
                    continue;
                }

                double pts = (audio_frame->pts != AV_NOPTS_VALUE) ? audio_frame->pts * av_q2d(state.format_ctx->streams[state.audio_stream]->time_base) : state.audio_clock.load() + (audio_frame->nb_samples / static_cast<double>(audio_frame->sample_rate));

                if (state.seeking.load())
                {
                    if (pts < state.seek_target.load())
                    {
                        av_frame_free(&converted);
                        av_frame_free(&audio_frame);
                        continue;
                    }
                    else if (!audio_reached)
                    {
                        audio_reached = true;
                        state.seek_completed++;
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(state.audio_frame_mutex);
                    state.audio_frame_queue.push_back(converted);
                }
                state.audio_clock.store(pts);
                av_frame_free(&audio_frame);
            }
        }
        else
        {
            av_packet_unref(packet);
        }

        if (state.seeking.load()) {
            int required_streams = 1 + (state.audio_stream >= 0 ? 1 : 0);
            if (state.seek_completed.load() >= required_streams)
            {
                state.seeking.store(false);
                state.seek_completed.store(0);
            }
        }
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
}

bool c_video_player::init_video(const char* filename)
{
    av_log_set_level(AV_LOG_ERROR);
    avformat_network_init();

    player_debug_log(std::string("init_video requested for ") + (filename ? filename : "<null>"));

    if (avformat_open_input(&state.format_ctx, filename, nullptr, nullptr) != 0)
    {
        player_debug_log("    avformat_open_input failed");
        return false;
    }
    if (avformat_find_stream_info(state.format_ctx, nullptr) < 0)
    {
        player_debug_log("    avformat_find_stream_info failed");
        return false;
    }

    for (unsigned i = 0; i < state.format_ctx->nb_streams; i++)
    {
        if (state.format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            state.video_stream = i;
            break;
        }
    }

    if (state.video_stream < 0)
    {
        player_debug_log("    no video stream found");
        return false;
    }

    auto* video_codecpar = state.format_ctx->streams[state.video_stream]->codecpar;
    const AVCodec* video_codec = avcodec_find_decoder(video_codecpar->codec_id);
    if (!video_codec)
    {
        player_debug_log("    avcodec_find_decoder for video failed");
        return false;
    }

    state.video_codec_ctx = avcodec_alloc_context3(video_codec);

    if (!state.video_codec_ctx)
    {
        player_debug_log("    avcodec_alloc_context3 for video failed");
        return false;
    }

    if (avcodec_parameters_to_context(state.video_codec_ctx, video_codecpar) < 0)
    {
        player_debug_log("    avcodec_parameters_to_context for video failed");
        return false;
    }

    if (avcodec_open2(state.video_codec_ctx, video_codec, nullptr) < 0)
    {
        player_debug_log("    avcodec_open2 for video failed");
        return false;
    }

    state.width = state.video_codec_ctx->width;
    state.height = state.video_codec_ctx->height;
    state.sws_ctx = sws_getContext(state.width, state.height, state.video_codec_ctx->pix_fmt, state.width, state.height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!state.sws_ctx)
    {
        player_debug_log("    sws_getContext failed");
        return false;
    }

    for (unsigned i = 0; i < state.format_ctx->nb_streams; i++)
    {
        if (state.format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            state.audio_stream = i;
            break;
        }
    }

    if (state.audio_stream >= 0)
    {
        auto* audio_codecpar = state.format_ctx->streams[state.audio_stream]->codecpar;
        const AVCodec* audio_codec = avcodec_find_decoder(audio_codecpar->codec_id);
        if (audio_codec)
        {
            state.audio_codec_ctx = avcodec_alloc_context3(audio_codec);
            if (state.audio_codec_ctx)
            {
                if (avcodec_parameters_to_context(state.audio_codec_ctx, audio_codecpar) >= 0)
                {
                    if (avcodec_open2(state.audio_codec_ctx, audio_codec, nullptr) >= 0)
                    {
                        av_channel_layout_default(&state.out_ch_layout, 2);
                        state.out_channels = state.out_ch_layout.nb_channels;

                        int ret = swr_alloc_set_opts2(&state.swr_ctx, &state.out_ch_layout, state.out_sample_fmt, state.out_sample_rate, &state.audio_codec_ctx->ch_layout, state.audio_codec_ctx->sample_fmt, state.audio_codec_ctx->sample_rate, 0, nullptr);

                        if (ret < 0 || !state.swr_ctx)
                        {
                            swr_free(&state.swr_ctx);
                            state.audio_stream = -1;
                            player_debug_log("    swr_alloc_set_opts2 failed, disabling audio");
                        }
                        else
                        {
                            swr_init(state.swr_ctx);
                            player_debug_log("    audio stream configured successfully");
                        }
                    }
                    else
                    {
                        avcodec_free_context(&state.audio_codec_ctx);
                        state.audio_stream = -1;
                        player_debug_log("    avcodec_open2 for audio failed");
                    }
                }
                else
                {
                    avcodec_free_context(&state.audio_codec_ctx);
                    state.audio_stream = -1;
                    player_debug_log("    avcodec_parameters_to_context for audio failed");
                }
            }
            else
            {
                player_debug_log("    avcodec_alloc_context3 for audio failed");
            }
        }
        else
        {
            player_debug_log("    avcodec_find_decoder for audio failed");
        }
    }

    if (state.audio_stream >= 0)
    {
        PaError err = Pa_Initialize();
        if (err != paNoError)
        {
            state.audio_stream = -1;
            player_debug_log("    Pa_Initialize failed");
        }
        else
        {
            PaStreamParameters outputParams;
            outputParams.device = Pa_GetDefaultOutputDevice();
            outputParams.channelCount = state.out_channels;
            outputParams.sampleFormat = paInt16;
            outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
            outputParams.hostApiSpecificStreamInfo = nullptr;

            err = Pa_OpenStream(&state.audio_stream_pa, nullptr, &outputParams, state.out_sample_rate, 1024, paClipOff, audio_callback, &state);

            if (err != paNoError)
            {
                Pa_Terminate();
                state.audio_stream = -1;
                player_debug_log("    Pa_OpenStream failed");
            }
            else
            {
                Pa_StartStream(state.audio_stream_pa);
                player_debug_log("    audio playback stream started");
            }
        }
    }

    player_debug_log(std::string("    initialization complete width=") + std::to_string(state.width) +
                     " height=" + std::to_string(state.height));
    state.duration = state.format_ctx->duration / static_cast<double>(AV_TIME_BASE);
    state.frame_rate = av_q2d(state.format_ctx->streams[state.video_stream]->avg_frame_rate);

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = state.width;
    desc.Height = state.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, nullptr, &state.texture)))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    ZeroMemory(&srv_desc, sizeof(srv_desc));
    srv_desc.Format = desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    if (FAILED(g_pd3dDevice->CreateShaderResourceView(state.texture, &srv_desc, &state.texture_srv)))
    {
        state.texture->Release();
        state.texture = nullptr;
        return false;
    }

    state.playing.store(true);
    decode_thread = std::thread(&c_video_player::video_decode_thread, this);
    return true;
}

void c_video_player::cleanup_video()
{
    state.quit.store(true);
    if (decode_thread.joinable())
        decode_thread.join();

    if (state.format_ctx)
        avformat_close_input(&state.format_ctx);

    if (state.video_codec_ctx)
        avcodec_free_context(&state.video_codec_ctx);

    if (state.sws_ctx)
        sws_freeContext(state.sws_ctx);

    if (state.texture_srv)
    {
        state.texture_srv->Release();
        state.texture_srv = nullptr;
    }

    if (state.texture)
    {
        state.texture->Release();
        state.texture = nullptr;
    }

    std::lock_guard<std::mutex> lock(state.frame_mutex);
    while (!state.frame_queue.empty())
    {
        av_frame_free(&state.frame_queue.front());
        state.frame_queue.pop_front();
    }
}

void c_video_player::seek_video(double pos)
{
    if (!state.format_ctx) return;
    pos = ImClamp(pos, 0.0, state.duration);
    state.seeking.store(true);
    state.seek_target.store(pos);
    state.eof.store(false);
    state.seek_completed.store(0);

    {
        std::lock_guard<std::mutex> lock(state.frame_mutex);
        while (!state.frame_queue.empty())
        {
            av_frame_free(&state.frame_queue.front());
            state.frame_queue.pop_front();
        }
    }

    {
        std::lock_guard<std::mutex> lock(state.audio_frame_mutex);
        while (!state.audio_frame_queue.empty())
        {
            av_frame_free(&state.audio_frame_queue.front());
            state.audio_frame_queue.pop_front();
        }
    }

    {
        std::lock_guard<std::mutex> lock(state.decode_mutex);
        AVStream* video_stream = state.format_ctx->streams[state.video_stream];
        int64_t ts = static_cast<int64_t>(pos / av_q2d(video_stream->time_base));
        av_seek_frame(state.format_ctx, state.video_stream, ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(state.video_codec_ctx);

        if (state.audio_stream >= 0)
        {
            avcodec_flush_buffers(state.audio_codec_ctx);
        }
    }

    state.current_pts.store(pos);
    state.audio_clock.store(pos);
}

std::string format_duration(float duration)
{
    int total = static_cast<int>(duration);
    int m = total / 60;
    int s = total % 60;
    std::ostringstream oss;
    oss << m << ":" << std::setw(2) << std::setfill('0') << s;
    return oss.str();
}

template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
bool slider_behavior_t(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, TYPE* v, const TYPE v_min, const TYPE v_max, const char* format, ImGuiSliderFlags flags, ImRect* out_grab_bb)
{
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImGuiAxis axis = (flags & ImGuiSliderFlags_Vertical) ? ImGuiAxis_Y : ImGuiAxis_X;
    const bool is_logarithmic = (flags & ImGuiSliderFlags_Logarithmic) != 0;
    const bool is_floating_point = (data_type == ImGuiDataType_Float) || (data_type == ImGuiDataType_Double);
    const float v_range_f = (float)(v_min < v_max ? v_max - v_min : v_min - v_max);

    const float grab_padding = SCALE(2.0f);
    const float slider_sz = (bb.Max[axis] - bb.Min[axis]) - grab_padding * 2.0f;
    float grab_sz = 13.f;

    const float slider_usable_sz = slider_sz - grab_sz;
    const float slider_usable_pos_min = bb.Min[axis] + grab_padding + grab_sz * 0.5f;
    const float slider_usable_pos_max = bb.Max[axis] - grab_padding - grab_sz * 0.5f;

    float logarithmic_zero_epsilon = 0.0f;
    float zero_deadzone_halfsize = 0.0f;
    if (is_logarithmic)
    {
        const int decimal_precision = is_floating_point ? ImParseFormatPrecision(format, 3) : 1;
        logarithmic_zero_epsilon = ImPow(0.1f, (float)decimal_precision);
        zero_deadzone_halfsize = (style.LogSliderDeadzone * 0.5f) / ImMax(slider_usable_sz, 1.0f);
    }

    bool value_changed = false;
    if (g.ActiveId == id)
    {
        bool set_new_value = false;
        float clicked_t = 0.0f;
        if (g.ActiveIdSource == ImGuiInputSource_Mouse)
        {
            if (!g.IO.MouseDown[0])
            {
                ClearActiveID();
            }
            else
            {
                const float mouse_abs_pos = g.IO.MousePos[axis];
                if (g.ActiveIdIsJustActivated)
                {
                    float grab_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, *v, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
                    if (axis == ImGuiAxis_Y)
                        grab_t = 1.0f - grab_t;
                    const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
                    const bool clicked_around_grab = (mouse_abs_pos >= grab_pos - grab_sz * 0.5f - 1.0f) && (mouse_abs_pos <= grab_pos + grab_sz * 0.5f + 1.0f);
                    g.SliderGrabClickOffset = (clicked_around_grab && is_floating_point) ? mouse_abs_pos - grab_pos : 0.0f;
                }
                if (slider_usable_sz > 0.0f)
                    clicked_t = ImSaturate((mouse_abs_pos - g.SliderGrabClickOffset - slider_usable_pos_min) / slider_usable_sz);
                if (axis == ImGuiAxis_Y)
                    clicked_t = 1.0f - clicked_t;
                set_new_value = true;
            }
        }
        else if (g.ActiveIdSource == ImGuiInputSource_Keyboard || g.ActiveIdSource == ImGuiInputSource_Gamepad)
        {
            if (g.ActiveIdIsJustActivated)
            {
                g.SliderCurrentAccum = 0.0f;
                g.SliderCurrentAccumDirty = false;
            }

            float input_delta = (axis == ImGuiAxis_X) ? GetNavTweakPressedAmount(axis) : -GetNavTweakPressedAmount(axis);
            if (input_delta != 0.0f)
            {
                const bool tweak_slow = IsKeyDown((g.NavInputSource == ImGuiInputSource_Gamepad) ? ImGuiKey_NavGamepadTweakSlow : ImGuiKey_NavKeyboardTweakSlow);
                const bool tweak_fast = IsKeyDown((g.NavInputSource == ImGuiInputSource_Gamepad) ? ImGuiKey_NavGamepadTweakFast : ImGuiKey_NavKeyboardTweakFast);
                const int decimal_precision = is_floating_point ? ImParseFormatPrecision(format, 3) : 0;
                if (decimal_precision > 0)
                {
                    input_delta /= 100.0f;
                    if (tweak_slow)
                        input_delta /= 10.0f;
                }
                else
                {
                    if ((v_range_f >= -100.0f && v_range_f <= 100.0f && v_range_f != 0.0f) || tweak_slow)
                        input_delta = ((input_delta < 0.0f) ? -1.0f : +1.0f) / v_range_f;
                    else
                        input_delta /= 100.0f;
                }
                if (tweak_fast)
                    input_delta *= 10.0f;

                g.SliderCurrentAccum += input_delta;
                g.SliderCurrentAccumDirty = true;
            }

            float delta = g.SliderCurrentAccum;
            if (g.NavActivatePressedId == id && !g.ActiveIdIsJustActivated)
            {
                ClearActiveID();
            }
            else if (g.SliderCurrentAccumDirty)
            {
                clicked_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, *v, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

                if ((clicked_t >= 1.0f && delta > 0.0f) || (clicked_t <= 0.0f && delta < 0.0f))
                {
                    set_new_value = false;
                    g.SliderCurrentAccum = 0.0f;
                }
                else
                {
                    set_new_value = true;
                    float old_clicked_t = clicked_t;
                    clicked_t = ImSaturate(clicked_t + delta);

                    TYPE v_new = ScaleValueFromRatioT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
                    if (is_floating_point && !(flags & ImGuiSliderFlags_NoRoundToFormat))
                        v_new = RoundScalarWithFormatT<TYPE>(format, data_type, v_new);
                    float new_clicked_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, v_new, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

                    if (delta > 0)
                        g.SliderCurrentAccum -= ImMin(new_clicked_t - old_clicked_t, delta);
                    else
                        g.SliderCurrentAccum -= ImMax(new_clicked_t - old_clicked_t, delta);
                }

                g.SliderCurrentAccumDirty = false;
            }
        }

        if (set_new_value)
            if ((g.LastItemData.InFlags & ImGuiItemFlags_ReadOnly) || (flags & ImGuiSliderFlags_ReadOnly))
                set_new_value = false;

        if (set_new_value)
        {
            TYPE v_new = ScaleValueFromRatioT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

            if (is_floating_point && !(flags & ImGuiSliderFlags_NoRoundToFormat))
                v_new = RoundScalarWithFormatT<TYPE>(format, data_type, v_new);

            if (*v != v_new)
            {
                *v = v_new;
                value_changed = true;
            }
        }
    }

    if (slider_sz < 1.0f)
    {
        *out_grab_bb = ImRect(bb.Min, bb.Min);
    }
    else
    {
        float grab_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, *v, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        if (axis == ImGuiAxis_Y)
            grab_t = 1.0f - grab_t;
        const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
        if (axis == ImGuiAxis_X)
            *out_grab_bb = ImRect(grab_pos - grab_sz * 0.5f, bb.Min.y + grab_padding, grab_pos + grab_sz * 0.5f, bb.Max.y - grab_padding);
        else
            *out_grab_bb = ImRect(bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f, bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f);
    }

    return value_changed;
}

bool slider_behavior(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, void* p_v, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags, ImRect* out_grab_bb)
{
    IM_ASSERT((flags == 1 || (flags & ImGuiSliderFlags_InvalidMask_) == 0) && "Invalid ImGuiSliderFlags flags! Has the legacy 'float power' argument been mistakenly cast to flags? Call function with ImGuiSliderFlags_Logarithmic flags instead.");
    IM_ASSERT((flags & ImGuiSliderFlags_WrapAround) == 0);

    switch (data_type)
    {
    case ImGuiDataType_S8: { ImS32 v32 = (ImS32) * (ImS8*)p_v;  bool r = slider_behavior_t<ImS32, ImS32, float>(bb, id, ImGuiDataType_S32, &v32, *(const ImS8*)p_min, *(const ImS8*)p_max, format, flags, out_grab_bb); if (r) *(ImS8*)p_v = (ImS8)v32;  return r; }
    case ImGuiDataType_U8: { ImU32 v32 = (ImU32) * (ImU8*)p_v;  bool r = slider_behavior_t<ImU32, ImS32, float>(bb, id, ImGuiDataType_U32, &v32, *(const ImU8*)p_min, *(const ImU8*)p_max, format, flags, out_grab_bb); if (r) *(ImU8*)p_v = (ImU8)v32;  return r; }
    case ImGuiDataType_S16: { ImS32 v32 = (ImS32) * (ImS16*)p_v; bool r = slider_behavior_t<ImS32, ImS32, float>(bb, id, ImGuiDataType_S32, &v32, *(const ImS16*)p_min, *(const ImS16*)p_max, format, flags, out_grab_bb); if (r) *(ImS16*)p_v = (ImS16)v32; return r; }
    case ImGuiDataType_U16: { ImU32 v32 = (ImU32) * (ImU16*)p_v; bool r = slider_behavior_t<ImU32, ImS32, float>(bb, id, ImGuiDataType_U32, &v32, *(const ImU16*)p_min, *(const ImU16*)p_max, format, flags, out_grab_bb); if (r) *(ImU16*)p_v = (ImU16)v32; return r; }
    case ImGuiDataType_S32:
        IM_ASSERT(*(const ImS32*)p_min >= INT_MIN / 2 && *(const ImS32*)p_max <= INT_MAX / 2);
        return slider_behavior_t<ImS32, ImS32, float >(bb, id, data_type, (ImS32*)p_v, *(const ImS32*)p_min, *(const ImS32*)p_max, format, flags, out_grab_bb);
    case ImGuiDataType_U32:
        IM_ASSERT(*(const ImU32*)p_max <= UINT_MAX / 2);
        return slider_behavior_t<ImU32, ImS32, float >(bb, id, data_type, (ImU32*)p_v, *(const ImU32*)p_min, *(const ImU32*)p_max, format, flags, out_grab_bb);
    case ImGuiDataType_S64:
        IM_ASSERT(*(const ImS64*)p_min >= LLONG_MIN / 2 && *(const ImS64*)p_max <= LLONG_MAX / 2);
        return slider_behavior_t<ImS64, ImS64, double>(bb, id, data_type, (ImS64*)p_v, *(const ImS64*)p_min, *(const ImS64*)p_max, format, flags, out_grab_bb);
    case ImGuiDataType_U64:
        IM_ASSERT(*(const ImU64*)p_max <= ULLONG_MAX / 2);
        return slider_behavior_t<ImU64, ImS64, double>(bb, id, data_type, (ImU64*)p_v, *(const ImU64*)p_min, *(const ImU64*)p_max, format, flags, out_grab_bb);
    case ImGuiDataType_Float:
        IM_ASSERT(*(const float*)p_min >= -FLT_MAX / 2.0f && *(const float*)p_max <= FLT_MAX / 2.0f);
        return slider_behavior_t<float, float, float >(bb, id, data_type, (float*)p_v, *(const float*)p_min, *(const float*)p_max, format, flags, out_grab_bb);
    case ImGuiDataType_Double:
        IM_ASSERT(*(const double*)p_min >= -DBL_MAX / 2.0f && *(const double*)p_max <= DBL_MAX / 2.0f);
        return slider_behavior_t<double, double, double>(bb, id, data_type, (double*)p_v, *(const double*)p_min, *(const double*)p_max, format, flags, out_grab_bb);
    case ImGuiDataType_COUNT: break;
    }
    IM_ASSERT(0);
    return false;
}

bool slider_scalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImRect rect{ gui->get_window()->DC.CursorPos, gui->get_window()->DC.CursorPos + ImVec2(gui->content_avail().x, SCALE(elements->player.slider_height)) };
    const ImRect slider_rect{ rect.Min - ImVec2(elements->player.slider_height, 0), rect.Max + ImVec2(elements->player.slider_height, 0) };

    const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
    gui->item_size(rect);
    if (!gui->item_add(rect, id))
        return false;

    const bool hovered = ItemHoverable(rect, id, g.LastItemData.InFlags);
    bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        // Tabbing or CTRL-clicking on Slider turns it into an input box
        const bool clicked = hovered && IsMouseClicked(0, ImGuiInputFlags_None, id);
        const bool make_active = (clicked || g.NavActivateId == id);
        if (make_active && clicked)
            SetKeyOwner(ImGuiKey_MouseLeft, id);
        if (make_active && temp_input_allowed)
            if ((clicked && g.IO.KeyCtrl) || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput)))
                temp_input_is_active = true;

        if (make_active && !temp_input_is_active)
        {
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }
    }

    ImRect grab_bb;
    const bool value_changed = slider_behavior(slider_rect, id, data_type, p_data, p_min, p_max, format, flags, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);

    //gui->set_next_window_pos(rect.Min);
    //gui->begin_def_child(std::string(label) + "blur", rect.GetSize(), 0, window_flags_always_use_window_padding | window_flags_no_move | window_flags_nav_flattened | window_flags_no_saved_settings | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse | window_flags_no_background);
    //{
    //    draw->push_clip_rect(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), true);
    //    draw_background_blur(gui->window_drawlist(), var->winapi.device_dx11, var->winapi.device_context, rect.GetHeight() / 2);
    //    draw->pop_clip_rect(gui->window_drawlist());
    //}
    //gui->end_def_child();

    draw->rect_filled(window->DrawList, rect.Min + SCALE(0, elements->player.line_padding), rect.Max - SCALE(0, elements->player.line_padding), draw->get_clr(clr->main.text, 0.24f), rect.GetHeight() / 2);

    draw->rect_filled(window->DrawList, rect.Min + SCALE(0, elements->player.line_padding), ImVec2(grab_bb.GetCenter().x, rect.Max.y - SCALE(elements->player.line_padding)), draw->get_clr(clr->main.accent), rect.GetHeight() / 2);
    draw->circle_filled(window->DrawList, ImVec2(grab_bb.GetCenter().x, rect.GetCenter().y), SCALE(elements->player.slider_height / 2), draw->get_clr(clr->main.text), 30);

    //gui->set_screen_pos(rect.Min, pos_all);
    //gui->dummy(rect.GetSize());

    return value_changed;
}

bool slider_float(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return slider_scalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
}

bool pause_button(bool callback)
{
    ImGuiWindow* window = gui->get_window();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID("pause button");

    const ImVec2 pos = window->DC.CursorPos;

    const ImRect total(pos, pos + SCALE(elements->player.buttons_size));

    gui->item_size(total, style.FramePadding.y);
    if (!gui->item_add(total, id))
        return false;

    bool pressed = total.Contains(g.IO.MousePos) && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered(NULL);

    if (callback)
        draw->text_clipped(gui->window_drawlist(), font->get(icons_data, 10), total.Min, total.Max, draw->get_clr(clr->main.text), "U", NULL, NULL, ImVec2(0.5f, 0.5f));

    else
        draw->text_clipped(gui->window_drawlist(), font->get(icons_data, 10), total.Min, total.Max, draw->get_clr(clr->main.text), "E", NULL, NULL, ImVec2(0.5f, 0.5f));

    return pressed;
}

bool fullscreen_button()
{
    ImGuiWindow* window = gui->get_window();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID("fullscreen button");

    const ImVec2 pos = window->DC.CursorPos;

    const ImRect total(pos, pos + SCALE(elements->player.buttons_size));

    gui->item_size(total, style.FramePadding.y);
    if (!gui->item_add(total, id))
        return false;

    bool pressed = total.Contains(g.IO.MousePos) && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered(NULL);

    draw->text_clipped(gui->window_drawlist(), font->get(icons_data, 12), total.Min, total.Max, draw->get_clr(clr->main.text, 0.48), "X", NULL, NULL, ImVec2(0.5f, 0.5f));

    return pressed;
}

template <typename T>
bool volume_slider_ex(std::string_view name, T* callback, T min, T max, std::string_view format)
{
    struct volume_state
    {
        float offset{ 0 };
        float size{ 0 };
        float alpha{ 0 };
        bool hovered, held, pressed;
    };

    ImGuiWindow* window = gui->get_window();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(name.data());

    volume_state* state = gui->anim_container<volume_state>(id);

    const ImVec2 pos = window->DC.CursorPos;
    ImRect total(pos, pos + SCALE(elements->player.buttons_size.x + state->size, elements->player.buttons_size.y));
    const ImRect rect{ ImVec2(total.Min.x + SCALE(elements->player.volume_padding + elements->player.buttons_size.x), total.GetCenter().y - SCALE(elements->player.slider_height / 2 - elements->player.line_padding)), ImVec2(total.Max.x, total.GetCenter().y + SCALE(elements->player.slider_height / 2 - elements->player.line_padding)) };

    gui->item_size(total, style.FramePadding.y);
    if (!gui->item_add(total, id))
        return false;

    bool hovered, held;
    bool pressed = gui->button_behavior(total, id, &hovered, &held);

    const float padding = SCALE(0);
    const float grab_width = SCALE(0);
    const float width_with_grab = rect.GetWidth() - grab_width - padding * 2;
    const float grab_offset = padding + grab_width / 2 + static_cast<float>((*callback - min)) / static_cast<float>(max - min) * width_with_grab;

    char value_buf[64]; const char* value_buf_end = gui->get_fmt(value_buf, callback, format);
    const float t = ImSaturate((g.IO.MousePos.x - (rect.Min.x + padding + grab_width / 2)) / width_with_grab);

    if (held || state->held)
    {
        *callback += static_cast<T>(((min + t * (max - min)) - *callback));
        *callback = ImClamp(*callback, min, max);
    }

    gui->easing(state->size, hovered || held || state->hovered || state->held ? elements->player.volume_width + elements->player.volume_padding : 0, 16.f, dynamic_easing);
    gui->easing(state->offset, grab_offset, 20.f, dynamic_easing);
    gui->easing(state->alpha, hovered || held || state->hovered || state->held ? 1.f : 0.f, 16.f, dynamic_easing);

        //gui->push_var(style_var_alpha, state->alpha);
        //gui->set_next_window_pos(total.Min);
        //gui->begin_def_child(std::string(name) + "blur", ImVec2(rect.GetSize().x, SCALE(6)), 0, window_flags_always_use_window_padding | window_flags_no_move | window_flags_nav_flattened | window_flags_no_saved_settings | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse | window_flags_no_background);
        //{
        //    state->pressed = gui->button_behavior(ImRect{ gui->window_pos(), gui->window_pos() + gui->window_size() }, gui->get_window()->GetID((std::string(name) + "blur").data()), &state->hovered, &state->held);
        //    draw_background_blur(gui->window_drawlist(), var->winapi.device_dx11, var->winapi.device_context, rect.GetHeight() / 2);

        //}
        //gui->end_def_child();
        //gui->pop_var();

        draw->rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->main.text, 0.24 * state->alpha), rect.GetHeight() / 2);

        draw->rect_filled(window->DrawList, rect.Min, ImVec2(rect.Min.x + state->offset, rect.Max.y), draw->get_clr(clr->main.accent, state->alpha), state->offset < 10 ? 0 : rect.GetHeight() / 2);


    draw->text_clipped(window->DrawList, font->get(icons_data, 12), total.Min, total.Max, draw->get_clr(clr->main.text, 0.48), "Y", NULL, NULL, ImVec2(0.f, 0.5f));

    //gui->set_screen_pos(total.Min, pos_all);
    //gui->dummy(total.GetSize());

    return held;
}

bool volume_slider(std::string_view name, float* callback, float min, float max, std::string_view format)
{
    return volume_slider_ex(name, callback, min, max, format);
}

void c_video_player::render(std::string_view id, const ImVec2& size, std::string_view name)
{
    struct player_state
    {
        float offset[2]{ 0, 0 };
        float volume_pos{ 0 };
        float control_alpha{ 0 };
        bool video_hovered{ false };
        bool init{ false };
    };

    ImGuiWindow* window = gui->get_window();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID idp = window->GetID(id.data());

    player_state* anim = gui->anim_container<player_state>(idp);

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total(pos, pos + SCALE(elements->player.size));

    gui->item_size(total, style.FramePadding.y);
    if (!gui->item_add(total, idp))
        return;

    if (!anim->init)
    {
        anim->init = true;
    }

    AVFrame* currentFrame = nullptr;
    {
        std::lock_guard<std::mutex> lock(state.frame_mutex);
        if (!state.frame_queue.empty())
        {
            currentFrame = state.frame_queue.front();
            state.frame_queue.pop_front();
        }
    }

    if (currentFrame)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (g_pd3dDeviceContext->Map(state.texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped) == S_OK)
        {
            uint8_t* dst_data = static_cast<uint8_t*>(mapped.pData);
            const uint8_t* src_data = currentFrame->data[0];
            const int dst_pitch = mapped.RowPitch;
            const int src_pitch = currentFrame->linesize[0];
            const int min_pitch = min(src_pitch, dst_pitch);

            for (int y = 0; y < state.height; ++y)
            {
                memcpy(dst_data, src_data, min_pitch);
                dst_data += dst_pitch;
                src_data += src_pitch;
            }
            g_pd3dDeviceContext->Unmap(state.texture, 0);
            texture_initialized = true;
        }
        av_frame_free(&currentFrame);
    }

    if (!var->winapi.is_fullscreen)
    {
        gui->push_var(style_var_popup_border_size, 0);
        gui->push_var(style_var_window_border_size, 0);
        gui->set_next_window_pos(total.Min, pos_all);
        gui->begin_content("video_player", SCALE(elements->player.size), SCALE(elements->window.padding.x, 0), SCALE(0, 0), window_flags_no_scrollbar | window_flags_no_scroll_with_mouse);
        {
            anim->video_hovered = gui->is_window_hovered(NULL);

            if (texture_initialized && state.texture_srv)
            {
                if (var->winapi.is_fullscreen)
                    draw->image_rounded(gui->window_drawlist(), (void*)state.texture_srv, gui->window_pos(), gui->window_pos() + gui->window_size(), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr({ 1.f, 1.f, 1.f, 1.f }));
                else
                    draw->image_rounded(gui->window_drawlist(), (void*)state.texture_srv, gui->window_pos(), gui->window_pos() + gui->window_size(), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr({ 1.f, 1.f, 1.f, 1.f }), SCALE(elements->widgets.rounding));
            }

            draw->rect(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->main.text, 0.12), SCALE(elements->widgets.rounding));
            gui->easing(anim->control_alpha, (ImRect{ gui->window_pos(), gui->window_pos() + gui->window_size() }.Contains(g.IO.MousePos)) || roundf(state.duration) == roundf(state.current_pts.load()) || !state.playing.load() ? 1.f : 0.f, 8.f, static_easing);
            gui->easing(anim->offset[0], (ImRect{ gui->window_pos(), gui->window_pos() + gui->window_size() }.Contains(g.IO.MousePos)) ? SCALE(elements->player.control_height + elements->window.padding.y) : 0.f, 12.f, dynamic_easing);
            gui->easing(anim->offset[1], (ImRect{ gui->window_pos(), gui->window_pos() + gui->window_size() }.Contains(g.IO.MousePos)) ? SCALE(elements->player.logo_height_1 + elements->player.logo_height_1 + elements->window.padding.y + elements->player.volume_padding) : 0.f, 12.f, dynamic_easing);

            std::string full = format_duration(state.duration);
            std::string current = format_duration(state.current_pts.load());

            gui->push_var(style_var_alpha, anim->control_alpha);
            gui->set_pos(ImVec2(SCALE(elements->window.padding.x), -SCALE(elements->player.logo_height_1 + elements->player.logo_height_1 + elements->player.volume_padding) +  anim->offset[1]), pos_all);
            gui->begin_content("top_zone", ImVec2(gui->content_avail().x, SCALE(elements->player.logo_height_1 + elements->player.logo_height_1 + elements->player.volume_padding)), SCALE(0, 0), SCALE(0, 0), window_flags_no_scrollbar | window_flags_no_scroll_with_mouse);
            {
                draw->text_clipped(gui->window_drawlist(), font->get(suisse_intl_medium_data, 18), gui->window_pos(), gui->window_pos() + ImVec2(gui->window_size().x, SCALE(elements->player.logo_height_1)), draw->get_clr(clr->main.text), name.data(), NULL, NULL, ImVec2(0.f, 0.5f));
                draw->text_clipped(gui->window_drawlist(), font->get(icons_data, 12), gui->window_pos() + SCALE(0, elements->player.logo_height_1 + elements->player.volume_padding), gui->window_pos() + SCALE(elements->player.buttons_size.x, elements->player.logo_height_1 + elements->player.logo_height_2 + elements->player.volume_padding), draw->get_clr(clr->main.text, 0.48), "S", NULL, NULL, ImVec2(0.5f, 0.5f));
                draw->text_clipped(gui->window_drawlist(), font->get(suisse_intl_medium_data, 16), gui->window_pos() + SCALE(elements->player.buttons_size.x + elements->player.volume_padding, elements->player.logo_height_1 + elements->player.volume_padding - 2), gui->window_pos() + ImVec2(gui->window_size().x, SCALE(elements->player.logo_height_1 + elements->player.logo_height_2 + elements->player.volume_padding)), draw->get_clr(clr->main.text, 0.72), "stanley murasakie", NULL, NULL, ImVec2(0.f, 0.5f));
            }
            gui->end_content();

            gui->set_pos(gui->window_height() - anim->offset[0], pos_y);
            gui->begin_content("bottom_zone", ImVec2(gui->content_avail().x, SCALE(elements->player.control_height)), SCALE(0, 1), SCALE(elements->player.volume_padding, elements->player.zone_padding), window_flags_no_scrollbar | window_flags_no_scroll_with_mouse);
            {
                if (pause_button(state.playing.load()))
                    state.playing.store(!state.playing.load());

                gui->sameline();
                gui->push_font(font->get(suisse_intl_medium_data, 16));
                gui->set_pos(gui->get_pos().y - SCALE(3.5), pos_y);
                PushStyleColor(ImGuiCol_Text, draw->get_clr(clr->main.text));
                Text((std::stringstream{} << current << " / " << full).str().data());
                PopStyleColor();
                gui->pop_font();

                gui->sameline();

                gui->set_pos(gui->content_max().x - SCALE(elements->player.volume_padding + elements->player.buttons_size.x) - anim->volume_pos, pos_x);

                float volume = state.volume.load();
                if (volume_slider("volume", &volume, 0.f, 1.f, "%.1f"))
                {
                    state.volume.store(volume);
                    state.playing.store(true);
                }

                gui->easing(anim->volume_pos, GetCurrentContext()->LastItemData.Rect.GetWidth(), 32.f, dynamic_easing);

                gui->sameline();

                gui->set_pos(gui->content_max().x - SCALE(elements->player.buttons_size.x), pos_x);
                if (fullscreen_button())
                {
                    var->winapi.is_fullscreen = !var->winapi.is_fullscreen;

                    if (var->winapi.is_fullscreen)
                    {
                        GetWindowRect(var->winapi.hwnd, &var->winapi.restore_rect);

                        int screen_width = GetSystemMetrics(SM_CXSCREEN);
                        int screen_height = GetSystemMetrics(SM_CYSCREEN);

                        SetWindowPos(var->winapi.hwnd, HWND_TOP, 0, 0, screen_width, screen_height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    }
                    else
                        SetWindowPos(var->winapi.hwnd, HWND_TOP, var->winapi.restore_rect.left, var->winapi.restore_rect.top, var->winapi.restore_rect.right - var->winapi.restore_rect.left, var->winapi.restore_rect.bottom - var->winapi.restore_rect.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                }

                float pos = static_cast<float>(state.current_pts.load());
                if (slider_float("video pos", &pos, 0.0f, static_cast<float>(state.duration), "%.1f", 0))
                {
                    seek_video(static_cast<double>(pos));
                    state.playing.store(true);
                }
            }
            gui->end_content();
            gui->pop_var();

        }
        gui->end_content();
        gui->pop_var(2);
    }
    else
    {
        gui->push_var(style_var_popup_border_size, 0);
        gui->push_var(style_var_window_border_size, 0);
        gui->set_next_window_pos(ImVec2(0, 0), pos_all);
        gui->set_next_window_size(GetIO().DisplaySize);
        gui->begin("video_player", nullptr, window_flags_no_decoration | window_flags_tooltip | window_flags_no_background);
        {
            gui->set_window_focus();
            anim->video_hovered = gui->is_window_hovered(NULL);

            if (texture_initialized && state.texture_srv)
            {
                if (var->winapi.is_fullscreen)
                    draw->image_rounded(gui->window_drawlist(), (void*)state.texture_srv, gui->window_pos(), gui->window_pos() + gui->window_size(), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr({ 1.f, 1.f, 1.f, 1.f }));
                else
                    draw->image_rounded(gui->window_drawlist(), (void*)state.texture_srv, gui->window_pos(), gui->window_pos() + gui->window_size(), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr({ 1.f, 1.f, 1.f, 1.f }), SCALE(elements->widgets.rounding));
            }

            draw->rect(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->main.text, 0.12), SCALE(elements->widgets.rounding));
            gui->easing(anim->control_alpha, (ImRect{ gui->window_pos(), gui->window_pos() + gui->window_size() }.Contains(g.IO.MousePos)) || roundf(state.duration) == roundf(state.current_pts.load()) || !state.playing.load() ? 1.f : 0.f, 8.f, static_easing);
            gui->easing(anim->offset[0], (ImRect{ gui->window_pos(), gui->window_pos() + gui->window_size() }.Contains(g.IO.MousePos)) ? SCALE(elements->player.control_height + elements->window.padding.y) : 0.f, 12.f, dynamic_easing);
            gui->easing(anim->offset[1], (ImRect{ gui->window_pos(), gui->window_pos() + gui->window_size() }.Contains(g.IO.MousePos)) ? SCALE(elements->player.logo_height_1 + elements->player.logo_height_1 + elements->window.padding.y + elements->player.volume_padding) : 0.f, 12.f, dynamic_easing);

            std::string full = format_duration(state.duration);
            std::string current = format_duration(state.current_pts.load());

            gui->push_var(style_var_alpha, anim->control_alpha);
            gui->set_pos(gui->window_height() - anim->offset[0], pos_y);
            gui->begin_content("bottom_zone", ImVec2(gui->content_avail().x, SCALE(elements->player.control_height)), SCALE(elements->window.padding.x, 1), SCALE(elements->player.volume_padding, elements->player.zone_padding), window_flags_no_scrollbar | window_flags_no_scroll_with_mouse);
            {
                if (pause_button(state.playing.load()))
                    state.playing.store(!state.playing.load());

                gui->sameline();
                gui->push_font(font->get(suisse_intl_medium_data, 16));
                gui->set_pos(gui->get_pos().y - SCALE(3.5), pos_y);
                PushStyleColor(ImGuiCol_Text, draw->get_clr(clr->main.text));
                Text((std::stringstream{} << current << " / " << full).str().data());
                PopStyleColor();
                gui->pop_font();

                gui->sameline();

                gui->set_pos(gui->content_max().x - SCALE(elements->player.volume_padding + elements->player.buttons_size.x) - anim->volume_pos, pos_x);

                float volume = state.volume.load();
                if (volume_slider("volume", &volume, 0.f, 1.f, "%.1f"))
                {
                    state.volume.store(volume);
                    state.playing.store(true);
                }

                gui->easing(anim->volume_pos, GetCurrentContext()->LastItemData.Rect.GetWidth(), 32.f, dynamic_easing);

                gui->sameline();

                gui->set_pos(gui->content_max().x - SCALE(elements->player.buttons_size.x), pos_x);
                if (fullscreen_button())
                {
                    var->winapi.is_fullscreen = !var->winapi.is_fullscreen;

                    if (var->winapi.is_fullscreen)
                    {
                        GetWindowRect(var->winapi.hwnd, &var->winapi.restore_rect);

                        int screen_width = GetSystemMetrics(SM_CXSCREEN);
                        int screen_height = GetSystemMetrics(SM_CYSCREEN);

                        SetWindowPos(var->winapi.hwnd, HWND_TOP, 0, 0, screen_width, screen_height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    }
                    else
                        SetWindowPos(var->winapi.hwnd, HWND_TOP, var->winapi.restore_rect.left, var->winapi.restore_rect.top, var->winapi.restore_rect.right - var->winapi.restore_rect.left, var->winapi.restore_rect.bottom - var->winapi.restore_rect.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                }

                float pos = static_cast<float>(state.current_pts.load());
                if (slider_float("video pos", &pos, 0.0f, static_cast<float>(state.duration), "%.1f", 0))
                {
                    seek_video(static_cast<double>(pos));
                    state.playing.store(true);
                }
            }
            gui->end_content();
            gui->pop_var();
        }
        gui->end();
        gui->pop_var(2);

    }
}
