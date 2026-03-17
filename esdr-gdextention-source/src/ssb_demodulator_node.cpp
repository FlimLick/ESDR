#include "ssb_demodulator_node.h"

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

void SSBDemodulator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_offset_changed", "value"), &SSBDemodulator::_on_offset_changed);
    ClassDB::bind_method(D_METHOD("_on_bandwidth_changed", "value"), &SSBDemodulator::_on_bandwidth_changed);
    ClassDB::bind_method(D_METHOD("_on_output_volume_changed", "value"), &SSBDemodulator::_on_output_volume_changed);
    ClassDB::bind_method(D_METHOD("_on_tune_to_source_pressed"), &SSBDemodulator::_on_tune_to_source_pressed);
    ClassDB::bind_method(D_METHOD("_on_lock_to_source_toggled", "enabled"), &SSBDemodulator::_on_lock_to_source_toggled);

    ClassDB::bind_method(D_METHOD("set_offset_hz", "value"), &SSBDemodulator::set_offset_hz);
    ClassDB::bind_method(D_METHOD("get_offset_hz"), &SSBDemodulator::get_offset_hz);

    ClassDB::bind_method(D_METHOD("set_bandwidth_hz", "value"), &SSBDemodulator::set_bandwidth_hz);
    ClassDB::bind_method(D_METHOD("get_bandwidth_hz"), &SSBDemodulator::get_bandwidth_hz);
    ClassDB::bind_method(D_METHOD("set_output_volume", "value"), &SSBDemodulator::set_output_volume);
    ClassDB::bind_method(D_METHOD("get_output_volume"), &SSBDemodulator::get_output_volume);
    ClassDB::bind_method(D_METHOD("set_upstream_tuned_frequency_hz", "frequency_hz"), &SSBDemodulator::set_upstream_tuned_frequency_hz);
    ClassDB::bind_method(D_METHOD("set_lock_to_source_frequency", "enabled"), &SSBDemodulator::set_lock_to_source_frequency);
    ClassDB::bind_method(D_METHOD("get_lock_to_source_frequency"), &SSBDemodulator::get_lock_to_source_frequency);

    ClassDB::bind_method(D_METHOD("set_input_sample_rate_hz", "value"), &SSBDemodulator::set_input_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("get_input_sample_rate_hz"), &SSBDemodulator::get_input_sample_rate_hz);

    ClassDB::bind_method(D_METHOD("set_demod_mode", "mode"), &SSBDemodulator::set_demod_mode);
    ClassDB::bind_method(D_METHOD("get_demod_mode"), &SSBDemodulator::get_demod_mode);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &SSBDemodulator::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &SSBDemodulator::set_port_value);
    ClassDB::bind_method(D_METHOD("push_baseband_frame", "frame"), &SSBDemodulator::push_baseband_frame);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "offset_hz", PROPERTY_HINT_RANGE, "-999000000000,999000000000,1"), "set_offset_hz", "get_offset_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bandwidth_hz", PROPERTY_HINT_RANGE, "300,15000,1"), "set_bandwidth_hz", "get_bandwidth_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "output_volume", PROPERTY_HINT_RANGE, "0,4,0.01"), "set_output_volume", "get_output_volume");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lock_to_source_frequency"), "set_lock_to_source_frequency", "get_lock_to_source_frequency");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "input_sample_rate_hz", PROPERTY_HINT_RANGE, "1000,120000000,1"), "set_input_sample_rate_hz", "get_input_sample_rate_hz");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "demod_mode", PROPERTY_HINT_ENUM, "USB,LSB,DSB"), "set_demod_mode", "get_demod_mode");
    ADD_SIGNAL(MethodInfo("audio_frame", PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "frame")));
    ADD_SIGNAL(MethodInfo("passthrough_frequency_request", PropertyInfo(Variant::INT, "frequency_hz")));
    ADD_SIGNAL(MethodInfo("offset_user_changed", PropertyInfo(Variant::INT, "offset_hz")));

    BIND_ENUM_CONSTANT(DEMOD_USB);
    BIND_ENUM_CONSTANT(DEMOD_LSB);
    BIND_ENUM_CONSTANT(DEMOD_DSB);
}

void SSBDemodulator::_notification(int32_t p_what) {
    if (p_what != NOTIFICATION_READY) {
        return;
    }

    ensure_ui();
    cache_ui_refs();
    bind_ui();
    refresh_title();
    refresh_dsp_config();
}

void SSBDemodulator::set_offset_hz(double p_value) {
    offset_hz = std::clamp(p_value, -999000000000.0, 999000000000.0);
    if (offset_selector != nullptr) {
        offset_selector->set_value_no_signal(static_cast<int64_t>(std::llround(offset_hz)));
    }
    dsp_config_dirty = true;
}

double SSBDemodulator::get_offset_hz() const {
    return offset_hz;
}

void SSBDemodulator::set_bandwidth_hz(double p_value) {
    bandwidth_hz = std::clamp(p_value, 300.0, 15000.0);
    if (bandwidth_spin != nullptr) {
        bandwidth_spin->set_value_no_signal(bandwidth_hz);
    }
    dsp_config_dirty = true;
}

double SSBDemodulator::get_bandwidth_hz() const {
    return bandwidth_hz;
}

void SSBDemodulator::set_output_volume(double p_value) {
    output_volume = std::clamp(p_value, 0.0, 4.0);
    if (output_volume_spin != nullptr) {
        output_volume_spin->set_value_no_signal(output_volume);
    }
}

double SSBDemodulator::get_output_volume() const {
    return output_volume;
}

void SSBDemodulator::set_lock_to_source_frequency(bool p_enabled) {
    lock_to_source_frequency = p_enabled;
    if (lock_to_source_button != nullptr) {
        lock_to_source_button->set_pressed_no_signal(lock_to_source_frequency);
    }
}

bool SSBDemodulator::get_lock_to_source_frequency() const {
    return lock_to_source_frequency;
}

void SSBDemodulator::set_input_sample_rate_hz(double p_value) {
    input_sample_rate_hz = std::clamp(p_value, 1000.0, 120000000.0);
    dsp_config_dirty = true;
}

double SSBDemodulator::get_input_sample_rate_hz() const {
    return input_sample_rate_hz;
}

void SSBDemodulator::set_upstream_tuned_frequency_hz(int64_t p_frequency_hz) {
    upstream_tuned_frequency_hz = p_frequency_hz;
}

void SSBDemodulator::set_demod_mode(int32_t p_mode) {
    demod_mode = std::clamp<int32_t>(p_mode, DEMOD_USB, DEMOD_DSB);
    refresh_title();
    dsp_config_dirty = true;
}

int32_t SSBDemodulator::get_demod_mode() const {
    return demod_mode;
}

void SSBDemodulator::_on_offset_changed(int64_t p_value) {
    set_offset_hz(static_cast<double>(p_value));
    emit_signal("offset_user_changed", static_cast<int64_t>(std::llround(offset_hz)));
}

void SSBDemodulator::_on_bandwidth_changed(double p_value) {
    set_bandwidth_hz(p_value);
}

void SSBDemodulator::_on_output_volume_changed(double p_value) {
    set_output_volume(p_value);
}

void SSBDemodulator::_on_tune_to_source_pressed() {
    const int64_t target_hz = upstream_tuned_frequency_hz + static_cast<int64_t>(std::llround(offset_hz));
    emit_signal("passthrough_frequency_request", target_hz);
}

void SSBDemodulator::_on_lock_to_source_toggled(bool p_enabled) {
    set_lock_to_source_frequency(p_enabled);
}

void SSBDemodulator::refresh_dsp_config() {
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

    esdr_demod::SSBParams params;
    params.input_sample_rate_hz = input_sample_rate_hz;
    params.audio_sample_rate_hz = audio_mix_rate;
    params.offset_hz = offset_hz;
    params.rf_lowpass_hz = std::clamp(bandwidth_hz * 0.60, 300.0, 12000.0);
    params.audio_lowpass_hz = std::clamp(bandwidth_hz * 0.55, 300.0, 8000.0);
    params.mode = static_cast<esdr_demod::SSBParams::Mode>(demod_mode);
    demod_core.configure(params);

    dsp_config_dirty = false;
}

void SSBDemodulator::push_baseband_frame(const PackedVector2Array &p_frame) {
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

Variant SSBDemodulator::get_port_value(int64_t p_port) const {
    switch (p_port) {
        case 0:
            return static_cast<int64_t>(std::llround(offset_hz));
        case 1:
            return static_cast<int64_t>(std::llround(bandwidth_hz));
        case 4:
            return output_volume;
        default:
            return Variant();
    }
}

void SSBDemodulator::set_port_value(int64_t p_port, const Variant &p_value) {
    switch (p_port) {
        case 0:
            set_offset_hz(static_cast<double>(p_value));
            break;
        case 1:
            set_bandwidth_hz(static_cast<double>(p_value));
            break;
        case 4:
            set_output_volume(static_cast<double>(p_value));
            break;
        case 2:
            if (p_value.get_type() == Variant::PACKED_VECTOR2_ARRAY) {
                push_baseband_frame(p_value);
            }
            break;
        default:
            break;
    }
}

void SSBDemodulator::cache_ui_refs() {
    offset_selector = Object::cast_to<DigitNumberSelector>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/OffsetPanel/OffsetSelector")));
    tune_to_source_button = Object::cast_to<Button>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/TuneToSourceButton")));
    lock_to_source_button = Object::cast_to<Button>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/LockToSourceButton")));
    bandwidth_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("BandwidthRow/BandwidthPanel/BandwidthSpin")));
    output_volume_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("OutputVolumeRow/OutputVolumePanel/OutputVolumeSpin")));
}

void SSBDemodulator::bind_ui() {
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

void SSBDemodulator::refresh_title() {
    switch (demod_mode) {
        case DEMOD_LSB:
            set_title("LSB Demodulator");
            break;
        case DEMOD_DSB:
            set_title("DSB Demodulator");
            break;
        case DEMOD_USB:
        default:
            set_title("USB Demodulator");
            break;
    }
}

void SSBDemodulator::configure_slots() {
    clear_all_slots();
    set_slot(0, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, true, SIGNAL_BASEBAND, signal_color(SIGNAL_BASEBAND), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(3, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO));
    set_slot(4, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
}

void SSBDemodulator::ensure_ui() {
    if (has_node(NodePath("OffsetRow/OffsetOverlay/OffsetPanel/OffsetSelector")) &&
            has_node(NodePath("OffsetRow/OffsetOverlay/TuneToSourceButton")) &&
            has_node(NodePath("OffsetRow/OffsetOverlay/LockToSourceButton")) &&
            has_node(NodePath("BandwidthRow/BandwidthPanel/BandwidthSpin")) &&
            has_node(NodePath("OutputVolumeRow/OutputVolumePanel/OutputVolumeSpin"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_custom_minimum_size(Vector2(400, 300));
    set_resizable(true);
    refresh_title();

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
    bandwidth->set_min(300.0);
    bandwidth->set_max(15000.0);
    bandwidth->set_step(50.0);
    bandwidth->set_value(bandwidth_hz);
    bandwidth->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    bandwidth_panel->add_child(bandwidth);

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
