#ifndef ESDR_BASEBAND_REPLAY_NODE_H
#define ESDR_BASEBAND_REPLAY_NODE_H

#include "digit_number_selector.h"
#include "sdr_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <thread>
#include <vector>

class BasebandReplay : public SDR {
    GDCLASS(BasebandReplay, SDR)

private:
    DigitNumberSelector *center_frequency_selector = nullptr;
    godot::SpinBox *sample_rate_spin = nullptr;
    godot::SpinBox *buffer_seconds_spin = nullptr;
    godot::Button *record_button = nullptr;
    godot::Button *play_button = nullptr;
    godot::Button *passthrough_button = nullptr;
    godot::Button *clear_button = nullptr;
    godot::Button *save_button = nullptr;
    godot::HSlider *playback_slider = nullptr;
    godot::TextureRect *spectrogram_texture_rect = nullptr;
    godot::ColorRect *spectrogram_scrubber = nullptr;
    godot::LineEdit *save_path_edit = nullptr;
    godot::Label *status_label = nullptr;
    godot::Label *memory_label = nullptr;
    godot::Ref<godot::ImageTexture> spectrogram_texture;

    int64_t center_frequency_hz = 88100000;
    int64_t sample_rate_hz = 2400000;
    double buffer_seconds = 15.0;
    bool record_enabled = true;
    bool play_enabled = false;
    bool passthrough_enabled = true;
    godot::PackedVector2Array last_live_frame;

    std::vector<godot::Vector2> ring_samples;
    std::size_t ring_capacity_samples = 0;
    std::size_t ring_write_index = 0;
    std::size_t ring_stored_samples = 0;

    std::size_t playback_offset = 0;
    double playback_pending_samples = 0.0;
    int32_t playback_chunk_size = 16384;
    bool spectrogram_dirty = true;

    std::thread spectrogram_worker_thread;
    std::atomic<bool> spectrogram_cancel_requested = false;
    std::atomic<bool> spectrogram_worker_running = false;
    std::atomic<int32_t> spectrogram_ready_columns = 0;
    int32_t spectrogram_width = 0;
    int32_t spectrogram_height = 0;
    int32_t spectrogram_uploaded_columns = 0;
    std::vector<uint8_t> spectrogram_pixels_rgba;
    godot::Ref<godot::Image> spectrogram_work_image;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();

    void on_ready();
    void on_exit_tree();
    void process_tick();

    void set_center_frequency_hz(int64_t p_frequency_hz, bool p_emit_signal = true);
    void set_sample_rate_hz(int64_t p_sample_rate_hz, bool p_emit_signal = true);
    void set_buffer_seconds(double p_seconds);
    void set_record_enabled(bool p_enabled);
    void set_play_enabled(bool p_enabled);
    void set_passthrough_enabled(bool p_enabled);
    void set_playback_offset(std::size_t p_offset, bool p_from_slider = false);
    void refresh_playback_slider_range();
    void update_playback_scrubber();
    void clear_spectrogram_texture();
    void rebuild_spectrogram_if_needed();
    void start_spectrogram_build_async();
    void stop_spectrogram_worker();
    void pump_spectrogram_render();
    static godot::Color spectrogram_color_from_norm(float p_norm);

    void resize_ring_preserving_latest(std::size_t p_new_capacity);
    void append_ring_sample(const godot::Vector2 &p_sample);
    void append_ring_frame(const godot::PackedVector2Array &p_frame);
    std::size_t ordered_to_ring_index(std::size_t p_ordered_index) const;
    godot::PackedVector2Array export_ordered_capture() const;

    static godot::String format_bytes(double p_bytes);
    void update_status_labels();
    void save_capture_to_path(const godot::String &p_path);

    void _on_center_frequency_changed(int64_t p_value);
    void _on_sample_rate_changed(double p_value);
    void _on_buffer_seconds_changed(double p_value);
    void _on_record_toggled(bool p_enabled);
    void _on_play_toggled(bool p_enabled);
    void _on_passthrough_toggled(bool p_enabled);
    void _on_playback_slider_changed(double p_value);
    void _on_clear_pressed();
    void _on_save_pressed();

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    void push_baseband_frame(const godot::PackedVector2Array &p_frame);
    int64_t get_current_sample_rate_hz() const;
    bool is_replay_playing() const;
    bool is_record_enabled() const;
    bool is_passthrough_enabled() const;
    void set_upstream_tuned_frequency_hz(int64_t p_frequency_hz);

    godot::Variant get_port_value(int64_t p_port) const;
    void set_port_value(int64_t p_port, const godot::Variant &p_value);
};

#endif // ESDR_BASEBAND_REPLAY_NODE_H
