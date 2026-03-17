#include "am_demodulator_node.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace {
void log_am(const String &p_message) {
    UtilityFunctions::print(String("[ESDR][AM] ") + p_message);
}
} // namespace

void AMDemodulator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_offset_changed", "value"), &AMDemodulator::_on_offset_changed);
    ClassDB::bind_method(D_METHOD("_on_bandwidth_changed", "value"), &AMDemodulator::_on_bandwidth_changed);
    ClassDB::bind_method(D_METHOD("_on_output_volume_changed", "value"), &AMDemodulator::_on_output_volume_changed);
    ClassDB::bind_method(D_METHOD("_on_tune_to_source_pressed"), &AMDemodulator::_on_tune_to_source_pressed);
    ClassDB::bind_method(D_METHOD("_on_lock_to_source_toggled", "enabled"), &AMDemodulator::_on_lock_to_source_toggled);

    ClassDB::bind_method(D_METHOD("set_offset_hz", "value"), &AMDemodulator::set_offset_hz);
    ClassDB::bind_method(D_METHOD("get_offset_hz"), &AMDemodulator::get_offset_hz);

    ClassDB::bind_method(D_METHOD("set_bandwidth_hz", "value"), &AMDemodulator::set_bandwidth_hz);
    ClassDB::bind_method(D_METHOD("get_bandwidth_hz"), &AMDemodulator::get_bandwidth_hz);
    ClassDB::bind_method(D_METHOD("set_output_volume", "value"), &AMDemodulator::set_output_volume);
    ClassDB::bind_method(D_METHOD("get_output_volume"), &AMDemodulator::get_output_volume);
    ClassDB::bind_method(D_METHOD("set_lock_to_source_frequency", "enabled"), &AMDemodulator::set_lock_to_source_frequency);
    ClassDB::bind_method(D_METHOD("get_lock_to_source_frequency"), &AMDemodulator::get_lock_to_source_frequency);
    ClassDB::bind_method(D_METHOD("set_debug_logging_enabled", "enabled"), &AMDemodulator::set_debug_logging_enabled);
    ClassDB::bind_method(D_METHOD("get_debug_logging_enabled"), &AMDemodulator::get_debug_logging_enabled);

    ClassDB::bind_method(D_METHOD("set_input_sample_rate_hz", "value"), &AMDemodulator::set_input_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("get_input_sample_rate_hz"), &AMDemodulator::get_input_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("set_upstream_tuned_frequency_hz", "frequency_hz"), &AMDemodulator::set_upstream_tuned_frequency_hz);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &AMDemodulator::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &AMDemodulator::set_port_value);
    ClassDB::bind_method(D_METHOD("push_baseband_frame", "frame"), &AMDemodulator::push_baseband_frame);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "offset_hz", PROPERTY_HINT_RANGE, "-999000000000,999000000000,1"), "set_offset_hz", "get_offset_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bandwidth_hz", PROPERTY_HINT_RANGE, "2000,50000,1"), "set_bandwidth_hz", "get_bandwidth_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "output_volume", PROPERTY_HINT_RANGE, "0,4,0.01"), "set_output_volume", "get_output_volume");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lock_to_source_frequency"), "set_lock_to_source_frequency", "get_lock_to_source_frequency");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_logging_enabled"), "set_debug_logging_enabled", "get_debug_logging_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "input_sample_rate_hz", PROPERTY_HINT_RANGE, "1000,120000000,1"), "set_input_sample_rate_hz", "get_input_sample_rate_hz");
    ADD_SIGNAL(MethodInfo("audio_frame", PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "frame")));
    ADD_SIGNAL(MethodInfo("passthrough_frequency_request", PropertyInfo(Variant::INT, "frequency_hz")));
    ADD_SIGNAL(MethodInfo("offset_user_changed", PropertyInfo(Variant::INT, "offset_hz")));
}

void AMDemodulator::_notification(int32_t p_what) {
    if (p_what != NOTIFICATION_READY) {
        return;
    }

    ensure_ui();
    cache_ui_refs();
    bind_ui();
    refresh_dsp_config();
}

void AMDemodulator::set_offset_hz(double p_value) {
    offset_hz = std::clamp(p_value, -999000000000.0, 999000000000.0);
    if (offset_selector != nullptr) {
        offset_selector->set_value_no_signal(static_cast<int64_t>(std::llround(offset_hz)));
    }
    dsp_config_dirty = true;
}

double AMDemodulator::get_offset_hz() const {
    return offset_hz;
}

void AMDemodulator::set_bandwidth_hz(double p_value) {
    bandwidth_hz = std::clamp(p_value, 2000.0, 50000.0);
    if (bandwidth_spin != nullptr) {
        bandwidth_spin->set_value_no_signal(bandwidth_hz);
    }
    dsp_config_dirty = true;
}

double AMDemodulator::get_bandwidth_hz() const {
    return bandwidth_hz;
}

void AMDemodulator::set_output_volume(double p_value) {
    output_volume = std::clamp(p_value, 0.0, 4.0);
    if (output_volume_spin != nullptr) {
        output_volume_spin->set_value_no_signal(output_volume);
    }
}

double AMDemodulator::get_output_volume() const {
    return output_volume;
}

void AMDemodulator::set_lock_to_source_frequency(bool p_enabled) {
    lock_to_source_frequency = p_enabled;
    if (lock_to_source_button != nullptr) {
        lock_to_source_button->set_pressed_no_signal(lock_to_source_frequency);
    }
}

bool AMDemodulator::get_lock_to_source_frequency() const {
    return lock_to_source_frequency;
}

void AMDemodulator::set_debug_logging_enabled(bool p_enabled) {
    debug_logging_enabled = p_enabled;
}

bool AMDemodulator::get_debug_logging_enabled() const {
    return debug_logging_enabled;
}

void AMDemodulator::set_input_sample_rate_hz(double p_value) {
    input_sample_rate_hz = std::clamp(p_value, 1000.0, 120000000.0);
    dsp_config_dirty = true;
}

void AMDemodulator::set_upstream_tuned_frequency_hz(int64_t p_frequency_hz) {
    upstream_tuned_frequency_hz = p_frequency_hz;
}

double AMDemodulator::get_input_sample_rate_hz() const {
    return input_sample_rate_hz;
}

void AMDemodulator::_on_offset_changed(int64_t p_value) {
    set_offset_hz(static_cast<double>(p_value));
    emit_signal("offset_user_changed", static_cast<int64_t>(std::llround(offset_hz)));
}

void AMDemodulator::_on_bandwidth_changed(double p_value) {
    set_bandwidth_hz(p_value);
}

void AMDemodulator::_on_output_volume_changed(double p_value) {
    set_output_volume(p_value);
}

void AMDemodulator::_on_tune_to_source_pressed() {
    const int64_t target_hz = upstream_tuned_frequency_hz + static_cast<int64_t>(std::llround(offset_hz));
    emit_signal("passthrough_frequency_request", target_hz);
}

void AMDemodulator::_on_lock_to_source_toggled(bool p_enabled) {
    set_lock_to_source_frequency(p_enabled);
}

void AMDemodulator::refresh_dsp_config() {
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

    esdr_demod::AMParams params;
    params.input_sample_rate_hz = input_sample_rate_hz;
    params.audio_sample_rate_hz = audio_mix_rate;
    params.offset_hz = offset_hz;
    params.rf_lowpass_hz = std::clamp(bandwidth_hz * 0.50, 1500.0, 30000.0);
    params.audio_lowpass_hz = std::clamp(bandwidth_hz * 0.45, 1200.0, 9000.0);
    params.agc_enabled = true;
    params.agc_target_level = 0.34;
    params.agc_attack_seconds = 0.010;
    params.agc_release_seconds = 0.280;
    demod_core.configure(params);

    dsp_config_dirty = false;
}

void AMDemodulator::push_baseband_frame(const PackedVector2Array &p_frame) {
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    if (p_frame.is_empty()) {
        return;
    }

    debug_in_samples += p_frame.size();
    for (int32_t i = 0; i < p_frame.size(); i += 8) {
        const Vector2 iq = p_frame[i];
        debug_iq_peak = std::max(debug_iq_peak, static_cast<float>(std::max(std::abs(iq.x), std::abs(iq.y))));
    }

    refresh_dsp_config();

    PackedFloat32Array audio;
    demod_core.process_frame(p_frame, audio);
    if (!audio.is_empty()) {
        for (int32_t i = 0; i < audio.size(); i++) {
            audio[i] = std::clamp(static_cast<float>(audio[i] * static_cast<float>(output_volume)), -1.0f, 1.0f);
        }
        debug_out_samples += audio.size();
        for (int32_t i = 0; i < audio.size(); i++) {
            const float sample = audio[i];
            const float abs_value = std::abs(sample);
            debug_out_peak = std::max(debug_out_peak, abs_value);
            if (abs_value >= 0.995f) {
                debug_out_clip_samples++;
            }
        }
        emit_signal("audio_frame", audio);
    }

    log_debug_stats_if_due();
}

Variant AMDemodulator::get_port_value(int64_t p_port) const {
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

void AMDemodulator::set_port_value(int64_t p_port, const Variant &p_value) {
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

void AMDemodulator::cache_ui_refs() {
    offset_selector = Object::cast_to<DigitNumberSelector>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/OffsetPanel/OffsetSelector")));
    tune_to_source_button = Object::cast_to<Button>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/TuneToSourceButton")));
    lock_to_source_button = Object::cast_to<Button>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/LockToSourceButton")));
    bandwidth_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("BandwidthRow/BandwidthPanel/BandwidthSpin")));
    output_volume_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("OutputVolumeRow/OutputVolumePanel/OutputVolumeSpin")));
}

void AMDemodulator::bind_ui() {
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
        tune_to_source_button->set_position(Vector2(-52, -4));
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

void AMDemodulator::log_debug_stats_if_due() {
    if (!debug_logging_enabled) {
        debug_last_log_us = 0;
        debug_in_samples = 0;
        debug_out_samples = 0;
        debug_out_clip_samples = 0;
        debug_iq_peak = 0.0f;
        debug_out_peak = 0.0f;
        return;
    }

    Time *time = Time::get_singleton();
    if (time == nullptr) {
        return;
    }
    const uint64_t now_us = time->get_ticks_usec();
    if (debug_last_log_us == 0) {
        debug_last_log_us = now_us;
        return;
    }

    const double dt_sec = static_cast<double>(now_us - debug_last_log_us) * 1e-6;
    if (dt_sec < 1.0) {
        return;
    }
    debug_last_log_us = now_us;

    const double in_sps = static_cast<double>(debug_in_samples) / dt_sec;
    const double out_sps = static_cast<double>(debug_out_samples) / dt_sec;
    log_am(String("stats dt=") + String::num(dt_sec, 3) +
            " in_sps=" + String::num(in_sps, 1) +
            " out_sps=" + String::num(out_sps, 1) +
            " iq_peak=" + String::num(debug_iq_peak, 4) +
            " out_peak=" + String::num(debug_out_peak, 4) +
            " out_clip=" + String::num_int64(debug_out_clip_samples) +
            " sr=" + String::num(input_sample_rate_hz, 1) +
            " bw=" + String::num(bandwidth_hz, 1));

    debug_in_samples = 0;
    debug_out_samples = 0;
    debug_out_clip_samples = 0;
    debug_iq_peak = 0.0f;
    debug_out_peak = 0.0f;
}

void AMDemodulator::configure_slots() {
    clear_all_slots();
    set_slot(0, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, true, SIGNAL_BASEBAND, signal_color(SIGNAL_BASEBAND), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(3, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO));
    set_slot(4, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
}

void AMDemodulator::ensure_ui() {
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

    set_title("AM Demodulator");
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
    tune_to_source->set_tooltip_text("Tune source to this demodulator frequency");
    tune_to_source->set_anchors_and_offsets_preset(Control::PRESET_TOP_RIGHT);
    tune_to_source->set_position(Vector2(-52, -4));
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
    bandwidth->set_min(2000.0);
    bandwidth->set_max(50000.0);
    bandwidth->set_step(100.0);
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
