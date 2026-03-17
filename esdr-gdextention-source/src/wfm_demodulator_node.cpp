#include "wfm_demodulator_node.h"

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
void log_wfm(const String &p_message) {
    UtilityFunctions::print(String("[ESDR][WFM] ") + p_message);
}
} // namespace

void WFMDemodulator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_offset_changed", "value"), &WFMDemodulator::_on_offset_changed);
    ClassDB::bind_method(D_METHOD("_on_bandwidth_changed", "value"), &WFMDemodulator::_on_bandwidth_changed);
    ClassDB::bind_method(D_METHOD("_on_output_volume_changed", "value"), &WFMDemodulator::_on_output_volume_changed);
    ClassDB::bind_method(D_METHOD("_on_tune_to_source_pressed"), &WFMDemodulator::_on_tune_to_source_pressed);
    ClassDB::bind_method(D_METHOD("_on_lock_to_source_toggled", "enabled"), &WFMDemodulator::_on_lock_to_source_toggled);
    ClassDB::bind_method(D_METHOD("_on_stereo_toggled", "enabled"), &WFMDemodulator::_on_stereo_toggled);
    ClassDB::bind_method(D_METHOD("_on_deemphasis_selected", "index"), &WFMDemodulator::_on_deemphasis_selected);

    ClassDB::bind_method(D_METHOD("set_offset_hz", "value"), &WFMDemodulator::set_offset_hz);
    ClassDB::bind_method(D_METHOD("get_offset_hz"), &WFMDemodulator::get_offset_hz);

    ClassDB::bind_method(D_METHOD("set_bandwidth_hz", "value"), &WFMDemodulator::set_bandwidth_hz);
    ClassDB::bind_method(D_METHOD("get_bandwidth_hz"), &WFMDemodulator::get_bandwidth_hz);
    ClassDB::bind_method(D_METHOD("set_output_volume", "value"), &WFMDemodulator::set_output_volume);
    ClassDB::bind_method(D_METHOD("get_output_volume"), &WFMDemodulator::get_output_volume);
    ClassDB::bind_method(D_METHOD("set_lock_to_source_frequency", "enabled"), &WFMDemodulator::set_lock_to_source_frequency);
    ClassDB::bind_method(D_METHOD("get_lock_to_source_frequency"), &WFMDemodulator::get_lock_to_source_frequency);
    ClassDB::bind_method(D_METHOD("set_debug_logging_enabled", "enabled"), &WFMDemodulator::set_debug_logging_enabled);
    ClassDB::bind_method(D_METHOD("get_debug_logging_enabled"), &WFMDemodulator::get_debug_logging_enabled);

    ClassDB::bind_method(D_METHOD("set_input_sample_rate_hz", "value"), &WFMDemodulator::set_input_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("get_input_sample_rate_hz"), &WFMDemodulator::get_input_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("set_upstream_tuned_frequency_hz", "frequency_hz"), &WFMDemodulator::set_upstream_tuned_frequency_hz);
    ClassDB::bind_method(D_METHOD("set_stereo_enabled", "enabled"), &WFMDemodulator::set_stereo_enabled);
    ClassDB::bind_method(D_METHOD("get_stereo_enabled"), &WFMDemodulator::get_stereo_enabled);
    ClassDB::bind_method(D_METHOD("set_deemphasis_mode", "mode"), &WFMDemodulator::set_deemphasis_mode);
    ClassDB::bind_method(D_METHOD("get_deemphasis_mode"), &WFMDemodulator::get_deemphasis_mode);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &WFMDemodulator::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &WFMDemodulator::set_port_value);
    ClassDB::bind_method(D_METHOD("push_baseband_frame", "frame"), &WFMDemodulator::push_baseband_frame);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "offset_hz", PROPERTY_HINT_RANGE, "-999000000000,999000000000,1"), "set_offset_hz", "get_offset_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bandwidth_hz", PROPERTY_HINT_RANGE, "50000,300000,1"), "set_bandwidth_hz", "get_bandwidth_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "output_volume", PROPERTY_HINT_RANGE, "0,4,0.01"), "set_output_volume", "get_output_volume");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lock_to_source_frequency"), "set_lock_to_source_frequency", "get_lock_to_source_frequency");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_logging_enabled"), "set_debug_logging_enabled", "get_debug_logging_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "input_sample_rate_hz", PROPERTY_HINT_RANGE, "1000,120000000,1"), "set_input_sample_rate_hz", "get_input_sample_rate_hz");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stereo_enabled"), "set_stereo_enabled", "get_stereo_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "deemphasis_mode", PROPERTY_HINT_ENUM, "Off,50us,75us"), "set_deemphasis_mode", "get_deemphasis_mode");
    ADD_SIGNAL(MethodInfo("audio_frame", PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "frame")));
    ADD_SIGNAL(MethodInfo("audio_stereo_frame", PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "frame")));
    ADD_SIGNAL(MethodInfo("passthrough_frequency_request", PropertyInfo(Variant::INT, "frequency_hz")));
    ADD_SIGNAL(MethodInfo("offset_user_changed", PropertyInfo(Variant::INT, "offset_hz")));
}

void WFMDemodulator::_notification(int32_t p_what) {
    if (p_what != NOTIFICATION_READY) {
        return;
    }

    ensure_ui();
    cache_ui_refs();
    bind_ui();
    refresh_dsp_config();
}

void WFMDemodulator::set_offset_hz(double p_value) {
    offset_hz = std::clamp(p_value, -999000000000.0, 999000000000.0);
    if (offset_selector != nullptr) {
        offset_selector->set_value_no_signal(static_cast<int64_t>(std::llround(offset_hz)));
    }
    dsp_config_dirty = true;
}

double WFMDemodulator::get_offset_hz() const {
    return offset_hz;
}

void WFMDemodulator::set_bandwidth_hz(double p_value) {
    bandwidth_hz = std::clamp(p_value, 50000.0, 300000.0);
    if (bandwidth_spin != nullptr) {
        bandwidth_spin->set_value_no_signal(bandwidth_hz);
    }
    dsp_config_dirty = true;
}

double WFMDemodulator::get_bandwidth_hz() const {
    return bandwidth_hz;
}

void WFMDemodulator::set_output_volume(double p_value) {
    output_volume = std::clamp(p_value, 0.0, 4.0);
    if (output_volume_spin != nullptr) {
        output_volume_spin->set_value_no_signal(output_volume);
    }
}

double WFMDemodulator::get_output_volume() const {
    return output_volume;
}

void WFMDemodulator::set_lock_to_source_frequency(bool p_enabled) {
    lock_to_source_frequency = p_enabled;
    if (lock_to_source_button != nullptr) {
        lock_to_source_button->set_pressed_no_signal(lock_to_source_frequency);
    }
}

bool WFMDemodulator::get_lock_to_source_frequency() const {
    return lock_to_source_frequency;
}

void WFMDemodulator::set_debug_logging_enabled(bool p_enabled) {
    debug_logging_enabled = p_enabled;
}

bool WFMDemodulator::get_debug_logging_enabled() const {
    return debug_logging_enabled;
}

void WFMDemodulator::set_input_sample_rate_hz(double p_value) {
    input_sample_rate_hz = std::clamp(p_value, 1000.0, 120000000.0);
    dsp_config_dirty = true;
}

void WFMDemodulator::set_upstream_tuned_frequency_hz(int64_t p_frequency_hz) {
    upstream_tuned_frequency_hz = p_frequency_hz;
}

double WFMDemodulator::get_input_sample_rate_hz() const {
    return input_sample_rate_hz;
}

void WFMDemodulator::set_stereo_enabled(bool p_enabled) {
    stereo_enabled = p_enabled;
    if (stereo_checkbox != nullptr) {
        stereo_checkbox->set_pressed_no_signal(stereo_enabled);
    }
    dsp_config_dirty = true;
}

bool WFMDemodulator::get_stereo_enabled() const {
    return stereo_enabled;
}

void WFMDemodulator::set_deemphasis_mode(int32_t p_mode) {
    deemphasis_mode = std::clamp<int32_t>(p_mode, 0, 2);
    if (deemphasis_selector != nullptr) {
        deemphasis_selector->select(deemphasis_mode);
    }
    dsp_config_dirty = true;
}

int32_t WFMDemodulator::get_deemphasis_mode() const {
    return deemphasis_mode;
}

void WFMDemodulator::_on_offset_changed(int64_t p_value) {
    set_offset_hz(static_cast<double>(p_value));
    emit_signal("offset_user_changed", static_cast<int64_t>(std::llround(offset_hz)));
}

void WFMDemodulator::_on_bandwidth_changed(double p_value) {
    set_bandwidth_hz(p_value);
}

void WFMDemodulator::_on_output_volume_changed(double p_value) {
    set_output_volume(p_value);
}

void WFMDemodulator::_on_tune_to_source_pressed() {
    const int64_t target_hz = upstream_tuned_frequency_hz + static_cast<int64_t>(std::llround(offset_hz));
    emit_signal("passthrough_frequency_request", target_hz);
}

void WFMDemodulator::_on_lock_to_source_toggled(bool p_enabled) {
    set_lock_to_source_frequency(p_enabled);
}

void WFMDemodulator::_on_stereo_toggled(bool p_enabled) {
    set_stereo_enabled(p_enabled);
}

void WFMDemodulator::_on_deemphasis_selected(int64_t p_index) {
    set_deemphasis_mode(static_cast<int32_t>(p_index));
}

void WFMDemodulator::refresh_dsp_config() {
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

    esdr_demod::WFMParams params;
    params.input_sample_rate_hz = input_sample_rate_hz;
    params.audio_sample_rate_hz = audio_mix_rate;
    params.offset_hz = offset_hz;
    // Broadcast FM baseline is 75 kHz max deviation; keep channel bandwidth separate.
    params.deviation_hz = 75000.0;
    params.rf_lowpass_hz = std::clamp(bandwidth_hz * 0.50, 100000.0, 220000.0);
    // Let bandwidth audibly control recovered audio width (mono + stereo branches).
    params.audio_lowpass_hz = std::clamp(bandwidth_hz * 0.11, 2500.0, 16000.0);
    params.deemphasis_enabled = deemphasis_mode != 0;
    params.deemphasis_tau_seconds = deemphasis_mode == 1 ? 50e-6 : 75e-6;
    params.stereo_enabled = stereo_enabled;
    demod_core.configure(params);

    dsp_config_dirty = false;
}

void WFMDemodulator::push_baseband_frame(const PackedVector2Array &p_frame) {
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

    PackedFloat32Array audio_mono;
    PackedVector2Array audio_stereo;
    demod_core.process_frame(p_frame, audio_mono, audio_stereo);
    if (!audio_stereo.is_empty()) {
        for (int32_t i = 0; i < audio_stereo.size(); i++) {
            const Vector2 sample = audio_stereo[i];
            audio_stereo.set(i, Vector2(std::clamp(sample.x * static_cast<float>(output_volume), -1.0f, 1.0f),
                                        std::clamp(sample.y * static_cast<float>(output_volume), -1.0f, 1.0f)));
        }
    }
    if (!audio_mono.is_empty()) {
        for (int32_t i = 0; i < audio_mono.size(); i++) {
            audio_mono[i] = std::clamp(audio_mono[i] * static_cast<float>(output_volume), -1.0f, 1.0f);
        }
    }
    if (audio_stereo.is_empty() && !audio_mono.is_empty()) {
        // Keep downstream stereo-only links alive when stereo decode is disabled.
        audio_stereo.resize(audio_mono.size());
        for (int32_t i = 0; i < audio_mono.size(); i++) {
            const float sample = audio_mono[i];
            audio_stereo.set(i, Vector2(sample, sample));
        }
    }
    if (!audio_stereo.is_empty()) {
        debug_out_stereo_frames += audio_stereo.size();
        for (int32_t i = 0; i < audio_stereo.size(); i++) {
            const Vector2 pair = audio_stereo[i];
            const float peak = static_cast<float>(std::max(std::abs(pair.x), std::abs(pair.y)));
            debug_out_peak = std::max(debug_out_peak, peak);
            if (peak >= 0.995f) {
                debug_out_clip_samples++;
            }
        }
        emit_signal("audio_stereo_frame", audio_stereo);
    }
    if (!audio_mono.is_empty()) {
        debug_out_mono_samples += audio_mono.size();
        for (int32_t i = 0; i < audio_mono.size(); i++) {
            const float peak = std::abs(audio_mono[i]);
            debug_out_peak = std::max(debug_out_peak, peak);
            if (peak >= 0.995f) {
                debug_out_clip_samples++;
            }
        }
        emit_signal("audio_frame", audio_mono);
    }

    log_debug_stats_if_due();
}

Variant WFMDemodulator::get_port_value(int64_t p_port) const {
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

void WFMDemodulator::set_port_value(int64_t p_port, const Variant &p_value) {
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

void WFMDemodulator::cache_ui_refs() {
    offset_selector = Object::cast_to<DigitNumberSelector>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/OffsetPanel/OffsetSelector")));
    tune_to_source_button = Object::cast_to<Button>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/TuneToSourceButton")));
    lock_to_source_button = Object::cast_to<Button>(get_node_or_null(NodePath("OffsetRow/OffsetOverlay/LockToSourceButton")));
    bandwidth_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("BandwidthRow/BandwidthPanel/BandwidthSpin")));
    output_volume_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("OutputVolumeRow/OutputVolumePanel/OutputVolumeSpin")));
    stereo_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("StereoRow/StereoCheckBox")));
    deemphasis_selector = Object::cast_to<OptionButton>(get_node_or_null(NodePath("DeemphasisRow/DeemphasisSelector")));
}

void WFMDemodulator::bind_ui() {
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

    if (stereo_checkbox != nullptr) {
        stereo_checkbox->set_pressed_no_signal(stereo_enabled);
        const Callable cb(this, "_on_stereo_toggled");
        if (!stereo_checkbox->is_connected("toggled", cb)) {
            stereo_checkbox->connect("toggled", cb);
        }
    }

    if (deemphasis_selector != nullptr) {
        deemphasis_selector->select(deemphasis_mode);
        const Callable cb(this, "_on_deemphasis_selected");
        if (!deemphasis_selector->is_connected("item_selected", cb)) {
            deemphasis_selector->connect("item_selected", cb);
        }
    }
}

void WFMDemodulator::log_debug_stats_if_due() {
    if (!debug_logging_enabled) {
        debug_last_log_us = 0;
        debug_in_samples = 0;
        debug_out_mono_samples = 0;
        debug_out_stereo_frames = 0;
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
    const double mono_out_sps = static_cast<double>(debug_out_mono_samples) / dt_sec;
    const double stereo_frame_sps = static_cast<double>(debug_out_stereo_frames) / dt_sec;
    log_wfm(String("stats dt=") + String::num(dt_sec, 3) +
            " in_sps=" + String::num(in_sps, 1) +
            " out_mono_sps=" + String::num(mono_out_sps, 1) +
            " out_stereo_fps=" + String::num(stereo_frame_sps, 1) +
            " iq_peak=" + String::num(debug_iq_peak, 4) +
            " out_peak=" + String::num(debug_out_peak, 4) +
            " out_clip=" + String::num_int64(debug_out_clip_samples) +
            " sr=" + String::num(input_sample_rate_hz, 1) +
            " bw=" + String::num(bandwidth_hz, 1) +
            " deemp=" + String::num_int64(deemphasis_mode) +
            " stereo=" + (stereo_enabled ? String("true") : String("false")));

    debug_in_samples = 0;
    debug_out_mono_samples = 0;
    debug_out_stereo_frames = 0;
    debug_out_clip_samples = 0;
    debug_iq_peak = 0.0f;
    debug_out_peak = 0.0f;
}

void WFMDemodulator::configure_slots() {
    clear_all_slots();
    set_slot(0, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, true, SIGNAL_BASEBAND, signal_color(SIGNAL_BASEBAND), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(3, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO));
    set_slot(4, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
}

void WFMDemodulator::ensure_ui() {
    if (has_node(NodePath("OffsetRow/OffsetOverlay/OffsetPanel/OffsetSelector")) &&
            has_node(NodePath("OffsetRow/OffsetOverlay/TuneToSourceButton")) &&
            has_node(NodePath("OffsetRow/OffsetOverlay/LockToSourceButton")) &&
            has_node(NodePath("BandwidthRow/BandwidthPanel/BandwidthSpin")) &&
            has_node(NodePath("OutputVolumeRow/OutputVolumePanel/OutputVolumeSpin")) &&
            has_node(NodePath("StereoRow/StereoCheckBox")) &&
            has_node(NodePath("DeemphasisRow/DeemphasisSelector"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("WFM Demodulator");
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
    bandwidth->set_min(50000.0);
    bandwidth->set_max(300000.0);
    bandwidth->set_step(1000.0);
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

    HBoxContainer *stereo_row = memnew(HBoxContainer);
    stereo_row->set_name("StereoRow");
    add_child(stereo_row);

    CheckBox *stereo = memnew(CheckBox);
    stereo->set_name("StereoCheckBox");
    stereo->set_text("Stereo");
    stereo->set_pressed(stereo_enabled);
    stereo_row->add_child(stereo);

    HBoxContainer *deemphasis_row = memnew(HBoxContainer);
    deemphasis_row->set_name("DeemphasisRow");
    add_child(deemphasis_row);

    Label *deemphasis_label = memnew(Label);
    deemphasis_label->set_name("DeemphasisLabel");
    deemphasis_label->set_text("De-emphasis");
    deemphasis_row->add_child(deemphasis_label);

    OptionButton *deemphasis = memnew(OptionButton);
    deemphasis->set_name("DeemphasisSelector");
    deemphasis->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    deemphasis->add_item("Off", 0);
    deemphasis->add_item("50us", 1);
    deemphasis->add_item("75us", 2);
    deemphasis->select(deemphasis_mode);
    deemphasis_row->add_child(deemphasis);

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
