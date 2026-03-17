#include "cw_demodulator_node.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

void CWDemodulator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_offset_changed", "value"), &CWDemodulator::_on_offset_changed);
    ClassDB::bind_method(D_METHOD("_on_bandwidth_changed", "value"), &CWDemodulator::_on_bandwidth_changed);
    ClassDB::bind_method(D_METHOD("_on_tone_changed", "value"), &CWDemodulator::_on_tone_changed);
    ClassDB::bind_method(D_METHOD("_on_output_volume_changed", "value"), &CWDemodulator::_on_output_volume_changed);
    ClassDB::bind_method(D_METHOD("_on_tune_to_source_pressed"), &CWDemodulator::_on_tune_to_source_pressed);
    ClassDB::bind_method(D_METHOD("_on_lock_to_source_toggled", "enabled"), &CWDemodulator::_on_lock_to_source_toggled);

    ClassDB::bind_method(D_METHOD("set_offset_hz", "value"), &CWDemodulator::set_offset_hz);
    ClassDB::bind_method(D_METHOD("get_offset_hz"), &CWDemodulator::get_offset_hz);

    ClassDB::bind_method(D_METHOD("set_bandwidth_hz", "value"), &CWDemodulator::set_bandwidth_hz);
    ClassDB::bind_method(D_METHOD("get_bandwidth_hz"), &CWDemodulator::get_bandwidth_hz);

    ClassDB::bind_method(D_METHOD("set_tone_hz", "value"), &CWDemodulator::set_tone_hz);
    ClassDB::bind_method(D_METHOD("get_tone_hz"), &CWDemodulator::get_tone_hz);
    ClassDB::bind_method(D_METHOD("set_output_volume", "value"), &CWDemodulator::set_output_volume);
    ClassDB::bind_method(D_METHOD("get_output_volume"), &CWDemodulator::get_output_volume);
    ClassDB::bind_method(D_METHOD("set_upstream_tuned_frequency_hz", "frequency_hz"), &CWDemodulator::set_upstream_tuned_frequency_hz);
    ClassDB::bind_method(D_METHOD("set_lock_to_source_frequency", "enabled"), &CWDemodulator::set_lock_to_source_frequency);
    ClassDB::bind_method(D_METHOD("get_lock_to_source_frequency"), &CWDemodulator::get_lock_to_source_frequency);

    ClassDB::bind_method(D_METHOD("set_input_sample_rate_hz", "value"), &CWDemodulator::set_input_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("get_input_sample_rate_hz"), &CWDemodulator::get_input_sample_rate_hz);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &CWDemodulator::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &CWDemodulator::set_port_value);
    ClassDB::bind_method(D_METHOD("push_baseband_frame", "frame"), &CWDemodulator::push_baseband_frame);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "offset_hz", PROPERTY_HINT_RANGE, "-999000000000,999000000000,1"), "set_offset_hz", "get_offset_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bandwidth_hz", PROPERTY_HINT_RANGE, "100,5000,1"), "set_bandwidth_hz", "get_bandwidth_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tone_hz", PROPERTY_HINT_RANGE, "100,3000,1"), "set_tone_hz", "get_tone_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "output_volume", PROPERTY_HINT_RANGE, "0,4,0.01"), "set_output_volume", "get_output_volume");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lock_to_source_frequency"), "set_lock_to_source_frequency", "get_lock_to_source_frequency");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "input_sample_rate_hz", PROPERTY_HINT_RANGE, "1000,120000000,1"), "set_input_sample_rate_hz", "get_input_sample_rate_hz");
    ADD_SIGNAL(MethodInfo("audio_frame", PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "frame")));
    ADD_SIGNAL(MethodInfo("passthrough_frequency_request", PropertyInfo(Variant::INT, "frequency_hz")));
    ADD_SIGNAL(MethodInfo("offset_user_changed", PropertyInfo(Variant::INT, "offset_hz")));
}

void CWDemodulator::_notification(int32_t p_what) {
    if (p_what != NOTIFICATION_READY) {
        return;
    }

    ensure_ui();
    cache_ui_refs();
    bind_ui();
    refresh_dsp_config();
}

void CWDemodulator::set_offset_hz(double p_value) {
    offset_hz = std::clamp(p_value, -999000000000.0, 999000000000.0);
    if (offset_selector != nullptr) {
        offset_selector->set_value_no_signal(static_cast<int64_t>(std::llround(offset_hz)));
    }
    dsp_config_dirty = true;
}

double CWDemodulator::get_offset_hz() const {
    return offset_hz;
}

void CWDemodulator::set_bandwidth_hz(double p_value) {
    bandwidth_hz = std::clamp(p_value, 100.0, 5000.0);
    if (bandwidth_spin != nullptr) {
        bandwidth_spin->set_value_no_signal(bandwidth_hz);
    }
    dsp_config_dirty = true;
}

double CWDemodulator::get_bandwidth_hz() const {
    return bandwidth_hz;
}

void CWDemodulator::set_tone_hz(double p_value) {
    tone_hz = std::clamp(p_value, 100.0, 3000.0);
    if (tone_spin != nullptr) {
        tone_spin->set_value_no_signal(tone_hz);
    }
    dsp_config_dirty = true;
}

double CWDemodulator::get_tone_hz() const {
    return tone_hz;
}

void CWDemodulator::set_output_volume(double p_value) {
    output_volume = std::clamp(p_value, 0.0, 4.0);
    if (output_volume_spin != nullptr) {
        output_volume_spin->set_value_no_signal(output_volume);
    }
}

double CWDemodulator::get_output_volume() const {
    return output_volume;
}

void CWDemodulator::set_lock_to_source_frequency(bool p_enabled) {
    lock_to_source_frequency = p_enabled;
    if (lock_to_source_button != nullptr) {
        lock_to_source_button->set_pressed_no_signal(lock_to_source_frequency);
    }
}

bool CWDemodulator::get_lock_to_source_frequency() const {
    return lock_to_source_frequency;
}

void CWDemodulator::set_input_sample_rate_hz(double p_value) {
    input_sample_rate_hz = std::clamp(p_value, 1000.0, 120000000.0);
    dsp_config_dirty = true;
}

double CWDemodulator::get_input_sample_rate_hz() const {
    return input_sample_rate_hz;
}

void CWDemodulator::set_upstream_tuned_frequency_hz(int64_t p_frequency_hz) {
    upstream_tuned_frequency_hz = p_frequency_hz;
}

void CWDemodulator::_on_offset_changed(int64_t p_value) {
    set_offset_hz(static_cast<double>(p_value));
    emit_signal("offset_user_changed", static_cast<int64_t>(std::llround(offset_hz)));
}

void CWDemodulator::_on_bandwidth_changed(double p_value) {
    set_bandwidth_hz(p_value);
}

void CWDemodulator::_on_tone_changed(double p_value) {
    set_tone_hz(p_value);
}

void CWDemodulator::_on_output_volume_changed(double p_value) {
    set_output_volume(p_value);
}

void CWDemodulator::_on_tune_to_source_pressed() {
    const int64_t target_hz = upstream_tuned_frequency_hz + static_cast<int64_t>(std::llround(offset_hz));
    emit_signal("passthrough_frequency_request", target_hz);
}

void CWDemodulator::_on_lock_to_source_toggled(bool p_enabled) {
    set_lock_to_source_frequency(p_enabled);
}

void CWDemodulator::refresh_dsp_config() {
    if (!dsp_config_dirty) {
        return;
    }

    double audio_mix_rate = 48000.0;
    AudioServer *audio_server = AudioServer::get_singleton();
    if (audio_server != nullptr) {
        const int32_t mix_rate = audio_server->get_mix_rate();
        if (mix_rate > 0) {
            audio_mix_rate = static_cast<double>(mix_rate);
        }
    }

    esdr_demod::CWParams params;
    params.input_sample_rate_hz = input_sample_rate_hz;
    params.audio_sample_rate_hz = audio_mix_rate;
    params.offset_hz = offset_hz;
    params.bandwidth_hz = bandwidth_hz;
    params.tone_hz = tone_hz;
    demod_core.configure(params);

    dsp_config_dirty = false;
}

void CWDemodulator::push_baseband_frame(const PackedVector2Array &p_frame) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    if (p_frame.is_empty()) {
        return;
    }

    refresh_dsp_config();

    PackedFloat32Array audio;
    demod_core.process_frame(p_frame, audio);
    if (!audio.is_empty()) {
        for (int32_t i = 0; i < audio.size(); i++) {
            audio[i] = std::clamp(audio[i] * static_cast<float>(output_volume), -1.0f, 1.0f);
        }
        emit_signal("audio_frame", audio);
    }
}

Variant CWDemodulator::get_port_value(int64_t p_port) const {
    switch (p_port) {
        case 0:
            return static_cast<int64_t>(std::llround(offset_hz));
        case 1:
            return static_cast<int64_t>(std::llround(bandwidth_hz));
        case 2:
            return static_cast<int64_t>(std::llround(tone_hz));
        case 5:
            return output_volume;
        default:
            return Variant();
    }
}

void CWDemodulator::set_port_value(int64_t p_port, const Variant &p_value) {
    switch (p_port) {
        case 0:
            set_offset_hz(static_cast<double>(p_value));
            break;
        case 1:
            set_bandwidth_hz(static_cast<double>(p_value));
            break;
        case 2:
            set_tone_hz(static_cast<double>(p_value));
            break;
        case 5:
            set_output_volume(static_cast<double>(p_value));
            break;
        case 3:
            if (p_value.get_type() == Variant::PACKED_VECTOR2_ARRAY) {
                push_baseband_frame(p_value);
            }
            break;
        default:
            break;
    }
}

void CWDemodulator::cache_ui_refs() {
    offset_selector = Object::cast_to<DigitNumberSelector>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/OffsetPanel/OffsetSelector")));
    tune_to_source_button = Object::cast_to<Button>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/TuneToSourceButton")));
    lock_to_source_button = Object::cast_to<Button>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/LockToSourceButton")));
    bandwidth_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("BandwidthRow/BandwidthPanel/BandwidthSpin")));
    tone_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("ToneRow/TonePanel/ToneSpin")));
    output_volume_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("OutputVolumeRow/OutputVolumePanel/OutputVolumeSpin")));
}

void CWDemodulator::bind_ui() {
    if (offset_selector != nullptr) {
        const Callable cb(this, "_on_offset_changed");
        if (!offset_selector->is_connected("value_changed", cb)) {
            offset_selector->connect("value_changed", cb);
        }
    }

    if (bandwidth_spin != nullptr) {
        const Callable cb(this, "_on_bandwidth_changed");
        if (!bandwidth_spin->is_connected("value_changed", cb)) {
            bandwidth_spin->connect("value_changed", cb);
        }
    }

    if (tone_spin != nullptr) {
        const Callable cb(this, "_on_tone_changed");
        if (!tone_spin->is_connected("value_changed", cb)) {
            tone_spin->connect("value_changed", cb);
        }
    }

    if (output_volume_spin != nullptr) {
        output_volume_spin->set_value_no_signal(output_volume);
        const Callable cb(this, "_on_output_volume_changed");
        if (!output_volume_spin->is_connected("value_changed", cb)) {
            output_volume_spin->connect("value_changed", cb);
        }
    }

    if (tune_to_source_button != nullptr) {
        tune_to_source_button->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT);
        tune_to_source_button->set_position(Vector2(-52, 0));
        tune_to_source_button->set_size(Vector2(48, 20));
        const Callable cb(this, "_on_tune_to_source_pressed");
        if (!tune_to_source_button->is_connected("pressed", cb)) {
            tune_to_source_button->connect("pressed", cb);
        }
    }

    if (lock_to_source_button != nullptr) {
        lock_to_source_button->set_pressed_no_signal(lock_to_source_frequency);
        lock_to_source_button->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT);
        lock_to_source_button->set_position(Vector2(-52, 20));
        lock_to_source_button->set_size(Vector2(48, 20));
        const Callable cb(this, "_on_lock_to_source_toggled");
        if (!lock_to_source_button->is_connected("toggled", cb)) {
            lock_to_source_button->connect("toggled", cb);
        }
    }
}

void CWDemodulator::configure_slots() {
    clear_all_slots();
    set_slot(0, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(3, true, SIGNAL_BASEBAND, signal_color(SIGNAL_BASEBAND), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(4, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO));
    set_slot(5, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
}

void CWDemodulator::ensure_ui() {
    if (has_node(NodePath("OffsetRow/OffsetOverlay/OffsetPanel/OffsetSelector")) &&
            has_node(NodePath("OffsetRow/OffsetOverlay/TuneToSourceButton")) &&
            has_node(NodePath("OffsetRow/OffsetOverlay/LockToSourceButton")) &&
            has_node(NodePath("BandwidthRow/BandwidthPanel/BandwidthSpin")) &&
            has_node(NodePath("ToneRow/TonePanel/ToneSpin")) &&
            has_node(NodePath("OutputVolumeRow/OutputVolumePanel/OutputVolumeSpin"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("CW Demodulator");
    set_custom_minimum_size(Vector2(400, 300));
    set_resizable(true);

    VBoxContainer *offset_row = memnew(VBoxContainer);
    offset_row->set_name("OffsetRow");
    add_child(offset_row);

    Control *offset_overlay = memnew(Control);
    offset_overlay->set_name("OffsetOverlay");
    offset_overlay->set_custom_minimum_size(Vector2(0, 56));
    offset_overlay->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    offset_row->add_child(offset_overlay);

    PanelContainer *offset_panel = memnew(PanelContainer);
    offset_panel->set_name("OffsetPanel");
    offset_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    offset_panel->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    offset_overlay->add_child(offset_panel);

    DigitNumberSelector *offset = memnew(DigitNumberSelector);
    offset->set_name("OffsetSelector");
    offset->set_limits(-999000000000LL, 999000000000LL);
    offset->set_digit_count(12);
    offset->set_group_size(3);
    offset->set_show_group_labels(true);
    offset->set_show_separators(true);
    offset->set_suffix_text("");
    PackedStringArray labels;
    labels.push_back("GHz");
    labels.push_back("MHz");
    labels.push_back("kHz");
    labels.push_back("Hz");
    offset->set_group_labels(labels);
    offset->set_value_no_signal(static_cast<int64_t>(std::llround(offset_hz)));
    offset_panel->add_child(offset);

    Button *tune_to_source = memnew(Button);
    tune_to_source->set_name("TuneToSourceButton");
    tune_to_source->set_text("Tune");
    tune_to_source->set_flat(true);
    tune_to_source->set_tooltip_text("Tune source to this demodulator center");
    tune_to_source->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT);
    tune_to_source->set_position(Vector2(-52, 0));
    tune_to_source->set_size(Vector2(48, 20));
    offset_overlay->add_child(tune_to_source);

    Button *lock_to_source = memnew(Button);
    lock_to_source->set_name("LockToSourceButton");
    lock_to_source->set_text("Absolute");
    lock_to_source->set_toggle_mode(true);
    lock_to_source->set_flat(true);
    lock_to_source->set_tooltip_text("Keep absolute center frequency while source retunes");
    lock_to_source->set_pressed(lock_to_source_frequency);
    lock_to_source->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT);
    lock_to_source->set_position(Vector2(-52, 20));
    lock_to_source->set_size(Vector2(48, 20));
    offset_overlay->add_child(lock_to_source);

    HBoxContainer *bandwidth_row = memnew(HBoxContainer);
    bandwidth_row->set_name("BandwidthRow");
    add_child(bandwidth_row);

    Label *bandwidth_label = memnew(Label);
    bandwidth_label->set_name("BandwidthLabel");
    bandwidth_label->set_text("Bandwidth");
    bandwidth_row->add_child(bandwidth_label);

    PanelContainer *bandwidth_panel = memnew(PanelContainer);
    bandwidth_panel->set_name("BandwidthPanel");
    bandwidth_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    bandwidth_row->add_child(bandwidth_panel);

    SpinBox *bandwidth = memnew(SpinBox);
    bandwidth->set_name("BandwidthSpin");
    bandwidth->set_min(100.0);
    bandwidth->set_max(5000.0);
    bandwidth->set_step(10.0);
    bandwidth->set_value(bandwidth_hz);
    bandwidth->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    bandwidth_panel->add_child(bandwidth);

    HBoxContainer *tone_row = memnew(HBoxContainer);
    tone_row->set_name("ToneRow");
    add_child(tone_row);

    Label *tone_label = memnew(Label);
    tone_label->set_name("ToneLabel");
    tone_label->set_text("Tone");
    tone_row->add_child(tone_label);

    PanelContainer *tone_panel = memnew(PanelContainer);
    tone_panel->set_name("TonePanel");
    tone_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    tone_row->add_child(tone_panel);

    SpinBox *tone = memnew(SpinBox);
    tone->set_name("ToneSpin");
    tone->set_min(100.0);
    tone->set_max(3000.0);
    tone->set_step(10.0);
    tone->set_value(tone_hz);
    tone->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    tone_panel->add_child(tone);

    HBoxContainer *output_row = memnew(HBoxContainer);
    output_row->set_name("OutputVolumeRow");
    add_child(output_row);

    Label *output_label = memnew(Label);
    output_label->set_name("OutputVolumeLabel");
    output_label->set_text("Output Volume");
    output_row->add_child(output_label);

    PanelContainer *output_panel = memnew(PanelContainer);
    output_panel->set_name("OutputVolumePanel");
    output_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    output_row->add_child(output_panel);

    SpinBox *output_spin = memnew(SpinBox);
    output_spin->set_name("OutputVolumeSpin");
    output_spin->set_min(0.0);
    output_spin->set_max(4.0);
    output_spin->set_step(0.01);
    output_spin->set_value(output_volume);
    output_spin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    output_panel->add_child(output_spin);

    Label *baseband = memnew(Label);
    baseband->set_name("LabelBaseband");
    baseband->set_text("Baseband");
    add_child(baseband);

    Label *audio = memnew(Label);
    audio->set_name("LabelAudio");
    audio->set_text("Audio");
    audio->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    add_child(audio);

    configure_slots();
}
