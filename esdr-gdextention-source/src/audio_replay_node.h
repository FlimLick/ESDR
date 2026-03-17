#ifndef ESDR_AUDIO_REPLAY_NODE_H
#define ESDR_AUDIO_REPLAY_NODE_H

#include "sdr_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

class AudioReplay : public SDR {
    GDCLASS(AudioReplay, SDR)

private:
    godot::SpinBox *sample_rate_spin = nullptr;
    godot::SpinBox *buffer_seconds_spin = nullptr;
    godot::Button *record_button = nullptr;
    godot::Button *play_button = nullptr;
    godot::Button *passthrough_button = nullptr;
    godot::Button *clear_button = nullptr;
    godot::Button *save_button = nullptr;
    godot::HSlider *playback_slider = nullptr;
    godot::LineEdit *save_path_edit = nullptr;
    godot::Label *status_label = nullptr;
    godot::Label *memory_label = nullptr;

    int64_t sample_rate_hz = 48000;
    double buffer_seconds = 30.0;
    bool record_enabled = true;
    bool play_enabled = false;
    bool passthrough_enabled = true;
    godot::PackedFloat32Array last_live_frame;

    std::vector<float> ring_samples;
    std::size_t ring_capacity_samples = 0;
    std::size_t ring_write_index = 0;
    std::size_t ring_stored_samples = 0;

    std::size_t playback_offset = 0;
    double playback_pending_samples = 0.0;
    int32_t playback_chunk_size = 4096;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();

    void on_ready();
    void on_exit_tree();
    void process_tick();

    void set_sample_rate_hz(int64_t p_sample_rate_hz);
    void set_buffer_seconds(double p_seconds);
    void set_record_enabled(bool p_enabled);
    void set_play_enabled(bool p_enabled);
    void set_passthrough_enabled(bool p_enabled);
    void set_playback_offset(std::size_t p_offset, bool p_from_slider = false);
    void refresh_playback_slider_range();

    void resize_ring_preserving_latest(std::size_t p_new_capacity);
    void append_ring_sample(float p_sample);
    void append_ring_frame(const godot::PackedFloat32Array &p_frame);
    std::size_t ordered_to_ring_index(std::size_t p_ordered_index) const;
    godot::PackedFloat32Array export_ordered_capture() const;

    static godot::String format_bytes(double p_bytes);
    void update_status_labels();
    void save_capture_to_path(const godot::String &p_path);

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
    void push_audio_frame(const godot::PackedFloat32Array &p_frame);
    void push_audio_stereo_frame(const godot::PackedVector2Array &p_frame);
    void push_audio_frame_from_source(const godot::PackedFloat32Array &p_frame, const godot::StringName &p_source_id);
    void push_audio_stereo_frame_from_source(const godot::PackedVector2Array &p_frame, const godot::StringName &p_source_id);

    godot::Variant get_port_value(int64_t p_port) const;
    void set_port_value(int64_t p_port, const godot::Variant &p_value);
};

#endif // ESDR_AUDIO_REPLAY_NODE_H
