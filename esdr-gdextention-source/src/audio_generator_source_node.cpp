#include "audio_generator_source_node.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace {
constexpr double PI = 3.14159265358979323846;
}

void AudioGeneratorSource::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_start_toggled", "enabled"), &AudioGeneratorSource::_on_start_toggled);
    ClassDB::bind_method(D_METHOD("_on_waveform_selected", "index"), &AudioGeneratorSource::_on_waveform_selected);
    ClassDB::bind_method(D_METHOD("_on_frequency_changed", "value"), &AudioGeneratorSource::_on_frequency_changed);
    ClassDB::bind_method(D_METHOD("_on_amplitude_changed", "value"), &AudioGeneratorSource::_on_amplitude_changed);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &AudioGeneratorSource::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &AudioGeneratorSource::set_port_value);

    ADD_SIGNAL(MethodInfo("audio_frame", PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "frame")));
}

void AudioGeneratorSource::_notification(int32_t p_what) {
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

void AudioGeneratorSource::on_ready() {
    ensure_ui();
    cache_ui_refs();
    bind_ui();
    const int32_t engine_mix_rate = AudioServer::get_singleton() != nullptr ? AudioServer::get_singleton()->get_mix_rate() : 0;
    if (engine_mix_rate > 0) {
        sample_rate = static_cast<double>(engine_mix_rate);
    }
    refresh_amplitude_header();
    set_status_text();

    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }

    set_process(true);
}

void AudioGeneratorSource::on_exit_tree() {
    running = false;
    pending_samples = 0.0;
}

void AudioGeneratorSource::process_tick() {
    if (!running || Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    const double delta = get_process_delta_time();
    if (delta <= 0.0) {
        return;
    }

    pending_samples += sample_rate * delta;

    const int32_t max_frames_per_tick = 8;
    int32_t emitted_frames = 0;
    while (pending_samples >= frame_size && emitted_frames < max_frames_per_tick) {
        const PackedFloat32Array frame = generate_audio_frame();
        if (frame.is_empty()) {
            break;
        }

        emit_signal("audio_frame", frame);
        pending_samples -= static_cast<double>(frame_size);
        emitted_frames++;
    }

    const double max_pending = sample_rate * 0.5;
    if (pending_samples > max_pending) {
        pending_samples = max_pending;
    }
}

PackedFloat32Array AudioGeneratorSource::generate_audio_frame() {
    PackedFloat32Array frame;
    frame.resize(frame_size);
    float *out = frame.ptrw();

    const double amp = std::clamp(amplitude, 0.0, 1.0);
    const double frequency = std::max(0.0, frequency_hz);
    const double phase_step = (sample_rate <= 0.0) ? 0.0 : (frequency / sample_rate);

    for (int32_t i = 0; i < frame_size; i++) {
        double sample = 0.0;
        switch (waveform) {
            case WAVE_SINE:
                sample = std::sin(2.0 * PI * phase);
                break;
            case WAVE_SQUARE:
                sample = (phase < 0.5) ? 1.0 : -1.0;
                break;
            case WAVE_SAW:
                sample = (2.0 * phase) - 1.0;
                break;
            case WAVE_TRIANGLE:
                sample = 1.0 - 4.0 * std::abs(phase - 0.5);
                break;
            case WAVE_NOISE:
                sample = UtilityFunctions::randf() * 2.0 - 1.0;
                break;
            default:
                sample = 0.0;
                break;
        }

        out[i] = static_cast<float>(sample * amp);
        phase += phase_step;
        while (phase >= 1.0) {
            phase -= 1.0;
        }
        while (phase < 0.0) {
            phase += 1.0;
        }
    }

    return frame;
}

void AudioGeneratorSource::_on_start_toggled(bool p_enabled) {
    running = p_enabled;
    pending_samples = 0.0;
    if (status_button != nullptr && status_button->is_pressed() != p_enabled) {
        status_button->set_pressed_no_signal(p_enabled);
    }
    set_status_text();
}

int32_t AudioGeneratorSource::wrap_index(int32_t p_index, int32_t p_count) const {
    if (p_count <= 0) {
        return 0;
    }

    int32_t wrapped = p_index % p_count;
    if (wrapped < 0) {
        wrapped += p_count;
    }
    return wrapped;
}

void AudioGeneratorSource::_on_waveform_selected(int64_t p_index) {
    const int32_t idx = static_cast<int32_t>(p_index);
    waveform = std::clamp(idx, static_cast<int32_t>(WAVE_SINE), static_cast<int32_t>(WAVE_NOISE));
}

void AudioGeneratorSource::_on_frequency_changed(double p_value) {
    frequency_hz = std::max(0.0, p_value);
}

void AudioGeneratorSource::_on_amplitude_changed(double p_value) {
    if (amplitude_slider != nullptr && std::abs(amplitude_slider->get_value() - p_value) > 1e-9) {
        amplitude_slider->set_value_no_signal(p_value);
    }
    amplitude = std::clamp(p_value, 0.0, 1.0);
    refresh_amplitude_header();
}

Variant AudioGeneratorSource::get_port_value(int64_t p_port) const {
    switch (p_port) {
        case 1:
            return waveform;
        case 2:
            return static_cast<int64_t>(std::llround(frequency_hz));
        case 4:
            return amplitude;
        default:
            return Variant();
    }
}

void AudioGeneratorSource::set_port_value(int64_t p_port, const Variant &p_value) {
    switch (p_port) {
        case 1: {
            if (waveform_selector == nullptr || waveform_selector->get_item_count() <= 0) {
                return;
            }
            const int32_t wrapped = wrap_index(static_cast<int32_t>(p_value), waveform_selector->get_item_count());
            waveform_selector->select(wrapped);
            _on_waveform_selected(wrapped);
            break;
        }
        case 2: {
            const double value = static_cast<double>(p_value);
            if (frequency_spin != nullptr) {
                frequency_spin->set_value_no_signal(value);
            }
            _on_frequency_changed(value);
            break;
        }
        case 4: {
            const double value = static_cast<double>(p_value);
            if (amplitude_slider != nullptr) {
                amplitude_slider->set_value_no_signal(value);
            }
            _on_amplitude_changed(value);
            break;
        }
        default:
            break;
    }
}

void AudioGeneratorSource::cache_ui_refs() {
    waveform_selector = Object::cast_to<OptionButton>(get_node_or_null(NodePath("WaveformRow/WaveformSelector")));
    frequency_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("FrequencyRow/FrequencyPanel/FrequencySpin")));
    amplitude_slider = Object::cast_to<HSlider>(get_node_or_null(NodePath("AmplitudeSlider")));
    amplitude_min_label = Object::cast_to<Label>(get_node_or_null(NodePath("AmplitudeHeaderRow/AmplitudeMinLabel")));
    amplitude_value_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("AmplitudeHeaderRow/AmplitudeValueSpin")));
    amplitude_max_label = Object::cast_to<Label>(get_node_or_null(NodePath("AmplitudeHeaderRow/AmplitudeMaxLabel")));
    status_button = Object::cast_to<Button>(get_node_or_null(NodePath("StartRow/StatusButton")));
}

void AudioGeneratorSource::bind_ui() {
    if (status_button != nullptr) {
        const Callable cb(this, "_on_start_toggled");
        if (!status_button->is_connected("toggled", cb)) {
            status_button->connect("toggled", cb);
        }
    }

    if (waveform_selector != nullptr) {
        const Callable cb(this, "_on_waveform_selected");
        if (!waveform_selector->is_connected("item_selected", cb)) {
            waveform_selector->connect("item_selected", cb);
        }
    }

    if (frequency_spin != nullptr) {
        const Callable cb(this, "_on_frequency_changed");
        if (!frequency_spin->is_connected("value_changed", cb)) {
            frequency_spin->connect("value_changed", cb);
        }
    }

    if (amplitude_slider != nullptr) {
        const Callable cb(this, "_on_amplitude_changed");
        if (!amplitude_slider->is_connected("value_changed", cb)) {
            amplitude_slider->connect("value_changed", cb);
        }
    }
    if (amplitude_value_spin != nullptr) {
        const Callable cb(this, "_on_amplitude_changed");
        if (!amplitude_value_spin->is_connected("value_changed", cb)) {
            amplitude_value_spin->connect("value_changed", cb);
        }
    }
}

void AudioGeneratorSource::set_status_text() {
    if (status_button == nullptr) {
        return;
    }

    const Color off_color = Color(1.0, 0.23, 0.23, 1.0);
    const Color on_color = Color(0.35, 1.0, 0.44, 1.0);
    const Color state_color = running ? on_color : off_color;

    // Keep panel/theme styles untouched; only tint the button text.
    status_button->remove_theme_stylebox_override("normal");
    status_button->remove_theme_stylebox_override("hover");
    status_button->remove_theme_stylebox_override("pressed");
    status_button->remove_theme_stylebox_override("focus");
    status_button->remove_theme_stylebox_override("disabled");

    status_button->add_theme_color_override("font_color", state_color);
    status_button->add_theme_color_override("font_pressed_color", state_color);
    status_button->add_theme_color_override("font_hover_color", state_color);
    status_button->add_theme_color_override("font_hover_pressed_color", state_color);
    status_button->add_theme_color_override("font_focus_color", state_color);
    status_button->add_theme_color_override("font_disabled_color", state_color.darkened(0.5));

    if (running) {
        status_button->set_text("ON");
    } else {
        status_button->set_text("OFF");
    }
}

void AudioGeneratorSource::refresh_amplitude_header() {
    if (amplitude_slider == nullptr) {
        return;
    }

    if (amplitude_min_label != nullptr) {
        amplitude_min_label->set_text(String("min ") + String::num(amplitude_slider->get_min()));
        amplitude_min_label->add_theme_font_size_override("font_size", 10);
        amplitude_min_label->add_theme_color_override("font_color", Color(0.67, 0.67, 0.67, 1.0));
    }
    if (amplitude_value_spin != nullptr) {
        amplitude_value_spin->set_min(amplitude_slider->get_min());
        amplitude_value_spin->set_max(amplitude_slider->get_max());
        amplitude_value_spin->set_step(amplitude_slider->get_step());
        amplitude_value_spin->set_value_no_signal(amplitude_slider->get_value());
    }
    if (amplitude_max_label != nullptr) {
        amplitude_max_label->set_text(String("max ") + String::num(amplitude_slider->get_max()));
        amplitude_max_label->add_theme_font_size_override("font_size", 10);
        amplitude_max_label->add_theme_color_override("font_color", Color(0.67, 0.67, 0.67, 1.0));
    }
}

void AudioGeneratorSource::configure_slots() {
    clear_all_slots();
    set_slot(0, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(3, false, 0, Color(1.0, 1.0, 1.0, 1.0), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(4, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(5, false, 0, Color(1.0, 1.0, 1.0, 1.0), false, 0, Color(1.0, 1.0, 1.0, 1.0));
}

void AudioGeneratorSource::ensure_ui() {
    if (has_node(NodePath("WaveformRow/WaveformSelector")) && has_node(NodePath("StartRow/StatusButton"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("Audio Generator");
    set_custom_minimum_size(Vector2(400, 300));
    set_resizable(true);

    Label *output_label = memnew(Label);
    output_label->set_name("OutputLabel");
    output_label->set_text("Audio");
    output_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    add_child(output_label);

    HBoxContainer *waveform_row = memnew(HBoxContainer);
    waveform_row->set_name("WaveformRow");
    add_child(waveform_row);

    Label *waveform_label = memnew(Label);
    waveform_label->set_name("WaveformLabel");
    waveform_label->set_text("Waveform");
    waveform_row->add_child(waveform_label);

    OptionButton *waveform = memnew(OptionButton);
    waveform->set_name("WaveformSelector");
    waveform->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    waveform->set_fit_to_longest_item(false);
    waveform->set_clip_text(true);
    waveform->add_item("Sine", WAVE_SINE);
    waveform->add_item("Square", WAVE_SQUARE);
    waveform->add_item("Saw", WAVE_SAW);
    waveform->add_item("Triangle", WAVE_TRIANGLE);
    waveform->add_item("Noise", WAVE_NOISE);
    waveform->select(WAVE_SINE);
    waveform_row->add_child(waveform);

    HBoxContainer *frequency_row = memnew(HBoxContainer);
    frequency_row->set_name("FrequencyRow");
    frequency_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    frequency_row->add_theme_constant_override("separation", 8);
    add_child(frequency_row);

    Label *frequency_label = memnew(Label);
    frequency_label->set_name("FrequencyLabel");
    frequency_label->set_text("Frequency");
    frequency_row->add_child(frequency_label);

    PanelContainer *frequency_panel = memnew(PanelContainer);
    frequency_panel->set_name("FrequencyPanel");
    frequency_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    frequency_row->add_child(frequency_panel);

    SpinBox *frequency = memnew(SpinBox);
    frequency->set_name("FrequencySpin");
    frequency->set_min(1.0);
    frequency->set_max(30000.0);
    frequency->set_value(frequency_hz);
    frequency->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    frequency_panel->add_child(frequency);

    HBoxContainer *amplitude_header_row = memnew(HBoxContainer);
    amplitude_header_row->set_name("AmplitudeHeaderRow");
    amplitude_header_row->add_theme_constant_override("separation", 6);
    add_child(amplitude_header_row);

    Label *amplitude_label = memnew(Label);
    amplitude_label->set_name("AmplitudeLabel");
    amplitude_label->set_text("Amplitude");
    amplitude_header_row->add_child(amplitude_label);

    Control *amplitude_spacer = memnew(Control);
    amplitude_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    amplitude_header_row->add_child(amplitude_spacer);

    Label *amplitude_min = memnew(Label);
    amplitude_min->set_name("AmplitudeMinLabel");
    amplitude_header_row->add_child(amplitude_min);

    SpinBox *amplitude_value = memnew(SpinBox);
    amplitude_value->set_name("AmplitudeValueSpin");
    amplitude_value->set_min(0.0);
    amplitude_value->set_max(1.0);
    amplitude_value->set_step(0.01);
    amplitude_value->set_custom_minimum_size(Vector2(86.0, 0.0));
    amplitude_header_row->add_child(amplitude_value);

    Label *amplitude_max = memnew(Label);
    amplitude_max->set_name("AmplitudeMaxLabel");
    amplitude_header_row->add_child(amplitude_max);

    HSlider *amplitude_control = memnew(HSlider);
    amplitude_control->set_name("AmplitudeSlider");
    amplitude_control->set_min(0.0);
    amplitude_control->set_max(1.0);
    amplitude_control->set_step(0.01);
    amplitude_control->set_value(amplitude);
    add_child(amplitude_control);

    HBoxContainer *start_row = memnew(HBoxContainer);
    start_row->set_name("StartRow");
    add_child(start_row);

    Label *status_prefix = memnew(Label);
    status_prefix->set_name("StatusPrefixLabel");
    status_prefix->set_text("Status:");
    start_row->add_child(status_prefix);

    Button *status = memnew(Button);
    status->set_name("StatusButton");
    status->set_toggle_mode(true);
    status->set_text("OFF");
    start_row->add_child(status);

    configure_slots();
}
