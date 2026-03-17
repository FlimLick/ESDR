#include "rtl_sdr_source_node.h"

#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace {
constexpr bool kRTLSourceStreamStatsLoggingEnabled = false;

void log_rtl_source(const String &p_message) {
    UtilityFunctions::print(String("[ESDR][RTL Source] ") + p_message);
}
} // namespace

void RTLSDRSource::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_start_toggled", "enabled"), &RTLSDRSource::_on_start_toggled);
    ClassDB::bind_method(D_METHOD("_on_frequency_selector_value_changed", "value"), &RTLSDRSource::_on_frequency_selector_value_changed);
    ClassDB::bind_method(D_METHOD("_on_device_selector_item_selected", "index"), &RTLSDRSource::_on_device_selector_item_selected);
    ClassDB::bind_method(D_METHOD("_on_sample_rate_selector_value_changed", "value"), &RTLSDRSource::_on_sample_rate_selector_value_changed);
    ClassDB::bind_method(D_METHOD("_on_gain_changed", "value"), &RTLSDRSource::_on_gain_changed);
    ClassDB::bind_method(D_METHOD("_on_bias_t_toggled", "enabled"), &RTLSDRSource::_on_bias_t_toggled);
    ClassDB::bind_method(D_METHOD("_on_rtl_agc_toggled", "enabled"), &RTLSDRSource::_on_rtl_agc_toggled);
    ClassDB::bind_method(D_METHOD("_on_tuner_agc_toggled", "enabled"), &RTLSDRSource::_on_tuner_agc_toggled);
    ClassDB::bind_method(D_METHOD("_on_iq_correction_toggled", "enabled"), &RTLSDRSource::_on_iq_correction_toggled);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &RTLSDRSource::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &RTLSDRSource::set_port_value);
    ClassDB::bind_method(D_METHOD("get_current_sample_rate_hz"), &RTLSDRSource::get_current_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("get_min_sample_rate_hz"), &RTLSDRSource::get_min_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("get_max_sample_rate_hz"), &RTLSDRSource::get_max_sample_rate_hz);
    ClassDB::bind_method(D_METHOD("get_min_frequency_hz"), &RTLSDRSource::get_min_frequency_hz);
    ClassDB::bind_method(D_METHOD("get_max_frequency_hz"), &RTLSDRSource::get_max_frequency_hz);
    ClassDB::bind_method(D_METHOD("set_frequency_control_locked", "locked"), &RTLSDRSource::set_frequency_control_locked);
    ClassDB::bind_method(D_METHOD("get_frequency_control_locked"), &RTLSDRSource::get_frequency_control_locked);
    ClassDB::bind_method(D_METHOD("set_scan_frequency_hz", "frequency_hz"), &RTLSDRSource::set_scan_frequency_hz);

    ADD_SIGNAL(MethodInfo("baseband_frame", PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "frame")));
    ADD_SIGNAL(MethodInfo("sample_rate_changed", PropertyInfo(Variant::INT, "sample_rate_hz")));
    ADD_SIGNAL(MethodInfo("frequency_tuned_changed", PropertyInfo(Variant::INT, "frequency_hz")));
    ADD_SIGNAL(MethodInfo("source_status_changed",
            PropertyInfo(Variant::BOOL, "is_running"),
            PropertyInfo(Variant::STRING, "message")));
}

void RTLSDRSource::_notification(int32_t p_what) {
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

void RTLSDRSource::on_ready() {
    log_rtl_source("on_ready");
    last_pull_time_us = 0;
    debug_last_stream_log_us = 0;
    debug_stream_samples = 0;
    debug_stream_iq_peak = 0.0f;
    last_emitted_tuned_frequency_hz = frequency_hz_value;
    effective_sample_rate_hz_value = sample_rate_value;
    ensure_ui();
    cache_ui_refs();
    set_frequency_hz_value(frequency_hz_value, false);
    set_sample_rate_value(sample_rate_value, false);
    refresh_gain_header();
    set_status(false, "Status: [OFF]");

    if (Engine::get_singleton()->is_editor_hint()) {
        log_rtl_source("editor hint active, process disabled.");
        set_process(false);
        return;
    }

    backend.instantiate();
    if (backend.is_null()) {
        log_rtl_source("backend instantiate failed.");
        set_status(false, "Status: [EXT Error]");
        set_problem_state(true, "RTL backend extension missing.");
        return;
    }

    log_rtl_source("backend instantiated.");
    bind_ui();
    refresh_device_selector();
    push_ui_to_backend();
    log_rtl_source("UI pushed to backend.");
    set_process(true);
}

void RTLSDRSource::on_exit_tree() {
    log_rtl_source("on_exit_tree; stopping backend.");
    backend_transition_active.store(false);
    backend_transition_result_ready.store(false);
    if (backend.is_valid()) {
        backend->stop();
    }
    if (backend_transition_thread.joinable()) {
        backend_transition_thread.join();
    }
}

void RTLSDRSource::process_tick() {
    if (backend.is_null()) {
        return;
    }
    poll_backend_transition();

    if (backend->is_running()) {
        const int64_t effective_hz = static_cast<int64_t>(std::llround(backend->get_effective_sample_rate()));
        if (effective_hz > 0 && effective_hz != effective_sample_rate_hz_value) {
            effective_sample_rate_hz_value = effective_hz;
            emit_signal("sample_rate_changed", effective_sample_rate_hz_value);
        }

        const int64_t tuned_frequency_hz = static_cast<int64_t>(std::llround(backend->get_effective_frequency_hz()));
        if (tuned_frequency_hz > 0 && tuned_frequency_hz != last_emitted_tuned_frequency_hz) {
            last_emitted_tuned_frequency_hz = tuned_frequency_hz;
            frequency_hz_value = tuned_frequency_hz;
            if (frequency_selector != nullptr) {
                frequency_selector->set_value_no_signal(frequency_hz_value);
            }
            emit_signal("frequency_tuned_changed", tuned_frequency_hz);
        }
    }

    Time *time = Time::get_singleton();
    const uint64_t now_us = time != nullptr ? time->get_ticks_usec() : 0;
    double dt_sec = 1.0 / 60.0;
    if (last_pull_time_us > 0 && now_us > last_pull_time_us) {
        dt_sec = std::clamp(static_cast<double>(now_us - last_pull_time_us) * 1e-6, 0.001, 0.25);
    }
    last_pull_time_us = now_us;

    const int32_t queued = backend->get_queued_sample_count();
    if (queued > 0) {
        const double source_rate = std::max(1000.0, static_cast<double>(effective_sample_rate_hz_value));
        int32_t target_samples = static_cast<int32_t>(std::llround(source_rate * dt_sec));
        target_samples = std::clamp(target_samples, 4096, 131072);
        if (queued > target_samples * 4) {
            target_samples = std::min(queued, target_samples * 2);
        } else {
            target_samples = std::min(queued, target_samples);
        }

        if (target_samples > 0) {
            PackedVector2Array frame = backend->pull_baseband_frame(target_samples);
            if (!frame.is_empty()) {
                if (kRTLSourceStreamStatsLoggingEnabled) {
                    debug_stream_samples += frame.size();
                    for (int32_t i = 0; i < frame.size(); i += 16) {
                        const Vector2 iq = frame[i];
                        debug_stream_iq_peak = std::max(debug_stream_iq_peak, static_cast<float>(std::max(std::abs(iq.x), std::abs(iq.y))));
                    }
                }
                emit_signal("baseband_frame", frame);
            }
        }
    }

    if (kRTLSourceStreamStatsLoggingEnabled && debug_stream_samples > 0 && now_us > 0) {
        if (debug_last_stream_log_us == 0) {
            debug_last_stream_log_us = now_us;
        } else {
            const double dt_sec = static_cast<double>(now_us - debug_last_stream_log_us) * 1e-6;
            if (dt_sec >= 1.0) {
                const double stream_sps = static_cast<double>(debug_stream_samples) / dt_sec;
                log_rtl_source(String("stream stats dt=") + String::num(dt_sec, 3) +
                        " sps=" + String::num(stream_sps, 1) +
                        " effective_sr=" + String::num_int64(effective_sample_rate_hz_value) +
                        " iq_peak=" + String::num(debug_stream_iq_peak, 4) +
                        " queue=" + String::num_int64(queued));
                debug_last_stream_log_us = now_us;
                debug_stream_samples = 0;
                debug_stream_iq_peak = 0.0f;
            }
        }
    } else if (!kRTLSourceStreamStatsLoggingEnabled) {
        debug_last_stream_log_us = 0;
        debug_stream_samples = 0;
        debug_stream_iq_peak = 0.0f;
    }

    String error_text = backend->consume_error();
    if (!error_text.is_empty()) {
        log_rtl_source(String("backend error: ") + error_text);
        set_status(false, "Status: [ERROR]");
        set_problem_state(true, error_text);
        if (status_button != nullptr) {
            status_button->set_tooltip_text(error_text);
        }
        if (status_button != nullptr && status_button->is_pressed()) {
            status_button->set_pressed_no_signal(false);
        }
        emit_signal("source_status_changed", false, error_text);
    }

    if (!backend_transition_active.load() && !backend->is_running() && status_button != nullptr && status_button->is_pressed()) {
        status_button->set_pressed_no_signal(false);
        set_status(false, "Status: [OFF]");
        emit_signal("source_status_changed", false, "Stopped");
    }
}

void RTLSDRSource::cache_ui_refs() {
    device_row = Object::cast_to<Control>(get_node_or_null(NodePath("DeviceRow")));
    device_label = Object::cast_to<Label>(get_node_or_null(NodePath("DeviceRow/DeviceLabel")));
    device_selector = Object::cast_to<OptionButton>(get_node_or_null(NodePath("DeviceRow/DeviceSelector")));
    frequency_selector = Object::cast_to<DigitNumberSelector>(get_node_or_null(NodePath("FrequencyRow/FrequencyPanel/FrequencySelector")));

    status_button = Object::cast_to<Button>(get_node_or_null(NodePath("StartRow/StatusButton")));
    scanning_label = Object::cast_to<Label>(get_node_or_null(NodePath("StartRow/ScanningLabel")));
    if (scanning_label == nullptr) {
        HBoxContainer *start_row = Object::cast_to<HBoxContainer>(get_node_or_null(NodePath("StartRow")));
        if (start_row != nullptr) {
            Label *scan_state = memnew(Label);
            scan_state->set_name("ScanningLabel");
            scan_state->set_text("Scanning");
            scan_state->add_theme_color_override("font_color", Color(1.0, 0.90, 0.12, 1.0));
            scan_state->add_theme_constant_override("outline_size", 1);
            scan_state->set_visible(false);
            start_row->add_child(scan_state);
            scanning_label = scan_state;
        }
    }
    bias_t_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("BiasTCheckBox")));
    rtl_agc_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("RTLAGCCheckBox")));
    tuner_agc_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("TunerAGCCheckBox")));
    iq_correction_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("IQCorrectionCheckBox")));
    sample_rate_selector = Object::cast_to<DigitNumberSelector>(get_node_or_null(NodePath("SampleRateRow/SampleRatePanel/SampleRateSelector")));
    gain_slider = Object::cast_to<HSlider>(get_node_or_null(NodePath("GainSlider")));
    gain_min_label = Object::cast_to<Label>(get_node_or_null(NodePath("GainHeaderRow/GainMinLabel")));
    gain_value_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("GainHeaderRow/GainValueSpin")));
    gain_max_label = Object::cast_to<Label>(get_node_or_null(NodePath("GainHeaderRow/GainMaxLabel")));
    refresh_scan_status_label();
}

void RTLSDRSource::bind_ui() {
    if (status_button != nullptr) {
        const Callable cb(this, "_on_start_toggled");
        if (!status_button->is_connected("toggled", cb)) {
            status_button->connect("toggled", cb);
        }
    }

    if (device_selector != nullptr) {
        const Callable cb(this, "_on_device_selector_item_selected");
        if (!device_selector->is_connected("item_selected", cb)) {
            device_selector->connect("item_selected", cb);
        }
    }

    if (frequency_selector != nullptr) {
        const Callable cb(this, "_on_frequency_selector_value_changed");
        if (!frequency_selector->is_connected("value_changed", cb)) {
            frequency_selector->connect("value_changed", cb);
        }
    }

    if (sample_rate_selector != nullptr) {
        const Callable cb(this, "_on_sample_rate_selector_value_changed");
        if (!sample_rate_selector->is_connected("value_changed", cb)) {
            sample_rate_selector->connect("value_changed", cb);
        }
    }

    if (gain_slider != nullptr) {
        const Callable cb(this, "_on_gain_changed");
        if (!gain_slider->is_connected("value_changed", cb)) {
            gain_slider->connect("value_changed", cb);
        }
    }
    if (gain_value_spin != nullptr) {
        const Callable cb(this, "_on_gain_changed");
        if (!gain_value_spin->is_connected("value_changed", cb)) {
            gain_value_spin->connect("value_changed", cb);
        }
    }

    if (bias_t_checkbox != nullptr) {
        const Callable cb(this, "_on_bias_t_toggled");
        if (!bias_t_checkbox->is_connected("toggled", cb)) {
            bias_t_checkbox->connect("toggled", cb);
        }
    }

    if (rtl_agc_checkbox != nullptr) {
        const Callable cb(this, "_on_rtl_agc_toggled");
        if (!rtl_agc_checkbox->is_connected("toggled", cb)) {
            rtl_agc_checkbox->connect("toggled", cb);
        }
    }

    if (tuner_agc_checkbox != nullptr) {
        const Callable cb(this, "_on_tuner_agc_toggled");
        if (!tuner_agc_checkbox->is_connected("toggled", cb)) {
            tuner_agc_checkbox->connect("toggled", cb);
        }
    }

    if (iq_correction_checkbox != nullptr) {
        const Callable cb(this, "_on_iq_correction_toggled");
        if (!iq_correction_checkbox->is_connected("toggled", cb)) {
            iq_correction_checkbox->connect("toggled", cb);
        }
    }
}

void RTLSDRSource::set_frequency_hz_value(int64_t p_frequency_hz, bool p_push_backend) {
    frequency_hz_value = std::clamp<int64_t>(p_frequency_hz, frequency_min_hz_value, frequency_max_hz_value);

    if (frequency_selector != nullptr) {
        frequency_selector->set_value_no_signal(frequency_hz_value);
    }

    if (p_push_backend && backend.is_valid()) {
        backend->set_frequency_hz(static_cast<double>(frequency_hz_value));
    }

    const bool backend_running = backend.is_valid() && backend->is_running();
    if (!backend_running && last_emitted_tuned_frequency_hz != frequency_hz_value) {
        last_emitted_tuned_frequency_hz = frequency_hz_value;
        emit_signal("frequency_tuned_changed", frequency_hz_value);
    }
}

void RTLSDRSource::set_sample_rate_value(int64_t p_sample_rate, bool p_push_backend) {
    sample_rate_value = std::clamp<int64_t>(p_sample_rate, sample_rate_min_hz_value, sample_rate_max_hz_value);
    effective_sample_rate_hz_value = sample_rate_value;

    if (sample_rate_selector != nullptr) {
        sample_rate_selector->set_value_no_signal(sample_rate_value);
    }

    if (p_push_backend && backend.is_valid()) {
        backend->set_sample_rate(static_cast<double>(sample_rate_value));
        const int64_t effective_hz = static_cast<int64_t>(std::llround(backend->get_effective_sample_rate()));
        if (effective_hz > 0) {
            effective_sample_rate_hz_value = effective_hz;
        }
    }

    emit_signal("sample_rate_changed", effective_sample_rate_hz_value);
}

int32_t RTLSDRSource::wrap_index(int32_t p_index, int32_t p_count) const {
    if (p_count <= 0) {
        return 0;
    }

    int32_t wrapped = p_index % p_count;
    if (wrapped < 0) {
        wrapped += p_count;
    }
    return wrapped;
}

String RTLSDRSource::get_device_args_for_index(int32_t p_index) const {
    if (p_index >= 0 && p_index < device_args_by_index.size()) {
        return device_args_by_index[p_index];
    }
    if (device_selector == nullptr || p_index < 0 || p_index >= device_selector->get_item_count()) {
        return String();
    }
    return device_selector->get_item_text(p_index);
}

String RTLSDRSource::extract_friendly_device_label(const String &p_device_args) {
    PackedStringArray parts = p_device_args.split(",", false);
    for (int32_t i = 0; i < parts.size(); i++) {
        const String token = parts[i].strip_edges();
        if (!token.begins_with("label=")) {
            continue;
        }
        const String label = token.substr(6, token.length()).strip_edges();
        if (!label.is_empty()) {
            return label;
        }
    }
    return p_device_args;
}

void RTLSDRSource::apply_status_button_theme(const Color &p_color) {
    if (status_button == nullptr) {
        return;
    }

    // Keep panel/theme styles untouched; only tint the button text.
    status_button->remove_theme_stylebox_override("normal");
    status_button->remove_theme_stylebox_override("hover");
    status_button->remove_theme_stylebox_override("pressed");
    status_button->remove_theme_stylebox_override("focus");
    status_button->remove_theme_stylebox_override("disabled");

    status_button->add_theme_color_override("font_color", p_color);
    status_button->add_theme_color_override("font_pressed_color", p_color);
    status_button->add_theme_color_override("font_hover_color", p_color);
    status_button->add_theme_color_override("font_hover_pressed_color", p_color);
    status_button->add_theme_color_override("font_focus_color", p_color);
    status_button->add_theme_color_override("font_disabled_color", p_color.darkened(0.5));
}

void RTLSDRSource::set_device_index_value(int32_t p_index, bool p_push_backend) {
    if (device_selector == nullptr || device_selector->get_item_count() <= 0 || !has_enumerated_devices) {
        log_rtl_source("set_device_index_value ignored (no enumerated devices).");
        return;
    }

    const int32_t index = wrap_index(p_index, device_selector->get_item_count());
    device_selector->select(index);
    const String selected_args = get_device_args_for_index(index);
    log_rtl_source(String("device index selected: ") + String::num_int64(index) +
            " label=" + device_selector->get_item_text(index) +
            " args=" + selected_args);

    if (p_push_backend && backend.is_valid()) {
        backend->set_device_args(selected_args);
        refresh_capability_limits();
    }
}

void RTLSDRSource::refresh_gain_header() {
    if (gain_slider == nullptr) {
        return;
    }

    if (gain_min_label != nullptr) {
        gain_min_label->set_text(String("min ") + String::num(gain_slider->get_min()));
    }
    if (gain_value_spin != nullptr) {
        gain_value_spin->set_min(gain_slider->get_min());
        gain_value_spin->set_max(gain_slider->get_max());
        gain_value_spin->set_step(gain_slider->get_step());
        gain_value_spin->set_value_no_signal(gain_slider->get_value());
    }
    if (gain_max_label != nullptr) {
        gain_max_label->set_text(String("max ") + String::num(gain_slider->get_max()));
    }
}

void RTLSDRSource::style_metric_label(Label *p_label) {
    if (p_label == nullptr) {
        return;
    }

    p_label->add_theme_font_size_override("font_size", 10);
    p_label->add_theme_color_override("font_color", Color(0.67, 0.67, 0.67, 1.0));
}

void RTLSDRSource::apply_selector_limits() {
    if (frequency_selector != nullptr) {
        frequency_selector->set_limits(frequency_min_hz_value, frequency_max_hz_value);
    }
    if (sample_rate_selector != nullptr) {
        sample_rate_selector->set_limits(sample_rate_min_hz_value, sample_rate_max_hz_value);
    }
}

void RTLSDRSource::refresh_capability_limits() {
    frequency_min_hz_value = frequency_min_hz_default;
    frequency_max_hz_value = frequency_max_hz_default;
    sample_rate_min_hz_value = sample_rate_min_default;
    sample_rate_max_hz_value = sample_rate_max_default;

    if (backend.is_valid()) {
        const int64_t min_freq = static_cast<int64_t>(std::llround(backend->get_min_frequency_hz()));
        const int64_t max_freq = static_cast<int64_t>(std::llround(backend->get_max_frequency_hz()));
        if (max_freq > min_freq) {
            frequency_min_hz_value = std::max<int64_t>(frequency_min_hz_default, min_freq);
            frequency_max_hz_value = std::max<int64_t>(frequency_min_hz_value, max_freq);
        }

        const int64_t min_sr = static_cast<int64_t>(std::llround(backend->get_min_sample_rate()));
        const int64_t max_sr = static_cast<int64_t>(std::llround(backend->get_max_sample_rate()));
        if (max_sr > min_sr) {
            sample_rate_min_hz_value = std::max<int64_t>(sample_rate_min_default, min_sr);
            sample_rate_max_hz_value = std::max<int64_t>(sample_rate_min_hz_value, max_sr);
        }
    }

    apply_selector_limits();
    set_frequency_hz_value(frequency_hz_value, false);
    set_sample_rate_value(sample_rate_value, false);
    log_rtl_source(String("device limits freq=[") + String::num_int64(frequency_min_hz_value) +
            ", " + String::num_int64(frequency_max_hz_value) +
            "] sr=[" + String::num_int64(sample_rate_min_hz_value) +
            ", " + String::num_int64(sample_rate_max_hz_value) + "]");
}

void RTLSDRSource::refresh_device_selector() {
    if (device_selector == nullptr) {
        return;
    }

    device_selector->clear();
    device_args_by_index.clear();
    has_enumerated_devices = false;

    if (device_row != nullptr) {
        device_row->set_visible(true);
    }
    if (device_selector != nullptr) {
        device_selector->set_visible(true);
    }
    if (device_label != nullptr) {
        device_label->set_visible(true);
    }
    device_selector->set_fit_to_longest_item(false);
    device_selector->set_clip_text(true);

    if (backend.is_null()) {
        device_selector->add_item("Backend unavailable");
        device_args_by_index.push_back(String());
        device_selector->set_disabled(true);
        log_rtl_source("device refresh: backend unavailable.");
        refresh_capability_limits();
        configure_slots();
        return;
    }

    const PackedStringArray devices = backend->enumerate_devices();
    if (devices.is_empty()) {
        backend->set_device_args(String());
        device_selector->add_item("No RTL-SDR devices found");
        device_args_by_index.push_back(String());
        device_selector->set_disabled(true);
        log_rtl_source("device refresh: no devices found.");
        refresh_capability_limits();
        configure_slots();
        return;
    }

    has_enumerated_devices = true;
    device_selector->set_disabled(false);
    for (int32_t i = 0; i < devices.size(); i++) {
        const String device_args = devices[i];
        const String friendly = extract_friendly_device_label(device_args);
        device_selector->add_item(friendly);
        device_args_by_index.push_back(device_args);
        log_rtl_source(String("device[") + String::num_int64(i) + "] label=" + friendly + " args=" + device_args);
    }

    device_selector->select(0);
    backend->set_device_args(get_device_args_for_index(0));
    log_rtl_source(String("device refresh complete. count=") + String::num_int64(devices.size()));
    refresh_capability_limits();
    configure_slots();
}

void RTLSDRSource::push_ui_to_backend() {
    if (backend.is_null()) {
        log_rtl_source("push_ui_to_backend skipped: backend null.");
        return;
    }

    backend->set_driver("rtlsdr");
    backend->set_frequency_hz(static_cast<double>(frequency_hz_value));
    backend->set_sample_rate(static_cast<double>(sample_rate_value));
    log_rtl_source(String("push config freq=") + String::num_int64(frequency_hz_value) +
            " sample_rate=" + String::num_int64(sample_rate_value));

    if (has_enumerated_devices && device_selector != nullptr && device_selector->get_item_count() > 0) {
        const int32_t selected = device_selector->get_selected();
        if (selected >= 0 && selected < device_selector->get_item_count()) {
            const String selected_args = get_device_args_for_index(selected);
            backend->set_device_args(selected_args);
            log_rtl_source(String("push selected device label=") + device_selector->get_item_text(selected) + " args=" + selected_args);
        }
    }

    if (gain_slider != nullptr) {
        backend->set_gain_db(static_cast<float>(gain_slider->get_value()));
    }

    if (bias_t_checkbox != nullptr) {
        backend->set_bias_t_enabled(bias_t_checkbox->is_pressed());
    }

    if (rtl_agc_checkbox != nullptr) {
        backend->set_rtl_agc_enabled(rtl_agc_checkbox->is_pressed());
    }

    if (tuner_agc_checkbox != nullptr) {
        backend->set_tuner_agc_enabled(tuner_agc_checkbox->is_pressed());
    }

    if (iq_correction_checkbox != nullptr) {
        backend->set_iq_correction_enabled(iq_correction_checkbox->is_pressed());
    }
}

void RTLSDRSource::set_status(bool p_is_running, const String &p_message, bool p_is_loading) {
    if (status_button == nullptr) {
        return;
    }

    const String state_text = p_is_loading ? String("LOADING") : (p_is_running ? String("ON") : String("OFF"));
    log_rtl_source(String("set_status: ") + state_text + " (" + p_message + ")");
    if (p_is_loading) {
        status_button->set_text("LOAD");
    } else {
        status_button->set_text(p_is_running ? "ON" : "OFF");
    }
    status_button->set_tooltip_text(p_message);

    const Color off_color = Color(1.0, 0.23, 0.23, 1.0);
    const Color loading_color = Color(1.0, 0.90, 0.12, 1.0);
    const Color on_color = Color(0.35, 1.0, 0.44, 1.0);
    const Color state_color = p_is_loading ? loading_color : (p_is_running ? on_color : off_color);
    apply_status_button_theme(state_color);
}

void RTLSDRSource::refresh_scan_status_label() {
    if (scanning_label == nullptr) {
        return;
    }
    scanning_label->set_visible(frequency_control_locked);
}

void RTLSDRSource::begin_backend_transition(bool p_target_running) {
    if (backend.is_null()) {
        if (status_button != nullptr) {
            status_button->set_pressed_no_signal(false);
        }
        set_status(false, "Status: [EXT Missing]");
        set_problem_state(true, "RTL backend is not available.");
        return;
    }

    if (backend_transition_active.load()) {
        log_rtl_source("backend transition ignored; one is already active.");
        return;
    }

    if (backend_transition_thread.joinable()) {
        backend_transition_thread.join();
    }

    backend_transition_target_running.store(p_target_running);
    backend_transition_succeeded.store(false);
    backend_transition_result_ready.store(false);
    backend_transition_active.store(true);

    if (status_button != nullptr) {
        status_button->set_disabled(true);
    }

    if (p_target_running) {
        set_status(false, "Status: [LOADING]", true);
    } else {
        set_status(false, "Status: [STOPPING]", true);
    }

    Ref<RTLSDRBackend> backend_ref = backend;
    backend_transition_thread = std::thread([this, backend_ref, p_target_running]() mutable {
        bool success = false;
        if (backend_ref.is_valid()) {
            if (p_target_running) {
                success = backend_ref->start();
            } else {
                backend_ref->stop();
                success = true;
            }
        }
        backend_transition_succeeded.store(success);
        backend_transition_active.store(false);
        backend_transition_result_ready.store(true);
    });
}

void RTLSDRSource::poll_backend_transition() {
    if (!backend_transition_result_ready.load()) {
        return;
    }

    backend_transition_result_ready.store(false);
    const bool target_running = backend_transition_target_running.load();
    const bool succeeded = backend_transition_succeeded.load();

    if (backend_transition_thread.joinable()) {
        backend_transition_thread.join();
    }

    if (status_button != nullptr) {
        status_button->set_disabled(false);
    }

    if (target_running && succeeded && backend.is_valid() && backend->is_running()) {
        log_rtl_source("backend start success (async).");
        if (status_button != nullptr) {
            status_button->set_pressed_no_signal(true);
        }
        set_status(true, "Status: [ON]");
        set_problem_state(false, "");
        emit_signal("source_status_changed", true, "Running");
        return;
    }

    if (target_running) {
        if (status_button != nullptr) {
            status_button->set_pressed_no_signal(false);
        }
        String error_text = backend.is_valid() ? backend->consume_error() : String("RTL backend is not available.");
        if (error_text.is_empty() && backend.is_valid()) {
            error_text = backend->get_last_status_message();
        }
        if (error_text.is_empty()) {
            error_text = "Failed to start RTL backend.";
        }
        log_rtl_source(String("backend start failed (async): ") + error_text);
        set_status(false, "Status: [OFF]");
        set_problem_state(true, error_text);
        if (status_button != nullptr) {
            status_button->set_tooltip_text(error_text);
        }
        emit_signal("source_status_changed", false, error_text);
        return;
    }

    if (status_button != nullptr) {
        status_button->set_pressed_no_signal(false);
    }
    set_status(false, "Status: [OFF]");
    set_problem_state(false, "");
    emit_signal("source_status_changed", false, "Stopped");
}

void RTLSDRSource::_on_start_toggled(bool p_enabled) {
    log_rtl_source(String("status toggled -> ") + (p_enabled ? String("ON") : String("OFF")));
    if (status_button != nullptr && status_button->is_pressed() != p_enabled) {
        status_button->set_pressed_no_signal(p_enabled);
    }

    if (p_enabled) {
        last_pull_time_us = 0;
        debug_last_stream_log_us = 0;
        debug_stream_samples = 0;
        debug_stream_iq_peak = 0.0f;
        push_ui_to_backend();
        begin_backend_transition(true);
        return;
    }

    begin_backend_transition(false);
}

void RTLSDRSource::_on_frequency_selector_value_changed(int64_t p_value) {
    if (frequency_control_locked) {
        log_rtl_source(String("frequency change ignored while scan lock active: ") + String::num_int64(p_value));
        if (frequency_selector != nullptr) {
            frequency_selector->set_value_no_signal(frequency_hz_value);
        }
        return;
    }
    log_rtl_source(String("frequency changed: ") + String::num_int64(p_value));
    set_frequency_hz_value(p_value, true);
}

void RTLSDRSource::_on_device_selector_item_selected(int64_t p_index) {
    log_rtl_source(String("device selector changed -> ") + String::num_int64(p_index));
    set_device_index_value(static_cast<int32_t>(p_index), true);
}

void RTLSDRSource::_on_sample_rate_selector_value_changed(int64_t p_value) {
    log_rtl_source(String("sample rate changed: ") + String::num_int64(p_value));
    set_sample_rate_value(p_value, true);
}

void RTLSDRSource::_on_gain_changed(double p_value) {
    log_rtl_source(String("gain changed: ") + String::num(p_value));
    if (gain_slider != nullptr && std::abs(gain_slider->get_value() - p_value) > 1e-9) {
        gain_slider->set_value_no_signal(p_value);
    }
    if (backend.is_valid()) {
        backend->set_gain_db(static_cast<float>(p_value));
    }
    refresh_gain_header();
}

void RTLSDRSource::_on_bias_t_toggled(bool p_enabled) {
    log_rtl_source(String("bias_t toggled: ") + (p_enabled ? String("true") : String("false")));
    if (backend.is_valid()) {
        backend->set_bias_t_enabled(p_enabled);
    }
}

void RTLSDRSource::_on_rtl_agc_toggled(bool p_enabled) {
    log_rtl_source(String("rtl_agc toggled: ") + (p_enabled ? String("true") : String("false")));
    if (backend.is_valid()) {
        backend->set_rtl_agc_enabled(p_enabled);
    }
}

void RTLSDRSource::_on_tuner_agc_toggled(bool p_enabled) {
    log_rtl_source(String("tuner_agc toggled: ") + (p_enabled ? String("true") : String("false")));
    if (backend.is_valid()) {
        backend->set_tuner_agc_enabled(p_enabled);
    }
}

void RTLSDRSource::_on_iq_correction_toggled(bool p_enabled) {
    log_rtl_source(String("iq_correction toggled: ") + (p_enabled ? String("true") : String("false")));
    if (backend.is_valid()) {
        backend->set_iq_correction_enabled(p_enabled);
    }
}

int64_t RTLSDRSource::get_current_sample_rate_hz() const {
    return effective_sample_rate_hz_value;
}

int64_t RTLSDRSource::get_min_sample_rate_hz() const {
    return sample_rate_min_hz_value;
}

int64_t RTLSDRSource::get_max_sample_rate_hz() const {
    return sample_rate_max_hz_value;
}

int64_t RTLSDRSource::get_min_frequency_hz() const {
    return frequency_min_hz_value;
}

int64_t RTLSDRSource::get_max_frequency_hz() const {
    return frequency_max_hz_value;
}

void RTLSDRSource::set_frequency_control_locked(bool p_locked) {
    if (frequency_control_locked == p_locked) {
        refresh_scan_status_label();
        return;
    }
    frequency_control_locked = p_locked;
    log_rtl_source(String("frequency control lock: ") + (frequency_control_locked ? String("ON") : String("OFF")));
    refresh_scan_status_label();
    if (frequency_selector != nullptr && frequency_control_locked) {
        frequency_selector->set_value_no_signal(frequency_hz_value);
    }
}

bool RTLSDRSource::get_frequency_control_locked() const {
    return frequency_control_locked;
}

void RTLSDRSource::set_scan_frequency_hz(int64_t p_frequency_hz) {
    set_frequency_hz_value(p_frequency_hz, true);
}

Variant RTLSDRSource::get_port_value(int64_t p_port) const {
    switch (p_port) {
        case 0:
            if (device_selector != nullptr && device_selector->get_item_count() > 0) {
                return device_selector->get_selected();
            }
            return 0;
        case 1:
            return frequency_hz_value;
        case 4:
            return bias_t_checkbox != nullptr ? bias_t_checkbox->is_pressed() : false;
        case 5:
            return rtl_agc_checkbox != nullptr ? rtl_agc_checkbox->is_pressed() : false;
        case 6:
            return tuner_agc_checkbox != nullptr ? tuner_agc_checkbox->is_pressed() : false;
        case 7:
            return iq_correction_checkbox != nullptr ? iq_correction_checkbox->is_pressed() : false;
        case 8:
            return sample_rate_value;
        case 10:
            return gain_slider != nullptr ? gain_slider->get_value() : 0.0;
        default:
            return Variant();
    }
}

void RTLSDRSource::set_port_value(int64_t p_port, const Variant &p_value) {
    switch (p_port) {
        case 0: {
            const int32_t index = static_cast<int32_t>(p_value);
            set_device_index_value(index, true);
            break;
        }
        case 1:
            if (frequency_control_locked) {
                if (frequency_selector != nullptr) {
                    frequency_selector->set_value_no_signal(frequency_hz_value);
                }
                log_rtl_source(String("set_port_value freq ignored while scan lock active: ") + String::num_int64(static_cast<int64_t>(p_value)));
                break;
            }
            set_frequency_hz_value(static_cast<int64_t>(p_value), true);
            break;
        case 4: {
            const bool enabled = static_cast<bool>(p_value);
            if (bias_t_checkbox != nullptr) {
                bias_t_checkbox->set_pressed_no_signal(enabled);
            }
            _on_bias_t_toggled(enabled);
            break;
        }
        case 5: {
            const bool enabled = static_cast<bool>(p_value);
            if (rtl_agc_checkbox != nullptr) {
                rtl_agc_checkbox->set_pressed_no_signal(enabled);
            }
            _on_rtl_agc_toggled(enabled);
            break;
        }
        case 6: {
            const bool enabled = static_cast<bool>(p_value);
            if (tuner_agc_checkbox != nullptr) {
                tuner_agc_checkbox->set_pressed_no_signal(enabled);
            }
            _on_tuner_agc_toggled(enabled);
            break;
        }
        case 7: {
            const bool enabled = static_cast<bool>(p_value);
            if (iq_correction_checkbox != nullptr) {
                iq_correction_checkbox->set_pressed_no_signal(enabled);
            }
            _on_iq_correction_toggled(enabled);
            break;
        }
        case 8:
            set_sample_rate_value(static_cast<int64_t>(p_value), true);
            break;
        case 10: {
            const double value = static_cast<double>(p_value);
            if (gain_slider != nullptr) {
                gain_slider->set_value_no_signal(value);
            }
            _on_gain_changed(value);
            break;
        }
        default:
            break;
    }
}

void RTLSDRSource::configure_slots() {
    clear_all_slots();
    set_slot(0, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, false, 0, Color(1.0, 1.0, 1.0, 1.0), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(3, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_BASEBAND, signal_color(SIGNAL_BASEBAND));
    set_slot(4, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(5, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(6, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(7, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(8, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(9, false, 0, Color(1.0, 1.0, 1.0, 1.0), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(10, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
}

void RTLSDRSource::ensure_ui() {
    if (has_node(NodePath("DeviceRow/DeviceSelector")) && has_node(NodePath("StartRow/StatusButton"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("RTL-SDR Source");
    set_custom_minimum_size(Vector2(420, 420));
    set_resizable(true);
    set("theme_override_constants/separation", 6);

    HBoxContainer *device_row = memnew(HBoxContainer);
    device_row->set_name("DeviceRow");
    add_child(device_row);

    Label *device_label = memnew(Label);
    device_label->set_name("DeviceLabel");
    device_label->set_text("Device");
    device_row->add_child(device_label);

    OptionButton *device_list = memnew(OptionButton);
    device_list->set_name("DeviceSelector");
    device_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    device_list->set_fit_to_longest_item(false);
    device_list->set_clip_text(true);
    device_row->add_child(device_list);

    HBoxContainer *frequency_row = memnew(HBoxContainer);
    frequency_row->set_name("FrequencyRow");
    frequency_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    frequency_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    add_child(frequency_row);

    PanelContainer *frequency_panel = memnew(PanelContainer);
    frequency_panel->set_name("FrequencyPanel");
    frequency_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    frequency_row->add_child(frequency_panel);

    DigitNumberSelector *frequency = memnew(DigitNumberSelector);
    frequency->set_name("FrequencySelector");
    frequency->set_limits(frequency_min_hz_default, frequency_max_hz_default);
    frequency->set_digit_count(12);
    frequency->set_group_size(3);
    frequency->set_show_group_labels(true);
    frequency->set_show_separators(true);
    frequency->set_suffix_text("");
    PackedStringArray frequency_labels;
    frequency_labels.push_back("GHz");
    frequency_labels.push_back("MHz");
    frequency_labels.push_back("kHz");
    frequency_labels.push_back("Hz");
    frequency->set_group_labels(frequency_labels);
    frequency->set_value_no_signal(frequency_hz_value);
    frequency_panel->add_child(frequency);

    HBoxContainer *start_hbox = memnew(HBoxContainer);
    start_hbox->set_name("StartRow");
    add_child(start_hbox);

    Label *status_prefix = memnew(Label);
    status_prefix->set_name("StatusPrefixLabel");
    status_prefix->set_text("Status:");
    start_hbox->add_child(status_prefix);

    Button *status = memnew(Button);
    status->set_name("StatusButton");
    status->set_toggle_mode(true);
    status->set_text("OFF");
    status->set_focus_mode(Control::FOCUS_NONE);
    start_hbox->add_child(status);

    Label *baseband = memnew(Label);
    baseband->set_name("BasebandLabel");
    baseband->set_text("Baseband");
    baseband->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    add_child(baseband);

    CheckBox *bias = memnew(CheckBox);
    bias->set_name("BiasTCheckBox");
    bias->set_text("Bias T");
    add_child(bias);

    CheckBox *rtl_agc = memnew(CheckBox);
    rtl_agc->set_name("RTLAGCCheckBox");
    rtl_agc->set_text("RTL AGC");
    add_child(rtl_agc);

    CheckBox *tuner_agc = memnew(CheckBox);
    tuner_agc->set_name("TunerAGCCheckBox");
    tuner_agc->set_text("TUNER AGC");
    add_child(tuner_agc);

    CheckBox *iq_corr = memnew(CheckBox);
    iq_corr->set_name("IQCorrectionCheckBox");
    iq_corr->set_text("IQ Correction");
    iq_corr->set_pressed(true);
    add_child(iq_corr);

    HBoxContainer *sample_rate_row = memnew(HBoxContainer);
    sample_rate_row->set_name("SampleRateRow");
    sample_rate_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    sample_rate_row->add_theme_constant_override("separation", 8);
    add_child(sample_rate_row);

    Label *sample_label = memnew(Label);
    sample_label->set_name("SampleRateLabel");
    sample_label->set_text("Sample Rate");
    sample_rate_row->add_child(sample_label);

    PanelContainer *sample_rate_panel = memnew(PanelContainer);
    sample_rate_panel->set_name("SampleRatePanel");
    sample_rate_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    sample_rate_row->add_child(sample_rate_panel);

    DigitNumberSelector *sample = memnew(DigitNumberSelector);
    sample->set_name("SampleRateSelector");
    sample->set_limits(sample_rate_min_default, sample_rate_max_default);
    sample->set_digit_count(12);
    sample->set_group_size(3);
    sample->set_show_group_labels(true);
    sample->set_show_separators(true);
    sample->set_suffix_text("");
    PackedStringArray sample_labels;
    sample_labels.push_back("GHz");
    sample_labels.push_back("MHz");
    sample_labels.push_back("kHz");
    sample_labels.push_back("Hz");
    sample->set_group_labels(sample_labels);
    sample->set_value_no_signal(sample_rate_value);
    sample_rate_panel->add_child(sample);

    HBoxContainer *gain_header_row = memnew(HBoxContainer);
    gain_header_row->set_name("GainHeaderRow");
    gain_header_row->add_theme_constant_override("separation", 6);
    add_child(gain_header_row);

    Label *gain_label = memnew(Label);
    gain_label->set_name("GainLabel");
    gain_label->set_text("Gain");
    gain_header_row->add_child(gain_label);

    Control *gain_spacer = memnew(Control);
    gain_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    gain_header_row->add_child(gain_spacer);

    Label *gain_min = memnew(Label);
    gain_min->set_name("GainMinLabel");
    style_metric_label(gain_min);
    gain_header_row->add_child(gain_min);

    SpinBox *gain_value = memnew(SpinBox);
    gain_value->set_name("GainValueSpin");
    gain_value->set_min(0.0);
    gain_value->set_max(49.6);
    gain_value->set_step(0.1);
    gain_value->set_custom_minimum_size(Vector2(82.0, 0.0));
    gain_header_row->add_child(gain_value);

    Label *gain_max = memnew(Label);
    gain_max->set_name("GainMaxLabel");
    style_metric_label(gain_max);
    gain_header_row->add_child(gain_max);

    HSlider *gain = memnew(HSlider);
    gain->set_name("GainSlider");
    gain->set_min(0.0);
    gain->set_max(49.6);
    gain->set_step(0.1);
    gain->set_value(40.0);
    add_child(gain);

    configure_slots();
}
