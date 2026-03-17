#ifndef ESDR_AUDIO_MIXER_NODE_H
#define ESDR_AUDIO_MIXER_NODE_H

#include "sdr_node.h"

#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/templates/vector.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

class AudioMixer : public SDR {
    GDCLASS(AudioMixer, SDR)

private:
    enum MixMode {
        MIX_AVERAGE = 0,
        MIX_ADD = 1,
    };

    godot::Label *output_label = nullptr;
    godot::Label *status_label = nullptr;
    godot::OptionButton *mode_selector = nullptr;
    godot::HSlider *gain_slider = nullptr;
    godot::Label *gain_min_label = nullptr;
    godot::SpinBox *gain_value_spin = nullptr;
    godot::Label *gain_max_label = nullptr;
    godot::Vector<godot::Label *> input_labels;

    int32_t connected_source_count = 0;
    int32_t input_slot_count = 1;
    float output_gain = 1.0f;
    int32_t mix_mode = MIX_AVERAGE;

    std::mutex frames_mutex;
    std::unordered_map<std::string, godot::PackedFloat32Array> source_frames;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();
    void rebuild_input_rows();
    void update_status();
    void refresh_gain_header();
    int32_t wrap_index(int32_t p_index, int32_t p_count) const;

    void on_ready();
    void on_exit_tree();
    void process_tick();

    void _on_mix_mode_selected(int64_t p_index);
    void _on_gain_changed(double p_value);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    void push_audio_input(const godot::PackedFloat32Array &p_frame, const godot::StringName &p_source_id);
    void set_connected_source_count(int32_t p_count);
    int32_t get_connected_source_count() const;

    godot::Variant get_port_value(int64_t p_port) const;
    void set_port_value(int64_t p_port, const godot::Variant &p_value);
};

#endif // ESDR_AUDIO_MIXER_NODE_H
