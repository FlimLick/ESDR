#ifndef ESDR_AUDIO_SINK_NODE_H
#define ESDR_AUDIO_SINK_NODE_H

#include "sdr_node.h"

#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

class AudioSink : public SDR {
    GDCLASS(AudioSink, SDR)

private:
    godot::AudioStreamPlayer *player = nullptr;
    godot::Control *device_row = nullptr;
    godot::Label *device_label = nullptr;
    godot::OptionButton *device_selector = nullptr;
    godot::HSlider *volume_slider = nullptr;
    godot::Label *volume_min_label = nullptr;
    godot::SpinBox *volume_value_spin = nullptr;
    godot::Label *volume_max_label = nullptr;
    godot::CheckBox *mute_checkbox = nullptr;
    godot::Button *clear_button = nullptr;
    godot::Label *status_label = nullptr;

    godot::Ref<godot::AudioStreamGenerator> generator;
    godot::Ref<godot::AudioStreamGeneratorPlayback> playback;

    std::mutex queue_mutex;
    std::deque<godot::Vector2> sample_queue;
    int32_t mix_rate = 48000;
    int32_t max_queued_samples = 12000;
    uint64_t debug_last_log_us = 0;
    int64_t debug_in_mono_samples = 0;
    int64_t debug_in_stereo_samples = 0;
    int64_t debug_out_samples = 0;
    int64_t debug_drop_samples = 0;
    int64_t debug_in_clip_samples = 0;
    int64_t debug_out_clip_samples = 0;
    std::unordered_map<std::string, int64_t> debug_source_samples;
    bool debug_logging_enabled = false;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();
    void update_status();
    void refresh_output_devices();
    void refresh_volume_header();
    int32_t wrap_index(int32_t p_index, int32_t p_count) const;
    void log_debug_stats_if_due();
    void record_source_samples(const godot::StringName &p_source_id, int32_t p_samples);

    void on_ready();
    void on_exit_tree();
    void process_tick();

    void _on_volume_changed(double p_value);
    void _on_mute_toggled(bool p_enabled);
    void _on_clear_pressed();
    void _on_device_selected(int64_t p_index);

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
    void set_debug_logging_enabled(bool p_enabled);
    bool get_debug_logging_enabled() const;
};

#endif // ESDR_AUDIO_SINK_NODE_H
