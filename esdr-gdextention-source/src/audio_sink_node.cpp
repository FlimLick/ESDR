#include "audio_sink_node.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace godot;

namespace {
void log_audio_sink(const String &p_message) {
    UtilityFunctions::print(String("[ESDR][Audio Sink] ") + p_message);
}
} // namespace

void AudioSink::_bind_methods() {
    ClassDB::bind_method(D_METHOD("push_audio_frame", "frame"), &AudioSink::push_audio_frame);
    ClassDB::bind_method(D_METHOD("push_audio_stereo_frame", "frame"), &AudioSink::push_audio_stereo_frame);
    ClassDB::bind_method(D_METHOD("push_audio_frame_from_source", "frame", "source_id"), &AudioSink::push_audio_frame_from_source);
    ClassDB::bind_method(D_METHOD("push_audio_stereo_frame_from_source", "frame", "source_id"), &AudioSink::push_audio_stereo_frame_from_source);

    ClassDB::bind_method(D_METHOD("_on_volume_changed", "value"), &AudioSink::_on_volume_changed);
    ClassDB::bind_method(D_METHOD("_on_mute_toggled", "enabled"), &AudioSink::_on_mute_toggled);
    ClassDB::bind_method(D_METHOD("_on_clear_pressed"), &AudioSink::_on_clear_pressed);
    ClassDB::bind_method(D_METHOD("_on_device_selected", "index"), &AudioSink::_on_device_selected);
    ClassDB::bind_method(D_METHOD("set_debug_logging_enabled", "enabled"), &AudioSink::set_debug_logging_enabled);
    ClassDB::bind_method(D_METHOD("get_debug_logging_enabled"), &AudioSink::get_debug_logging_enabled);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &AudioSink::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &AudioSink::set_port_value);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_logging_enabled"), "set_debug_logging_enabled", "get_debug_logging_enabled");
}

void AudioSink::_notification(int32_t p_what) {
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

void AudioSink::on_ready() {
    ensure_ui();
    cache_ui_refs();
    bind_ui();
    refresh_output_devices();
    refresh_volume_header();
    update_status();

    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }

    generator.instantiate();
    if (generator.is_null()) {
        return;
    }

    const int32_t engine_mix_rate = AudioServer::get_singleton() != nullptr ? AudioServer::get_singleton()->get_mix_rate() : 0;
    if (engine_mix_rate > 0) {
        mix_rate = engine_mix_rate;
    }

    generator->set_mix_rate(static_cast<float>(mix_rate));
    // Larger generator buffer helps absorb temporary UI stalls.
    generator->set_buffer_length(1.0);
    // Keep multiple seconds of queue headroom before dropping.
    max_queued_samples = std::max(16384, mix_rate * 6);

    if (player != nullptr) {
        player->set_stream(generator);
        player->play();
        playback = player->get_stream_playback();
    }

    set_process(true);
    debug_last_log_us = 0;
    debug_in_mono_samples = 0;
    debug_in_stereo_samples = 0;
    debug_out_samples = 0;
    debug_drop_samples = 0;
    debug_in_clip_samples = 0;
    debug_out_clip_samples = 0;
    debug_source_samples.clear();
}

void AudioSink::on_exit_tree() {
    if (player != nullptr) {
        player->stop();
    }
    if (playback.is_valid()) {
        playback->clear_buffer();
    }

    std::lock_guard<std::mutex> lock(queue_mutex);
    sample_queue.clear();
}

void AudioSink::process_tick() {
    if (playback.is_null()) {
        return;
    }

    int32_t frames_available = playback->get_frames_available();
    if (frames_available <= 0) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (frames_available > 0 && !sample_queue.empty()) {
            const Vector2 sample = sample_queue.front();
            sample_queue.pop_front();
            if (std::abs(sample.x) >= 0.995f || std::abs(sample.y) >= 0.995f) {
                debug_out_clip_samples++;
            }
            debug_out_samples++;
            playback->push_frame(sample);
            frames_available--;
        }
    }

    update_status();
    log_debug_stats_if_due();
}

void AudioSink::push_audio_frame(const PackedFloat32Array &p_frame) {
    push_audio_frame_from_source(p_frame, StringName());
}

void AudioSink::push_audio_stereo_frame(const PackedVector2Array &p_frame) {
    push_audio_stereo_frame_from_source(p_frame, StringName());
}

void AudioSink::push_audio_frame_from_source(const PackedFloat32Array &p_frame, const StringName &p_source_id) {
    if (p_frame.is_empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(queue_mutex);
    debug_in_mono_samples += p_frame.size();
    record_source_samples(p_source_id, p_frame.size());
    for (int32_t i = 0; i < p_frame.size(); i++) {
        const float sample = std::clamp(p_frame[i], -1.0f, 1.0f);
        if (std::abs(sample) >= 0.995f) {
            debug_in_clip_samples++;
        }
        sample_queue.push_back(Vector2(sample, sample));
    }

    while (static_cast<int32_t>(sample_queue.size()) > max_queued_samples) {
        sample_queue.pop_front();
        debug_drop_samples++;
    }
}

void AudioSink::push_audio_stereo_frame_from_source(const PackedVector2Array &p_frame, const StringName &p_source_id) {
    if (p_frame.is_empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(queue_mutex);
    debug_in_stereo_samples += p_frame.size();
    record_source_samples(p_source_id, p_frame.size());
    for (int32_t i = 0; i < p_frame.size(); i++) {
        const Vector2 pair = p_frame[i];
        const float left = std::clamp(pair.x, -1.0f, 1.0f);
        const float right = std::clamp(pair.y, -1.0f, 1.0f);
        if (std::abs(left) >= 0.995f || std::abs(right) >= 0.995f) {
            debug_in_clip_samples++;
        }
        sample_queue.push_back(Vector2(left, right));
    }

    while (static_cast<int32_t>(sample_queue.size()) > max_queued_samples) {
        sample_queue.pop_front();
        debug_drop_samples++;
    }
}

void AudioSink::record_source_samples(const StringName &p_source_id, int32_t p_samples) {
    const String source_text = String(p_source_id);
    const std::string key = source_text.is_empty() ? std::string("<unknown>") : std::string(source_text.utf8().get_data());
    debug_source_samples[key] += p_samples;
}

void AudioSink::log_debug_stats_if_due() {
    if (!debug_logging_enabled) {
        debug_last_log_us = 0;
        debug_in_mono_samples = 0;
        debug_in_stereo_samples = 0;
        debug_out_samples = 0;
        debug_drop_samples = 0;
        debug_in_clip_samples = 0;
        debug_out_clip_samples = 0;
        debug_source_samples.clear();
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

    const double in_mono_rate = static_cast<double>(debug_in_mono_samples) / dt_sec;
    const double in_stereo_rate = static_cast<double>(debug_in_stereo_samples) / dt_sec;
    const double out_rate = static_cast<double>(debug_out_samples) / dt_sec;
    const double in_total_rate = in_mono_rate + in_stereo_rate;
    const double rate_delta = in_total_rate - out_rate;

    int64_t queue_depth = 0;
    std::vector<std::pair<std::string, int64_t>> top_sources;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue_depth = static_cast<int64_t>(sample_queue.size());
        top_sources.reserve(debug_source_samples.size());
        for (const auto &entry : debug_source_samples) {
            top_sources.push_back(entry);
        }
    }

    std::sort(top_sources.begin(), top_sources.end(), [](const auto &a, const auto &b) {
        return a.second > b.second;
    });
    String source_text;
    const int32_t max_sources = std::min<int32_t>(3, static_cast<int32_t>(top_sources.size()));
    for (int32_t i = 0; i < max_sources; i++) {
        if (i > 0) {
            source_text += ", ";
        }
        source_text += String(top_sources[static_cast<size_t>(i)].first.c_str()) + ":" + String::num_int64(top_sources[static_cast<size_t>(i)].second);
    }

    log_audio_sink(String("stats dt=") + String::num(dt_sec, 3) +
            " in_mono_sps=" + String::num(in_mono_rate, 1) +
            " in_stereo_frames_sps=" + String::num(in_stereo_rate, 1) +
            " out_sps=" + String::num(out_rate, 1) +
            " rate_delta=" + String::num(rate_delta, 1) +
            " queue=" + String::num_int64(queue_depth) +
            " dropped=" + String::num_int64(debug_drop_samples) +
            " in_clip=" + String::num_int64(debug_in_clip_samples) +
            " out_clip=" + String::num_int64(debug_out_clip_samples) +
            " sources={" + source_text + "}");

    debug_in_mono_samples = 0;
    debug_in_stereo_samples = 0;
    debug_out_samples = 0;
    debug_drop_samples = 0;
    debug_in_clip_samples = 0;
    debug_out_clip_samples = 0;
    debug_source_samples.clear();
}

void AudioSink::_on_volume_changed(double p_value) {
    if (volume_slider != nullptr && std::abs(volume_slider->get_value() - p_value) > 1e-9) {
        volume_slider->set_value_no_signal(p_value);
    }
    if (player != nullptr) {
        player->set_volume_db(static_cast<float>(p_value));
    }
    refresh_volume_header();
}

void AudioSink::_on_mute_toggled(bool p_enabled) {
    if (player != nullptr) {
        player->set_stream_paused(p_enabled);
    }
}

void AudioSink::_on_clear_pressed() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        sample_queue.clear();
    }
    if (playback.is_valid()) {
        playback->clear_buffer();
    }
    update_status();
}

int32_t AudioSink::wrap_index(int32_t p_index, int32_t p_count) const {
    if (p_count <= 0) {
        return 0;
    }

    int32_t wrapped = p_index % p_count;
    if (wrapped < 0) {
        wrapped += p_count;
    }
    return wrapped;
}

Variant AudioSink::get_port_value(int64_t p_port) const {
    switch (p_port) {
        case 0:
            if (device_selector != nullptr && device_selector->get_item_count() > 0) {
                return device_selector->get_selected();
            }
            return 0;
        case 3:
            return volume_slider != nullptr ? volume_slider->get_value() : 0.0;
        case 4:
            return mute_checkbox != nullptr ? mute_checkbox->is_pressed() : false;
        default:
            return Variant();
    }
}

void AudioSink::set_port_value(int64_t p_port, const Variant &p_value) {
    switch (p_port) {
        case 0: {
            if (device_selector == nullptr || device_selector->get_item_count() <= 0) {
                return;
            }
            const int32_t index = wrap_index(static_cast<int32_t>(p_value), device_selector->get_item_count());
            device_selector->select(index);
            _on_device_selected(index);
            break;
        }
        case 3: {
            const double value = static_cast<double>(p_value);
            if (volume_slider != nullptr) {
                volume_slider->set_value_no_signal(value);
            }
            _on_volume_changed(value);
            break;
        }
        case 4: {
            const bool enabled = static_cast<bool>(p_value);
            if (mute_checkbox != nullptr) {
                mute_checkbox->set_pressed_no_signal(enabled);
            }
            _on_mute_toggled(enabled);
            break;
        }
        default:
            break;
    }
}

void AudioSink::configure_slots() {
    clear_all_slots();
    set_slot(0, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(1, true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(2, false, 0, Color(1.0, 1.0, 1.0, 1.0), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(3, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(4, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(5, false, 0, Color(1.0, 1.0, 1.0, 1.0), false, 0, Color(1.0, 1.0, 1.0, 1.0));
}

void AudioSink::ensure_ui() {
    if (has_node(NodePath("DeviceRow/DeviceSelector")) && has_node(NodePath("StatusRow/StatusLabel")) && has_node(NodePath("VolumeSlider"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("Audio Sink");
    set_custom_minimum_size(Vector2(400, 300));
    set_resizable(true);

    HBoxContainer *device_row = memnew(HBoxContainer);
    device_row->set_name("DeviceRow");
    add_child(device_row);

    Label *device_label = memnew(Label);
    device_label->set_name("DeviceLabel");
    device_label->set_text("Device");
    device_row->add_child(device_label);

    OptionButton *device_select = memnew(OptionButton);
    device_select->set_name("DeviceSelector");
    device_select->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    device_select->set_fit_to_longest_item(false);
    device_select->set_clip_text(true);
    device_row->add_child(device_select);

    HBoxContainer *status_row = memnew(HBoxContainer);
    status_row->set_name("StatusRow");
    add_child(status_row);

    Label *audio_label = memnew(Label);
    audio_label->set_name("AudioLabel");
    audio_label->set_text("Audio In");
    status_row->add_child(audio_label);

    Label *status = memnew(Label);
    status->set_name("StatusLabel");
    status->set_text("Buffered: 0");
    status->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    status_row->add_child(status);

    HBoxContainer *volume_header_row = memnew(HBoxContainer);
    volume_header_row->set_name("VolumeHeaderRow");
    volume_header_row->add_theme_constant_override("separation", 6);
    add_child(volume_header_row);

    Label *volume_label = memnew(Label);
    volume_label->set_name("VolumeLabel");
    volume_label->set_text("Volume");
    volume_header_row->add_child(volume_label);

    Control *volume_spacer = memnew(Control);
    volume_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    volume_header_row->add_child(volume_spacer);

    Label *volume_min = memnew(Label);
    volume_min->set_name("VolumeMinLabel");
    volume_header_row->add_child(volume_min);

    SpinBox *volume_value = memnew(SpinBox);
    volume_value->set_name("VolumeValueSpin");
    volume_value->set_min(-60.0);
    volume_value->set_max(6.0);
    volume_value->set_step(0.1);
    volume_value->set_custom_minimum_size(Vector2(86.0, 0.0));
    volume_header_row->add_child(volume_value);

    Label *volume_max = memnew(Label);
    volume_max->set_name("VolumeMaxLabel");
    volume_header_row->add_child(volume_max);

    HSlider *volume = memnew(HSlider);
    volume->set_name("VolumeSlider");
    volume->set_min(-60.0);
    volume->set_max(6.0);
    volume->set_value(0.0);
    add_child(volume);

    HBoxContainer *controls_row = memnew(HBoxContainer);
    controls_row->set_name("ControlsRow");
    add_child(controls_row);

    CheckBox *mute = memnew(CheckBox);
    mute->set_name("MuteCheckBox");
    mute->set_text("Mute");
    controls_row->add_child(mute);

    Button *clear = memnew(Button);
    clear->set_name("ClearButton");
    clear->set_text("Clear Buffer");
    controls_row->add_child(clear);

    AudioStreamPlayer *audio_player = memnew(AudioStreamPlayer);
    audio_player->set_name("Player");
    add_child(audio_player);

    configure_slots();
}

void AudioSink::cache_ui_refs() {
    player = Object::cast_to<AudioStreamPlayer>(get_node_or_null(NodePath("Player")));
    device_row = Object::cast_to<Control>(get_node_or_null(NodePath("DeviceRow")));
    device_label = Object::cast_to<Label>(get_node_or_null(NodePath("DeviceRow/DeviceLabel")));
    device_selector = Object::cast_to<OptionButton>(get_node_or_null(NodePath("DeviceRow/DeviceSelector")));
    volume_slider = Object::cast_to<HSlider>(get_node_or_null(NodePath("VolumeSlider")));
    volume_min_label = Object::cast_to<Label>(get_node_or_null(NodePath("VolumeHeaderRow/VolumeMinLabel")));
    volume_value_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("VolumeHeaderRow/VolumeValueSpin")));
    volume_max_label = Object::cast_to<Label>(get_node_or_null(NodePath("VolumeHeaderRow/VolumeMaxLabel")));
    mute_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("ControlsRow/MuteCheckBox")));
    clear_button = Object::cast_to<Button>(get_node_or_null(NodePath("ControlsRow/ClearButton")));
    status_label = Object::cast_to<Label>(get_node_or_null(NodePath("StatusRow/StatusLabel")));
}

void AudioSink::bind_ui() {
    if (device_selector != nullptr) {
        const Callable cb(this, "_on_device_selected");
        if (!device_selector->is_connected("item_selected", cb)) {
            device_selector->connect("item_selected", cb);
        }
    }

    if (volume_slider != nullptr) {
        const Callable cb(this, "_on_volume_changed");
        if (!volume_slider->is_connected("value_changed", cb)) {
            volume_slider->connect("value_changed", cb);
        }
    }
    if (volume_value_spin != nullptr) {
        const Callable cb(this, "_on_volume_changed");
        if (!volume_value_spin->is_connected("value_changed", cb)) {
            volume_value_spin->connect("value_changed", cb);
        }
    }

    if (mute_checkbox != nullptr) {
        const Callable cb(this, "_on_mute_toggled");
        if (!mute_checkbox->is_connected("toggled", cb)) {
            mute_checkbox->connect("toggled", cb);
        }
    }

    if (clear_button != nullptr) {
        const Callable cb(this, "_on_clear_pressed");
        if (!clear_button->is_connected("pressed", cb)) {
            clear_button->connect("pressed", cb);
        }
    }
}

void AudioSink::refresh_output_devices() {
    if (device_selector == nullptr) {
        return;
    }

    device_selector->clear();

    AudioServer *audio_server = AudioServer::get_singleton();
    if (audio_server == nullptr) {
        if (device_row != nullptr) {
            device_row->set_visible(false);
        }
        if (device_label != nullptr) {
            device_label->set_visible(false);
        }
        device_selector->set_visible(false);
        configure_slots();
        return;
    }

    const PackedStringArray devices = audio_server->get_output_device_list();
    if (devices.is_empty()) {
        if (device_row != nullptr) {
            device_row->set_visible(false);
        }
        if (device_label != nullptr) {
            device_label->set_visible(false);
        }
        device_selector->set_visible(false);
        configure_slots();
        return;
    }

    const String active_device = audio_server->get_output_device();
    int32_t selected_index = 0;
    for (int32_t i = 0; i < devices.size(); i++) {
        device_selector->add_item(devices[i]);
        if (devices[i] == active_device) {
            selected_index = i;
        }
    }

    device_selector->select(selected_index);
    device_selector->set_fit_to_longest_item(false);
    device_selector->set_clip_text(true);
    const bool show_selector = devices.size() > 1;
    if (device_row != nullptr) {
        device_row->set_visible(show_selector);
    }
    if (device_label != nullptr) {
        device_label->set_visible(show_selector);
    }
    device_selector->set_visible(show_selector);
    configure_slots();
}

void AudioSink::refresh_volume_header() {
    if (volume_slider == nullptr) {
        return;
    }

    if (volume_min_label != nullptr) {
        volume_min_label->set_text(String("min ") + String::num(volume_slider->get_min()));
        volume_min_label->add_theme_font_size_override("font_size", 10);
        volume_min_label->add_theme_color_override("font_color", Color(0.67, 0.67, 0.67, 1.0));
    }
    if (volume_value_spin != nullptr) {
        volume_value_spin->set_min(volume_slider->get_min());
        volume_value_spin->set_max(volume_slider->get_max());
        volume_value_spin->set_step(0.1);
        volume_value_spin->set_value_no_signal(volume_slider->get_value());
    }
    if (volume_max_label != nullptr) {
        volume_max_label->set_text(String("max ") + String::num(volume_slider->get_max()));
        volume_max_label->add_theme_font_size_override("font_size", 10);
        volume_max_label->add_theme_color_override("font_color", Color(0.67, 0.67, 0.67, 1.0));
    }
}

void AudioSink::_on_device_selected(int64_t p_index) {
    if (device_selector == nullptr) {
        return;
    }

    const int32_t index = static_cast<int32_t>(p_index);
    if (index < 0 || index >= device_selector->get_item_count()) {
        return;
    }

    AudioServer *audio_server = AudioServer::get_singleton();
    if (audio_server == nullptr) {
        return;
    }

    audio_server->set_output_device(device_selector->get_item_text(index));
}

void AudioSink::set_debug_logging_enabled(bool p_enabled) {
    debug_logging_enabled = p_enabled;
}

bool AudioSink::get_debug_logging_enabled() const {
    return debug_logging_enabled;
}

void AudioSink::update_status() {
    if (status_label == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(queue_mutex);
    status_label->set_text(String("Buffered: ") + String::num_int64(sample_queue.size()));
}
