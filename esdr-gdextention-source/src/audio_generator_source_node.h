#ifndef ESDR_AUDIO_GENERATOR_SOURCE_NODE_H
#define ESDR_AUDIO_GENERATOR_SOURCE_NODE_H

#include "sdr_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/variant.hpp>

class AudioGeneratorSource : public SDR {
    GDCLASS(AudioGeneratorSource, SDR)

private:
    enum Waveform {
        WAVE_SINE = 0,
        WAVE_SQUARE = 1,
        WAVE_SAW = 2,
        WAVE_TRIANGLE = 3,
        WAVE_NOISE = 4,
    };

    godot::OptionButton *waveform_selector = nullptr;
    godot::SpinBox *frequency_spin = nullptr;
    godot::HSlider *amplitude_slider = nullptr;
    godot::Label *amplitude_min_label = nullptr;
    godot::SpinBox *amplitude_value_spin = nullptr;
    godot::Label *amplitude_max_label = nullptr;
    godot::Button *status_button = nullptr;

    int32_t waveform = WAVE_SINE;
    double frequency_hz = 1000.0;
    double amplitude = 0.5;
    double sample_rate = 48000.0;
    double phase = 0.0;
    double pending_samples = 0.0;
    int32_t frame_size = 1024;
    bool running = false;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();
    void set_status_text();
    void refresh_amplitude_header();
    int32_t wrap_index(int32_t p_index, int32_t p_count) const;
    godot::PackedFloat32Array generate_audio_frame();

    void on_ready();
    void on_exit_tree();
    void process_tick();

    void _on_start_toggled(bool p_enabled);
    void _on_waveform_selected(int64_t p_index);
    void _on_frequency_changed(double p_value);
    void _on_amplitude_changed(double p_value);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    godot::Variant get_port_value(int64_t p_port) const;
    void set_port_value(int64_t p_port, const godot::Variant &p_value);
};

#endif // ESDR_AUDIO_GENERATOR_SOURCE_NODE_H
