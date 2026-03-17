#include "baseband_replay_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/color.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <utility>

using namespace godot;

void BasebandReplay::_bind_methods() {
    ClassDB::bind_method(D_METHOD("push_baseband_frame", "frame"), &BasebandReplay::push_baseband_frame);
    ClassDB::bind_method(D_METHOD("get_current_sample_rate_hz"), &BasebandReplay::get_current_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("is_replay_playing"), &BasebandReplay::is_replay_playing);
    ClassDB::bind_method(D_METHOD("is_record_enabled"), &BasebandReplay::is_record_enabled);
    ClassDB::bind_method(D_METHOD("is_passthrough_enabled"), &BasebandReplay::is_passthrough_enabled);
    ClassDB::bind_method(D_METHOD("set_upstream_tuned_frequency_hz", "frequency_hz"), &BasebandReplay::set_upstream_tuned_frequency_hz);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &BasebandReplay::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &BasebandReplay::set_port_value);

    ClassDB::bind_method(D_METHOD("_on_center_frequency_changed", "value"), &BasebandReplay::_on_center_frequency_changed);
    ClassDB::bind_method(D_METHOD("_on_sample_rate_changed", "value"), &BasebandReplay::_on_sample_rate_changed);
    ClassDB::bind_method(D_METHOD("_on_buffer_seconds_changed", "value"), &BasebandReplay::_on_buffer_seconds_changed);
    ClassDB::bind_method(D_METHOD("_on_record_toggled", "enabled"), &BasebandReplay::_on_record_toggled);
    ClassDB::bind_method(D_METHOD("_on_play_toggled", "enabled"), &BasebandReplay::_on_play_toggled);
    ClassDB::bind_method(D_METHOD("_on_passthrough_toggled", "enabled"), &BasebandReplay::_on_passthrough_toggled);
    ClassDB::bind_method(D_METHOD("_on_playback_slider_changed", "value"), &BasebandReplay::_on_playback_slider_changed);
    ClassDB::bind_method(D_METHOD("_on_clear_pressed"), &BasebandReplay::_on_clear_pressed);
    ClassDB::bind_method(D_METHOD("_on_save_pressed"), &BasebandReplay::_on_save_pressed);

    ADD_SIGNAL(MethodInfo("baseband_frame", PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "frame")));
    ADD_SIGNAL(MethodInfo("sample_rate_changed", PropertyInfo(Variant::INT, "sample_rate_hz")));
    ADD_SIGNAL(MethodInfo("frequency_tuned_changed", PropertyInfo(Variant::INT, "frequency_hz")));
    ADD_SIGNAL(MethodInfo("passthrough_frequency_request", PropertyInfo(Variant::INT, "frequency_hz")));
}

void BasebandReplay::_notification(int32_t p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            on_ready();
            break;
        case NOTIFICATION_PROCESS:
            process_tick();
            break;
        case NOTIFICATION_EXIT_TREE:
            on_exit_tree();
            break;
        default:
            break;
    }
}

void BasebandReplay::on_ready() {
    ensure_ui();
    cache_ui_refs();
    bind_ui();

    set_center_frequency_hz(center_frequency_hz, false);
    set_sample_rate_hz(sample_rate_hz, false);
    set_buffer_seconds(buffer_seconds);
    set_record_enabled(record_enabled);
    set_play_enabled(false);
    set_passthrough_enabled(passthrough_enabled);
    refresh_playback_slider_range();
    clear_spectrogram_texture();

    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }

    set_process(true);
}

void BasebandReplay::on_exit_tree() {
    stop_spectrogram_worker();
    play_enabled = false;
    playback_pending_samples = 0.0;
    playback_offset = 0;
    last_live_frame = PackedVector2Array();
    spectrogram_pixels_rgba.clear();
    spectrogram_work_image.unref();
    spectrogram_width = 0;
    spectrogram_height = 0;
    spectrogram_uploaded_columns = 0;
    spectrogram_ready_columns.store(0, std::memory_order_release);
}

void BasebandReplay::process_tick() {
    rebuild_spectrogram_if_needed();
    pump_spectrogram_render();

    if (!play_enabled || Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    if (ring_stored_samples == 0 || sample_rate_hz <= 0) {
        set_play_enabled(false);
        return;
    }

    const double delta = get_process_delta_time();
    if (delta <= 0.0) {
        return;
    }

    playback_pending_samples += static_cast<double>(sample_rate_hz) * delta;
    int32_t safety_counter = 0;
    while (playback_pending_samples >= 1.0 && play_enabled && safety_counter < 6) {
        const int32_t samples_requested = static_cast<int32_t>(std::floor(playback_pending_samples));
        const int32_t remaining = static_cast<int32_t>(ring_stored_samples - playback_offset);
        const int32_t chunk = std::max(0, std::min({samples_requested, playback_chunk_size, remaining}));
        if (chunk <= 0) {
            set_play_enabled(false);
            break;
        }

        PackedVector2Array out_frame;
        out_frame.resize(chunk);
        Vector2 *out_ptr = out_frame.ptrw();
        for (int32_t i = 0; i < chunk; i++) {
            const std::size_t ordered_index = playback_offset + static_cast<std::size_t>(i);
            out_ptr[i] = ring_samples[ordered_to_ring_index(ordered_index)];
        }

        emit_signal("baseband_frame", out_frame);
        set_playback_offset(playback_offset + static_cast<std::size_t>(chunk));
        playback_pending_samples -= static_cast<double>(chunk);
        safety_counter++;

        if (playback_offset >= ring_stored_samples) {
            set_play_enabled(false);
        }
    }

    update_status_labels();
}

void BasebandReplay::set_center_frequency_hz(int64_t p_frequency_hz, bool p_emit_signal) {
    const int64_t clamped = std::clamp<int64_t>(p_frequency_hz, -999000000000LL, 999000000000LL);
    center_frequency_hz = clamped;
    if (center_frequency_selector != nullptr) {
        center_frequency_selector->set_value_no_signal(center_frequency_hz);
    }
    if (p_emit_signal) {
        emit_signal("frequency_tuned_changed", center_frequency_hz);
        if (passthrough_enabled && !play_enabled) {
            emit_signal("passthrough_frequency_request", center_frequency_hz);
        }
    }
}

void BasebandReplay::set_sample_rate_hz(int64_t p_sample_rate_hz, bool p_emit_signal) {
    sample_rate_hz = std::clamp<int64_t>(p_sample_rate_hz, 1000LL, 999000000000LL);
    if (sample_rate_spin != nullptr) {
        sample_rate_spin->set_value_no_signal(static_cast<double>(sample_rate_hz));
    }

    const std::size_t new_capacity = static_cast<std::size_t>(std::llround(buffer_seconds * static_cast<double>(sample_rate_hz)));
    resize_ring_preserving_latest(std::max<std::size_t>(1, new_capacity));
    spectrogram_dirty = true;

    if (p_emit_signal) {
        emit_signal("sample_rate_changed", sample_rate_hz);
    }
}

void BasebandReplay::set_buffer_seconds(double p_seconds) {
    buffer_seconds = std::clamp(p_seconds, 0.1, 3600.0);
    if (buffer_seconds_spin != nullptr) {
        buffer_seconds_spin->set_value_no_signal(buffer_seconds);
    }

    const std::size_t new_capacity = static_cast<std::size_t>(std::llround(buffer_seconds * static_cast<double>(sample_rate_hz)));
    resize_ring_preserving_latest(std::max<std::size_t>(1, new_capacity));
    spectrogram_dirty = true;
}

void BasebandReplay::set_record_enabled(bool p_enabled) {
    const bool was_recording = record_enabled;
    record_enabled = p_enabled;
    if (record_button != nullptr) {
        record_button->set_pressed_no_signal(record_enabled);
    }
    if (was_recording && !record_enabled) {
        spectrogram_dirty = true;
    }
    update_status_labels();
}

void BasebandReplay::set_play_enabled(bool p_enabled) {
    const bool was_play_enabled = play_enabled;
    play_enabled = p_enabled && ring_stored_samples > 0;
    if (play_enabled) {
        if (playback_offset >= ring_stored_samples) {
            playback_offset = 0;
        }
        playback_pending_samples = 0.0;
        if (record_enabled) {
            set_record_enabled(false);
        }
    } else {
        playback_pending_samples = 0.0;
    }
    if (play_button != nullptr) {
        play_button->set_pressed_no_signal(play_enabled);
    }
    if (was_play_enabled && !play_enabled && passthrough_enabled) {
        if (!last_live_frame.is_empty()) {
            emit_signal("baseband_frame", last_live_frame);
        }
        emit_signal("passthrough_frequency_request", center_frequency_hz);
    }
    update_playback_scrubber();
    update_status_labels();
}

void BasebandReplay::set_passthrough_enabled(bool p_enabled) {
    passthrough_enabled = p_enabled;
    if (passthrough_button != nullptr) {
        passthrough_button->set_pressed_no_signal(passthrough_enabled);
    }
    if (passthrough_enabled && !play_enabled) {
        emit_signal("passthrough_frequency_request", center_frequency_hz);
    }
    update_status_labels();
}

void BasebandReplay::set_playback_offset(std::size_t p_offset, bool p_from_slider) {
    if (ring_stored_samples == 0) {
        playback_offset = 0;
    } else {
        playback_offset = std::min<std::size_t>(p_offset, ring_stored_samples - 1);
    }
    if (p_from_slider) {
        playback_pending_samples = 0.0;
    }
    update_playback_scrubber();
    update_status_labels();
}

void BasebandReplay::refresh_playback_slider_range() {
    if (playback_slider == nullptr) {
        return;
    }
    playback_slider->set_min(0.0);
    const double max_value = ring_stored_samples > 0 ? static_cast<double>(ring_stored_samples - 1) : 0.0;
    playback_slider->set_max(max_value);
    playback_slider->set_step(1.0);
    playback_slider->set_editable(ring_stored_samples > 0);
    playback_slider->set_value_no_signal(static_cast<double>(std::min<std::size_t>(playback_offset, ring_stored_samples > 0 ? ring_stored_samples - 1 : 0)));
}

void BasebandReplay::update_playback_scrubber() {
    if (spectrogram_scrubber == nullptr || spectrogram_texture_rect == nullptr) {
        return;
    }
    const float width = std::max(1.0f, spectrogram_texture_rect->get_size().x);
    float ratio = 0.0f;
    if (ring_stored_samples > 1) {
        ratio = static_cast<float>(playback_offset) / static_cast<float>(ring_stored_samples - 1);
    }
    const float x = std::clamp(ratio * width, 0.0f, width);
    spectrogram_scrubber->set_position(Vector2(x, 0.0f));
    spectrogram_scrubber->set_size(Vector2(2.0f, std::max(1.0f, spectrogram_texture_rect->get_size().y)));
    spectrogram_scrubber->set_visible(ring_stored_samples > 0);
}

void BasebandReplay::clear_spectrogram_texture() {
    stop_spectrogram_worker();

    if (spectrogram_texture_rect == nullptr) {
        return;
    }

    Ref<Image> image = Image::create(2, 2, false, Image::FORMAT_RGBA8);
    image->fill(Color(0.03, 0.03, 0.05, 1.0));
    spectrogram_work_image = image;
    spectrogram_width = 0;
    spectrogram_height = 0;
    spectrogram_uploaded_columns = 0;
    spectrogram_ready_columns.store(0, std::memory_order_release);
    spectrogram_pixels_rgba.clear();

    if (spectrogram_texture.is_null()) {
        spectrogram_texture.instantiate();
    }
    spectrogram_texture->set_image(image);
    spectrogram_texture_rect->set_texture(spectrogram_texture);
    update_playback_scrubber();
}

Color BasebandReplay::spectrogram_color_from_norm(float p_norm) {
    const float t = std::clamp(p_norm, 0.0f, 1.0f);
    if (t < 0.3f) {
        const float lt = t / 0.3f;
        return Color(0.02f + 0.1f * lt, 0.03f + 0.15f * lt, 0.08f + 0.6f * lt, 1.0f);
    }
    if (t < 0.7f) {
        const float lt = (t - 0.3f) / 0.4f;
        return Color(0.12f + 0.6f * lt, 0.18f + 0.55f * lt, 0.68f - 0.45f * lt, 1.0f);
    }
    const float lt = (t - 0.7f) / 0.3f;
    return Color(0.72f + 0.28f * lt, 0.73f + 0.27f * lt, 0.23f - 0.23f * lt, 1.0f);
}

void BasebandReplay::rebuild_spectrogram_if_needed() {
    if (!spectrogram_dirty) {
        return;
    }
    spectrogram_dirty = false;
    start_spectrogram_build_async();
}

void BasebandReplay::stop_spectrogram_worker() {
    spectrogram_cancel_requested.store(true, std::memory_order_release);
    if (spectrogram_worker_thread.joinable()) {
        spectrogram_worker_thread.join();
    }
    spectrogram_worker_running.store(false, std::memory_order_release);
    spectrogram_cancel_requested.store(false, std::memory_order_release);
}

void BasebandReplay::start_spectrogram_build_async() {
    if (spectrogram_texture_rect == nullptr) {
        return;
    }

    stop_spectrogram_worker();

    const int32_t view_width = static_cast<int32_t>(std::round(std::max(64.0, static_cast<double>(spectrogram_texture_rect->get_size().x))));
    const int32_t view_height = static_cast<int32_t>(std::round(std::max(56.0, static_cast<double>(spectrogram_texture_rect->get_size().y))));
    spectrogram_width = view_width;
    spectrogram_height = view_height;
    spectrogram_uploaded_columns = 0;
    spectrogram_ready_columns.store(0, std::memory_order_release);
    spectrogram_pixels_rgba.assign(static_cast<std::size_t>(view_width) * static_cast<std::size_t>(view_height) * 4U, 0U);

    for (int32_t y = 0; y < view_height; y++) {
        for (int32_t x = 0; x < view_width; x++) {
            const std::size_t pixel_index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(view_width) + static_cast<std::size_t>(x)) * 4U;
            spectrogram_pixels_rgba[pixel_index + 0] = static_cast<uint8_t>(std::round(0.03 * 255.0));
            spectrogram_pixels_rgba[pixel_index + 1] = static_cast<uint8_t>(std::round(0.03 * 255.0));
            spectrogram_pixels_rgba[pixel_index + 2] = static_cast<uint8_t>(std::round(0.05 * 255.0));
            spectrogram_pixels_rgba[pixel_index + 3] = 255U;
        }
    }

    if (spectrogram_work_image.is_null() ||
            spectrogram_work_image->get_width() != view_width ||
            spectrogram_work_image->get_height() != view_height) {
        spectrogram_work_image = Image::create(view_width, view_height, false, Image::FORMAT_RGBA8);
    }
    spectrogram_work_image->fill(Color(0.03, 0.03, 0.05, 1.0));
    if (spectrogram_texture.is_null()) {
        spectrogram_texture.instantiate();
    }
    spectrogram_texture->set_image(spectrogram_work_image);
    spectrogram_texture_rect->set_texture(spectrogram_texture);
    update_playback_scrubber();

    if (ring_stored_samples < 64) {
        spectrogram_ready_columns.store(view_width, std::memory_order_release);
        return;
    }

    const PackedVector2Array ordered = export_ordered_capture();
    const int32_t total_samples = ordered.size();
    if (total_samples < 64) {
        spectrogram_ready_columns.store(view_width, std::memory_order_release);
        return;
    }

    std::vector<Vector2> ordered_samples;
    ordered_samples.resize(static_cast<std::size_t>(total_samples));
    {
        const Vector2 *src = ordered.ptr();
        std::copy(src, src + total_samples, ordered_samples.begin());
    }

    spectrogram_worker_running.store(true, std::memory_order_release);
    spectrogram_cancel_requested.store(false, std::memory_order_release);

    spectrogram_worker_thread = std::thread([this, samples = std::move(ordered_samples), total_samples, view_width, view_height]() {
        int32_t fft_size = 512;
        while (fft_size > total_samples && fft_size > 64) {
            fft_size >>= 1;
        }
        fft_size = std::clamp(fft_size, 64, 2048);
        const int32_t half_fft = fft_size / 2;
        constexpr float PI_F = 3.14159265358979323846f;
        constexpr float MIN_DB = -110.0f;
        constexpr float MAX_DB = -20.0f;
        const float db_range = MAX_DB - MIN_DB;

        std::vector<float> real(static_cast<std::size_t>(fft_size), 0.0f);
        std::vector<float> imag(static_cast<std::size_t>(fft_size), 0.0f);
        std::vector<float> window(static_cast<std::size_t>(fft_size), 1.0f);
        for (int32_t i = 0; i < fft_size; i++) {
            window[static_cast<std::size_t>(i)] = 0.5f - 0.5f * std::cos(2.0f * PI_F * static_cast<float>(i) / static_cast<float>(fft_size - 1));
        }

        auto fft_in_place = [&](std::vector<float> &re, std::vector<float> &im) {
            int32_t j = 0;
            for (int32_t i = 1; i < fft_size; i++) {
                int32_t bit = fft_size >> 1;
                while ((j & bit) != 0 && bit > 0) {
                    j ^= bit;
                    bit >>= 1;
                }
                j ^= bit;
                if (i < j) {
                    std::swap(re[static_cast<std::size_t>(i)], re[static_cast<std::size_t>(j)]);
                    std::swap(im[static_cast<std::size_t>(i)], im[static_cast<std::size_t>(j)]);
                }
            }

            for (int32_t len = 2; len <= fft_size; len <<= 1) {
                const float angle = -2.0f * PI_F / static_cast<float>(len);
                const float wlen_cos = std::cos(angle);
                const float wlen_sin = std::sin(angle);
                const int32_t half = len >> 1;
                for (int32_t base = 0; base < fft_size; base += len) {
                    float w_cos = 1.0f;
                    float w_sin = 0.0f;
                    for (int32_t k = 0; k < half; k++) {
                        const int32_t i0 = base + k;
                        const int32_t i1 = i0 + half;
                        const float u_r = re[static_cast<std::size_t>(i0)];
                        const float u_i = im[static_cast<std::size_t>(i0)];
                        const float v_r = re[static_cast<std::size_t>(i1)] * w_cos - im[static_cast<std::size_t>(i1)] * w_sin;
                        const float v_i = re[static_cast<std::size_t>(i1)] * w_sin + im[static_cast<std::size_t>(i1)] * w_cos;
                        re[static_cast<std::size_t>(i0)] = u_r + v_r;
                        im[static_cast<std::size_t>(i0)] = u_i + v_i;
                        re[static_cast<std::size_t>(i1)] = u_r - v_r;
                        im[static_cast<std::size_t>(i1)] = u_i - v_i;
                        const float next_cos = w_cos * wlen_cos - w_sin * wlen_sin;
                        w_sin = w_cos * wlen_sin + w_sin * wlen_cos;
                        w_cos = next_cos;
                    }
                }
            }
        };

        constexpr int32_t UPDATE_CHUNK = 8;
        int32_t last_ready = 0;
        for (int32_t x = 0; x < view_width; x++) {
            if (spectrogram_cancel_requested.load(std::memory_order_acquire)) {
                break;
            }

            const float col_t = view_width > 1 ? static_cast<float>(x) / static_cast<float>(view_width - 1) : 0.0f;
            const int32_t center_index = std::clamp(static_cast<int32_t>(std::round(col_t * static_cast<float>(total_samples - 1))), 0, total_samples - 1);
            const int32_t start_index = std::clamp(center_index - half_fft, 0, std::max(0, total_samples - fft_size));

            for (int32_t i = 0; i < fft_size; i++) {
                const int32_t sample_index = start_index + i;
                const Vector2 iq = samples[static_cast<std::size_t>(sample_index)];
                const float win = window[static_cast<std::size_t>(i)];
                real[static_cast<std::size_t>(i)] = iq.x * win;
                imag[static_cast<std::size_t>(i)] = iq.y * win;
            }

            fft_in_place(real, imag);

            for (int32_t y = 0; y < view_height; y++) {
                const float y_t = view_height > 1 ? static_cast<float>(y) / static_cast<float>(view_height - 1) : 0.0f;
                const int32_t shifted_bin = std::clamp(static_cast<int32_t>(std::round((1.0f - y_t) * static_cast<float>(fft_size - 1))), 0, fft_size - 1);
                const int32_t src_bin = (shifted_bin + half_fft) % fft_size;
                const float re_v = real[static_cast<std::size_t>(src_bin)];
                const float im_v = imag[static_cast<std::size_t>(src_bin)];
                const float mag = std::sqrt(re_v * re_v + im_v * im_v) / static_cast<float>(fft_size);
                const float db = 20.0f * std::log10(std::max(mag, 1e-12f));
                const float norm = std::clamp((db - MIN_DB) / std::max(db_range, 1.0f), 0.0f, 1.0f);
                const Color color = spectrogram_color_from_norm(norm);
                const std::size_t pixel_index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(view_width) + static_cast<std::size_t>(x)) * 4U;
                spectrogram_pixels_rgba[pixel_index + 0] = static_cast<uint8_t>(std::round(std::clamp(color.r, 0.0f, 1.0f) * 255.0f));
                spectrogram_pixels_rgba[pixel_index + 1] = static_cast<uint8_t>(std::round(std::clamp(color.g, 0.0f, 1.0f) * 255.0f));
                spectrogram_pixels_rgba[pixel_index + 2] = static_cast<uint8_t>(std::round(std::clamp(color.b, 0.0f, 1.0f) * 255.0f));
                spectrogram_pixels_rgba[pixel_index + 3] = 255U;
            }

            if (((x + 1) - last_ready) >= UPDATE_CHUNK || x == (view_width - 1)) {
                last_ready = x + 1;
                spectrogram_ready_columns.store(last_ready, std::memory_order_release);
            }
        }

        spectrogram_worker_running.store(false, std::memory_order_release);
    });
}

void BasebandReplay::pump_spectrogram_render() {
    if (spectrogram_texture_rect == nullptr || spectrogram_work_image.is_null()) {
        return;
    }
    if (spectrogram_width <= 0 || spectrogram_height <= 0 || spectrogram_pixels_rgba.empty()) {
        return;
    }

    const int32_t ready_columns = std::clamp(spectrogram_ready_columns.load(std::memory_order_acquire), 0, spectrogram_width);
    if (ready_columns <= spectrogram_uploaded_columns) {
        return;
    }

    const int32_t start_x = spectrogram_uploaded_columns;
    const int32_t end_x = ready_columns;
    for (int32_t x = start_x; x < end_x; x++) {
        for (int32_t y = 0; y < spectrogram_height; y++) {
            const std::size_t pixel_index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(spectrogram_width) + static_cast<std::size_t>(x)) * 4U;
            const Color color(
                    static_cast<float>(spectrogram_pixels_rgba[pixel_index + 0]) / 255.0f,
                    static_cast<float>(spectrogram_pixels_rgba[pixel_index + 1]) / 255.0f,
                    static_cast<float>(spectrogram_pixels_rgba[pixel_index + 2]) / 255.0f,
                    1.0f);
            spectrogram_work_image->set_pixel(x, y, color);
        }
    }

    spectrogram_uploaded_columns = end_x;
    if (spectrogram_texture.is_null()) {
        spectrogram_texture.instantiate();
        spectrogram_texture->set_image(spectrogram_work_image);
        spectrogram_texture_rect->set_texture(spectrogram_texture);
    } else {
        spectrogram_texture->update(spectrogram_work_image);
    }
}

void BasebandReplay::resize_ring_preserving_latest(std::size_t p_new_capacity) {
    const std::size_t new_capacity = std::max<std::size_t>(1, p_new_capacity);
    if (new_capacity == ring_capacity_samples) {
        update_status_labels();
        return;
    }

    const PackedVector2Array ordered = export_ordered_capture();
    const std::size_t keep_count = std::min<std::size_t>(new_capacity, static_cast<std::size_t>(ordered.size()));

    ring_samples.clear();
    ring_samples.resize(new_capacity);
    ring_capacity_samples = new_capacity;
    ring_write_index = 0;
    ring_stored_samples = 0;

    if (keep_count > 0) {
        const int64_t start = static_cast<int64_t>(ordered.size()) - static_cast<int64_t>(keep_count);
        const Vector2 *src = ordered.ptr();
        for (std::size_t i = 0; i < keep_count; i++) {
            append_ring_sample(src[start + static_cast<int64_t>(i)]);
        }
    }

    if (play_enabled && playback_offset >= ring_stored_samples) {
        set_play_enabled(false);
    }
    if (!play_enabled && playback_offset >= ring_stored_samples) {
        playback_offset = ring_stored_samples > 0 ? ring_stored_samples - 1 : 0;
    }
    refresh_playback_slider_range();
    update_playback_scrubber();
    update_status_labels();
}

void BasebandReplay::append_ring_sample(const Vector2 &p_sample) {
    if (ring_capacity_samples == 0) {
        return;
    }
    ring_samples[ring_write_index] = p_sample;
    ring_write_index = (ring_write_index + 1) % ring_capacity_samples;
    ring_stored_samples = std::min<std::size_t>(ring_stored_samples + 1, ring_capacity_samples);
}

void BasebandReplay::append_ring_frame(const PackedVector2Array &p_frame) {
    const Vector2 *src = p_frame.ptr();
    for (int32_t i = 0; i < p_frame.size(); i++) {
        append_ring_sample(src[i]);
    }
}

std::size_t BasebandReplay::ordered_to_ring_index(std::size_t p_ordered_index) const {
    if (ring_capacity_samples == 0 || ring_stored_samples == 0) {
        return 0;
    }
    const std::size_t oldest = (ring_write_index + ring_capacity_samples - ring_stored_samples) % ring_capacity_samples;
    return (oldest + p_ordered_index) % ring_capacity_samples;
}

PackedVector2Array BasebandReplay::export_ordered_capture() const {
    PackedVector2Array out;
    out.resize(static_cast<int32_t>(ring_stored_samples));
    Vector2 *out_ptr = out.ptrw();
    for (std::size_t i = 0; i < ring_stored_samples; i++) {
        out_ptr[static_cast<int32_t>(i)] = ring_samples[ordered_to_ring_index(i)];
    }
    return out;
}

String BasebandReplay::format_bytes(double p_bytes) {
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = std::max(0.0, p_bytes);
    int32_t unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }
    return String::num(value, unit == 0 ? 0 : 2) + " " + units[unit];
}

void BasebandReplay::update_status_labels() {
    if (status_label != nullptr) {
        String mode = "IDLE";
        if (play_enabled) {
            mode = "PLAY";
        } else if (record_enabled) {
            mode = "REC";
        }

        const double used_seconds = sample_rate_hz > 0 ? static_cast<double>(ring_stored_samples) / static_cast<double>(sample_rate_hz) : 0.0;
        const String passthrough_text = passthrough_enabled ? "ON" : "OFF";
        status_label->set_text(String("State: ") + mode + "  PT: " + passthrough_text + "  Used: " + String::num(used_seconds, 2) + " s");
    }

    if (memory_label != nullptr) {
        const double used_bytes = static_cast<double>(ring_stored_samples) * 8.0;
        const double capacity_bytes = static_cast<double>(ring_capacity_samples) * 8.0;
        memory_label->set_text(String("Memory: ") + format_bytes(used_bytes) + " / " + format_bytes(capacity_bytes));
    }

    refresh_playback_slider_range();
    update_playback_scrubber();
}

void BasebandReplay::save_capture_to_path(const String &p_path) {
    const PackedVector2Array ordered = export_ordered_capture();
    if (ordered.is_empty()) {
        set_problem_state(true, "No baseband samples captured.");
        update_status_labels();
        return;
    }

    const String path = p_path.strip_edges();
    if (path.is_empty()) {
        set_problem_state(true, "Save path is empty.");
        update_status_labels();
        return;
    }

    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
    if (file.is_null()) {
        set_problem_state(true, String("Failed to open save path: ") + path);
        update_status_labels();
        return;
    }

    file->store_32(0x45534452); // ESDR
    file->store_32(0x42423130); // BB10
    file->store_64(sample_rate_hz);
    file->store_64(center_frequency_hz);
    file->store_64(static_cast<int64_t>(ordered.size()));

    const Vector2 *src = ordered.ptr();
    for (int32_t i = 0; i < ordered.size(); i++) {
        file->store_float(src[i].x);
        file->store_float(src[i].y);
    }
    file->flush();

    set_problem_state(false, String());
    if (status_label != nullptr) {
        status_label->set_text(String("Saved: ") + String::num_int64(ordered.size()) + " samples");
    }
    update_status_labels();
}

void BasebandReplay::_on_center_frequency_changed(int64_t p_value) {
    set_center_frequency_hz(p_value, true);
}

void BasebandReplay::_on_sample_rate_changed(double p_value) {
    set_sample_rate_hz(static_cast<int64_t>(std::llround(p_value)), true);
}

void BasebandReplay::_on_buffer_seconds_changed(double p_value) {
    set_buffer_seconds(p_value);
}

void BasebandReplay::_on_record_toggled(bool p_enabled) {
    set_record_enabled(p_enabled);
}

void BasebandReplay::_on_play_toggled(bool p_enabled) {
    set_play_enabled(p_enabled);
}

void BasebandReplay::_on_passthrough_toggled(bool p_enabled) {
    set_passthrough_enabled(p_enabled);
}

void BasebandReplay::_on_playback_slider_changed(double p_value) {
    if (ring_stored_samples == 0) {
        set_playback_offset(0, true);
        return;
    }
    const std::size_t target = static_cast<std::size_t>(std::llround(std::max(0.0, p_value)));
    set_playback_offset(target, true);
}

void BasebandReplay::_on_clear_pressed() {
    ring_write_index = 0;
    ring_stored_samples = 0;
    playback_offset = 0;
    playback_pending_samples = 0.0;
    spectrogram_dirty = true;
    set_play_enabled(false);
    clear_spectrogram_texture();
    refresh_playback_slider_range();
    update_status_labels();
}

void BasebandReplay::_on_save_pressed() {
    if (save_path_edit == nullptr) {
        return;
    }
    save_capture_to_path(save_path_edit->get_text());
}

void BasebandReplay::push_baseband_frame(const PackedVector2Array &p_frame) {
    if (p_frame.is_empty()) {
        return;
    }

    last_live_frame = p_frame;

    if (record_enabled) {
        append_ring_frame(p_frame);
    }

    if (!play_enabled && passthrough_enabled) {
        emit_signal("baseband_frame", p_frame);
    }

    update_status_labels();
}

int64_t BasebandReplay::get_current_sample_rate_hz() const {
    return sample_rate_hz;
}

bool BasebandReplay::is_replay_playing() const {
    return play_enabled;
}

bool BasebandReplay::is_record_enabled() const {
    return record_enabled;
}

bool BasebandReplay::is_passthrough_enabled() const {
    return passthrough_enabled;
}

void BasebandReplay::set_upstream_tuned_frequency_hz(int64_t p_frequency_hz) {
    if (!passthrough_enabled || play_enabled) {
        return;
    }
    const int64_t clamped = std::clamp<int64_t>(p_frequency_hz, -999000000000LL, 999000000000LL);
    center_frequency_hz = clamped;
    if (center_frequency_selector != nullptr) {
        center_frequency_selector->set_value_no_signal(center_frequency_hz);
    }
    emit_signal("frequency_tuned_changed", center_frequency_hz);
}

Variant BasebandReplay::get_port_value(int64_t p_port) const {
    switch (p_port) {
        case 0:
            return buffer_seconds;
        case 1:
            return center_frequency_hz;
        case 2:
            return sample_rate_hz;
        case 4:
            return record_enabled;
        case 5:
            return play_enabled;
        case 6:
            return passthrough_enabled;
        default:
            return Variant();
    }
}

void BasebandReplay::set_port_value(int64_t p_port, const Variant &p_value) {
    switch (p_port) {
        case 0:
            set_buffer_seconds(static_cast<double>(p_value));
            break;
        case 1:
            set_center_frequency_hz(static_cast<int64_t>(p_value), true);
            break;
        case 2:
            set_sample_rate_hz(static_cast<int64_t>(p_value), true);
            break;
        case 3:
            if (p_value.get_type() == Variant::PACKED_VECTOR2_ARRAY) {
                push_baseband_frame(p_value);
            }
            break;
        case 4:
            set_record_enabled(static_cast<bool>(p_value));
            break;
        case 5:
            set_play_enabled(static_cast<bool>(p_value));
            break;
        case 6:
            set_passthrough_enabled(static_cast<bool>(p_value));
            break;
        default:
            break;
    }
}

void BasebandReplay::ensure_ui() {
    if (has_node(NodePath("BufferRow/BufferPanel/BufferSecondsSpin")) &&
            has_node(NodePath("PlayRow/PlayButton")) &&
            has_node(NodePath("PassthroughRow/PassthroughButton")) &&
            has_node(NodePath("SpectrogramPanel/SpectrogramOverlay/SpectrogramTexture")) &&
            has_node(NodePath("TimelineRow/PlaybackSlider")) &&
            has_node(NodePath("SaveRow/SavePathPanel/SavePath"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("Baseband Replay");
    set_custom_minimum_size(Vector2(420, 320));
    set_resizable(true);

    HBoxContainer *buffer_row = memnew(HBoxContainer);
    buffer_row->set_name("BufferRow");
    add_child(buffer_row);

    Label *buffer_label = memnew(Label);
    buffer_label->set_name("BufferLabel");
    buffer_label->set_text("Buffer Seconds");
    buffer_row->add_child(buffer_label);

    PanelContainer *buffer_panel = memnew(PanelContainer);
    buffer_panel->set_name("BufferPanel");
    buffer_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    buffer_row->add_child(buffer_panel);

    SpinBox *buffer = memnew(SpinBox);
    buffer->set_name("BufferSecondsSpin");
    buffer->set_min(0.1);
    buffer->set_max(3600.0);
    buffer->set_step(0.1);
    buffer->set_value(buffer_seconds);
    buffer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    buffer_panel->add_child(buffer);

    HBoxContainer *center_row = memnew(HBoxContainer);
    center_row->set_name("CenterFrequencyRow");
    add_child(center_row);

    Label *center_label = memnew(Label);
    center_label->set_name("CenterFrequencyLabel");
    center_label->set_text("Center");
    center_row->add_child(center_label);

    PanelContainer *center_panel = memnew(PanelContainer);
    center_panel->set_name("CenterFrequencyPanel");
    center_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    center_row->add_child(center_panel);

    DigitNumberSelector *center_selector = memnew(DigitNumberSelector);
    center_selector->set_name("CenterFrequencySelector");
    center_selector->set_limits(-999000000000LL, 999000000000LL);
    center_selector->set_digit_count(12);
    center_selector->set_group_size(3);
    center_selector->set_show_group_labels(true);
    center_selector->set_show_separators(true);
    center_selector->set_suffix_text("");
    PackedStringArray labels;
    labels.push_back("GHz");
    labels.push_back("MHz");
    labels.push_back("kHz");
    labels.push_back("Hz");
    center_selector->set_group_labels(labels);
    center_selector->set_value_no_signal(center_frequency_hz);
    center_panel->add_child(center_selector);

    HBoxContainer *sample_rate_row = memnew(HBoxContainer);
    sample_rate_row->set_name("SampleRateRow");
    add_child(sample_rate_row);

    Label *sample_rate_label = memnew(Label);
    sample_rate_label->set_name("SampleRateLabel");
    sample_rate_label->set_text("Sample Rate");
    sample_rate_row->add_child(sample_rate_label);

    PanelContainer *sample_rate_panel = memnew(PanelContainer);
    sample_rate_panel->set_name("SampleRatePanel");
    sample_rate_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    sample_rate_row->add_child(sample_rate_panel);

    SpinBox *sample_rate = memnew(SpinBox);
    sample_rate->set_name("SampleRateSpin");
    sample_rate->set_min(1000.0);
    sample_rate->set_max(999000000000.0);
    sample_rate->set_step(1000.0);
    sample_rate->set_value(static_cast<double>(sample_rate_hz));
    sample_rate->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    sample_rate_panel->add_child(sample_rate);

    HBoxContainer *baseband_row = memnew(HBoxContainer);
    baseband_row->set_name("BasebandRow");
    add_child(baseband_row);

    Label *baseband_label = memnew(Label);
    baseband_label->set_name("BasebandIOLabel");
    baseband_label->set_text("Baseband I/O");
    baseband_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    baseband_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    baseband_row->add_child(baseband_label);

    HBoxContainer *record_row = memnew(HBoxContainer);
    record_row->set_name("RecordRow");
    record_row->add_theme_constant_override("separation", 6);
    add_child(record_row);

    Label *record_label = memnew(Label);
    record_label->set_name("RecordLabel");
    record_label->set_text("Record");
    record_row->add_child(record_label);

    Button *record = memnew(Button);
    record->set_name("RecordButton");
    record->set_toggle_mode(true);
    record->set_text("ON");
    record_row->add_child(record);

    HBoxContainer *play_row = memnew(HBoxContainer);
    play_row->set_name("PlayRow");
    play_row->add_theme_constant_override("separation", 6);
    add_child(play_row);

    Label *play_label = memnew(Label);
    play_label->set_name("PlayLabel");
    play_label->set_text("Play");
    play_row->add_child(play_label);

    Button *play = memnew(Button);
    play->set_name("PlayButton");
    play->set_toggle_mode(true);
    play->set_text("ON");
    play_row->add_child(play);

    HBoxContainer *passthrough_row = memnew(HBoxContainer);
    passthrough_row->set_name("PassthroughRow");
    passthrough_row->add_theme_constant_override("separation", 6);
    add_child(passthrough_row);

    Label *passthrough_label = memnew(Label);
    passthrough_label->set_name("PassthroughLabel");
    passthrough_label->set_text("Pass Through");
    passthrough_row->add_child(passthrough_label);

    Button *passthrough = memnew(Button);
    passthrough->set_name("PassthroughButton");
    passthrough->set_toggle_mode(true);
    passthrough->set_text("ON");
    passthrough_row->add_child(passthrough);

    HBoxContainer *save_row = memnew(HBoxContainer);
    save_row->set_name("SaveRow");
    add_child(save_row);

    Label *save_label = memnew(Label);
    save_label->set_name("SaveLabel");
    save_label->set_text("Path");
    save_row->add_child(save_label);

    PanelContainer *save_panel = memnew(PanelContainer);
    save_panel->set_name("SavePathPanel");
    save_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    save_row->add_child(save_panel);

    LineEdit *save_path = memnew(LineEdit);
    save_path->set_name("SavePath");
    save_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    save_path->set_text("user://baseband_capture.esdrbb");
    save_panel->add_child(save_path);

    Button *save = memnew(Button);
    save->set_name("SaveButton");
    save->set_text("Save");
    save_row->add_child(save);

    HBoxContainer *action_row = memnew(HBoxContainer);
    action_row->set_name("ActionRow");
    action_row->add_theme_constant_override("separation", 6);
    add_child(action_row);

    Button *clear = memnew(Button);
    clear->set_name("ClearButton");
    clear->set_text("Clear");
    action_row->add_child(clear);

    Label *status = memnew(Label);
    status->set_name("StatusLabel");
    status->set_text("State: IDLE");
    add_child(status);

    Label *memory = memnew(Label);
    memory->set_name("MemoryLabel");
    memory->set_text("Memory: 0 B / 0 B");
    add_child(memory);

    PanelContainer *spectrogram_panel = memnew(PanelContainer);
    spectrogram_panel->set_name("SpectrogramPanel");
    spectrogram_panel->set_custom_minimum_size(Vector2(0, 96));
    spectrogram_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    add_child(spectrogram_panel);

    Control *spectrogram_overlay = memnew(Control);
    spectrogram_overlay->set_name("SpectrogramOverlay");
    spectrogram_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    spectrogram_overlay->set_custom_minimum_size(Vector2(0, 96));
    spectrogram_overlay->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    spectrogram_overlay->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    spectrogram_panel->add_child(spectrogram_overlay);

    TextureRect *spectrogram_texture_node = memnew(TextureRect);
    spectrogram_texture_node->set_name("SpectrogramTexture");
    spectrogram_texture_node->set_anchors_preset(Control::PRESET_FULL_RECT);
    spectrogram_texture_node->set_stretch_mode(TextureRect::STRETCH_SCALE);
    spectrogram_texture_node->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    spectrogram_overlay->add_child(spectrogram_texture_node);

    ColorRect *scrubber = memnew(ColorRect);
    scrubber->set_name("SpectrogramScrubber");
    scrubber->set_color(Color(1.0, 1.0, 1.0, 0.95));
    scrubber->set_position(Vector2(0, 0));
    scrubber->set_size(Vector2(2, 96));
    scrubber->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    spectrogram_overlay->add_child(scrubber);

    HBoxContainer *timeline_row = memnew(HBoxContainer);
    timeline_row->set_name("TimelineRow");
    timeline_row->add_theme_constant_override("separation", 6);
    add_child(timeline_row);

    Label *timeline_label = memnew(Label);
    timeline_label->set_name("TimelineLabel");
    timeline_label->set_text("Timeline");
    timeline_row->add_child(timeline_label);

    HSlider *timeline_slider = memnew(HSlider);
    timeline_slider->set_name("PlaybackSlider");
    timeline_slider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    timeline_slider->set_min(0.0);
    timeline_slider->set_max(0.0);
    timeline_slider->set_step(1.0);
    timeline_slider->set_value(0.0);
    timeline_row->add_child(timeline_slider);

    configure_slots();
}

void BasebandReplay::cache_ui_refs() {
    buffer_seconds_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("BufferRow/BufferPanel/BufferSecondsSpin")));
    center_frequency_selector = Object::cast_to<DigitNumberSelector>(get_node_or_null(NodePath("CenterFrequencyRow/CenterFrequencyPanel/CenterFrequencySelector")));
    sample_rate_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("SampleRateRow/SampleRatePanel/SampleRateSpin")));
    record_button = Object::cast_to<Button>(get_node_or_null(NodePath("RecordRow/RecordButton")));
    play_button = Object::cast_to<Button>(get_node_or_null(NodePath("PlayRow/PlayButton")));
    passthrough_button = Object::cast_to<Button>(get_node_or_null(NodePath("PassthroughRow/PassthroughButton")));
    clear_button = Object::cast_to<Button>(get_node_or_null(NodePath("ActionRow/ClearButton")));
    save_button = Object::cast_to<Button>(get_node_or_null(NodePath("SaveRow/SaveButton")));
    playback_slider = Object::cast_to<HSlider>(get_node_or_null(NodePath("TimelineRow/PlaybackSlider")));
    spectrogram_texture_rect = Object::cast_to<TextureRect>(get_node_or_null(NodePath("SpectrogramPanel/SpectrogramOverlay/SpectrogramTexture")));
    spectrogram_scrubber = Object::cast_to<ColorRect>(get_node_or_null(NodePath("SpectrogramPanel/SpectrogramOverlay/SpectrogramScrubber")));
    save_path_edit = Object::cast_to<LineEdit>(get_node_or_null(NodePath("SaveRow/SavePathPanel/SavePath")));
    status_label = Object::cast_to<Label>(get_node_or_null(NodePath("StatusLabel")));
    memory_label = Object::cast_to<Label>(get_node_or_null(NodePath("MemoryLabel")));
}

void BasebandReplay::bind_ui() {
    if (center_frequency_selector != nullptr) {
        const Callable cb(this, "_on_center_frequency_changed");
        if (!center_frequency_selector->is_connected("value_changed", cb)) {
            center_frequency_selector->connect("value_changed", cb);
        }
    }

    if (sample_rate_spin != nullptr) {
        const Callable cb(this, "_on_sample_rate_changed");
        if (!sample_rate_spin->is_connected("value_changed", cb)) {
            sample_rate_spin->connect("value_changed", cb);
        }
    }

    if (buffer_seconds_spin != nullptr) {
        const Callable cb(this, "_on_buffer_seconds_changed");
        if (!buffer_seconds_spin->is_connected("value_changed", cb)) {
            buffer_seconds_spin->connect("value_changed", cb);
        }
    }

    if (record_button != nullptr) {
        const Callable cb(this, "_on_record_toggled");
        if (!record_button->is_connected("toggled", cb)) {
            record_button->connect("toggled", cb);
        }
    }

    if (play_button != nullptr) {
        const Callable cb(this, "_on_play_toggled");
        if (!play_button->is_connected("toggled", cb)) {
            play_button->connect("toggled", cb);
        }
    }

    if (passthrough_button != nullptr) {
        const Callable cb(this, "_on_passthrough_toggled");
        if (!passthrough_button->is_connected("toggled", cb)) {
            passthrough_button->connect("toggled", cb);
        }
    }

    if (clear_button != nullptr) {
        const Callable cb(this, "_on_clear_pressed");
        if (!clear_button->is_connected("pressed", cb)) {
            clear_button->connect("pressed", cb);
        }
    }

    if (save_button != nullptr) {
        const Callable cb(this, "_on_save_pressed");
        if (!save_button->is_connected("pressed", cb)) {
            save_button->connect("pressed", cb);
        }
    }

    if (playback_slider != nullptr) {
        const Callable cb(this, "_on_playback_slider_changed");
        if (!playback_slider->is_connected("value_changed", cb)) {
            playback_slider->connect("value_changed", cb);
        }
    }
}

void BasebandReplay::configure_slots() {
    clear_all_slots();
    set_slot(0, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(3, true, SIGNAL_BASEBAND, signal_color(SIGNAL_BASEBAND), true, SIGNAL_BASEBAND, signal_color(SIGNAL_BASEBAND));
    set_slot(4, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(5, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(6, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
}
