#include "audio_replay_node.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

void AudioReplay::_bind_methods() {
    ClassDB::bind_method(D_METHOD("push_audio_frame", "frame"), &AudioReplay::push_audio_frame);
    ClassDB::bind_method(D_METHOD("push_audio_stereo_frame", "frame"), &AudioReplay::push_audio_stereo_frame);
    ClassDB::bind_method(D_METHOD("push_audio_frame_from_source", "frame", "source_id"), &AudioReplay::push_audio_frame_from_source);
    ClassDB::bind_method(D_METHOD("push_audio_stereo_frame_from_source", "frame", "source_id"), &AudioReplay::push_audio_stereo_frame_from_source);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &AudioReplay::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &AudioReplay::set_port_value);

    ClassDB::bind_method(D_METHOD("_on_sample_rate_changed", "value"), &AudioReplay::_on_sample_rate_changed);
    ClassDB::bind_method(D_METHOD("_on_buffer_seconds_changed", "value"), &AudioReplay::_on_buffer_seconds_changed);
    ClassDB::bind_method(D_METHOD("_on_record_toggled", "enabled"), &AudioReplay::_on_record_toggled);
    ClassDB::bind_method(D_METHOD("_on_play_toggled", "enabled"), &AudioReplay::_on_play_toggled);
    ClassDB::bind_method(D_METHOD("_on_passthrough_toggled", "enabled"), &AudioReplay::_on_passthrough_toggled);
    ClassDB::bind_method(D_METHOD("_on_playback_slider_changed", "value"), &AudioReplay::_on_playback_slider_changed);
    ClassDB::bind_method(D_METHOD("_on_clear_pressed"), &AudioReplay::_on_clear_pressed);
    ClassDB::bind_method(D_METHOD("_on_save_pressed"), &AudioReplay::_on_save_pressed);

    ADD_SIGNAL(MethodInfo("audio_frame", PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "frame")));
}

void AudioReplay::_notification(int32_t p_what) {
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

void AudioReplay::on_ready() {
    ensure_ui();
    cache_ui_refs();
    bind_ui();

    const int32_t engine_mix_rate = AudioServer::get_singleton() != nullptr ? AudioServer::get_singleton()->get_mix_rate() : 0;
    if (engine_mix_rate > 0) {
        sample_rate_hz = engine_mix_rate;
    }

    set_sample_rate_hz(sample_rate_hz);
    set_buffer_seconds(buffer_seconds);
    set_record_enabled(record_enabled);
    set_play_enabled(false);
    set_passthrough_enabled(passthrough_enabled);
    refresh_playback_slider_range();

    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }

    set_process(true);
}

void AudioReplay::on_exit_tree() {
    play_enabled = false;
    playback_pending_samples = 0.0;
    playback_offset = 0;
    last_live_frame = PackedFloat32Array();
}

void AudioReplay::process_tick() {
    if (!play_enabled || Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    if (ring_stored_samples == 0 || sample_rate_hz <= 0) {
        set_play_enabled(false);
        return;
    }

    const double delta = get_process_delta_time();
    if (delta <= 0.0) {
        return;
    }

    playback_pending_samples += static_cast<double>(sample_rate_hz) * delta;
    int32_t safety_counter = 0;
    while (playback_pending_samples >= 1.0 && play_enabled && safety_counter < 6) {
        const int32_t samples_requested = static_cast<int32_t>(std::floor(playback_pending_samples));
        const int32_t remaining = static_cast<int32_t>(ring_stored_samples - playback_offset);
        const int32_t chunk = std::max(0, std::min({samples_requested, playback_chunk_size, remaining}));
        if (chunk <= 0) {
            set_play_enabled(false);
            break;
        }

        PackedFloat32Array out_frame;
        out_frame.resize(chunk);
        float *out_ptr = out_frame.ptrw();
        for (int32_t i = 0; i < chunk; i++) {
            const std::size_t ordered_index = playback_offset + static_cast<std::size_t>(i);
            out_ptr[i] = ring_samples[ordered_to_ring_index(ordered_index)];
        }

        emit_signal("audio_frame", out_frame);
        set_playback_offset(playback_offset + static_cast<std::size_t>(chunk));
        playback_pending_samples -= static_cast<double>(chunk);
        safety_counter++;

        if (playback_offset >= ring_stored_samples) {
            set_play_enabled(false);
        }
    }

    update_status_labels();
}

void AudioReplay::set_sample_rate_hz(int64_t p_sample_rate_hz) {
    sample_rate_hz = std::clamp<int64_t>(p_sample_rate_hz, 1000LL, 384000LL);
    if (sample_rate_spin != nullptr) {
        sample_rate_spin->set_value_no_signal(static_cast<double>(sample_rate_hz));
    }

    const std::size_t new_capacity = static_cast<std::size_t>(std::llround(buffer_seconds * static_cast<double>(sample_rate_hz)));
    resize_ring_preserving_latest(std::max<std::size_t>(1, new_capacity));
}

void AudioReplay::set_buffer_seconds(double p_seconds) {
    buffer_seconds = std::clamp(p_seconds, 0.1, 3600.0);
    if (buffer_seconds_spin != nullptr) {
        buffer_seconds_spin->set_value_no_signal(buffer_seconds);
    }

    const std::size_t new_capacity = static_cast<std::size_t>(std::llround(buffer_seconds * static_cast<double>(sample_rate_hz)));
    resize_ring_preserving_latest(std::max<std::size_t>(1, new_capacity));
}

void AudioReplay::set_record_enabled(bool p_enabled) {
    record_enabled = p_enabled;
    if (record_button != nullptr) {
        record_button->set_pressed_no_signal(record_enabled);
    }
    update_status_labels();
}

void AudioReplay::set_play_enabled(bool p_enabled) {
    const bool was_play_enabled = play_enabled;
    play_enabled = p_enabled && ring_stored_samples > 0;
    if (play_enabled) {
        if (playback_offset >= ring_stored_samples) {
            playback_offset = 0;
        }
        playback_pending_samples = 0.0;
        if (record_enabled) {
            set_record_enabled(false);
        }
    } else {
        playback_pending_samples = 0.0;
    }
    if (play_button != nullptr) {
        play_button->set_pressed_no_signal(play_enabled);
    }
    if (was_play_enabled && !play_enabled && passthrough_enabled && !last_live_frame.is_empty()) {
        emit_signal("audio_frame", last_live_frame);
    }
    update_status_labels();
}

void AudioReplay::set_passthrough_enabled(bool p_enabled) {
    passthrough_enabled = p_enabled;
    if (passthrough_button != nullptr) {
        passthrough_button->set_pressed_no_signal(passthrough_enabled);
    }
    update_status_labels();
}

void AudioReplay::set_playback_offset(std::size_t p_offset, bool p_from_slider) {
    if (ring_stored_samples == 0) {
        playback_offset = 0;
    } else {
        playback_offset = std::min<std::size_t>(p_offset, ring_stored_samples - 1);
    }
    if (p_from_slider) {
        playback_pending_samples = 0.0;
    }
    update_status_labels();
}

void AudioReplay::refresh_playback_slider_range() {
    if (playback_slider == nullptr) {
        return;
    }
    playback_slider->set_min(0.0);
    const double max_value = ring_stored_samples > 0 ? static_cast<double>(ring_stored_samples - 1) : 0.0;
    playback_slider->set_max(max_value);
    playback_slider->set_step(1.0);
    playback_slider->set_editable(ring_stored_samples > 0);
    playback_slider->set_value_no_signal(static_cast<double>(std::min<std::size_t>(playback_offset, ring_stored_samples > 0 ? ring_stored_samples - 1 : 0)));
}

void AudioReplay::resize_ring_preserving_latest(std::size_t p_new_capacity) {
    const std::size_t new_capacity = std::max<std::size_t>(1, p_new_capacity);
    if (new_capacity == ring_capacity_samples) {
        update_status_labels();
        return;
    }

    const PackedFloat32Array ordered = export_ordered_capture();
    const std::size_t keep_count = std::min<std::size_t>(new_capacity, static_cast<std::size_t>(ordered.size()));

    ring_samples.clear();
    ring_samples.resize(new_capacity);
    ring_capacity_samples = new_capacity;
    ring_write_index = 0;
    ring_stored_samples = 0;

    if (keep_count > 0) {
        const int64_t start = static_cast<int64_t>(ordered.size()) - static_cast<int64_t>(keep_count);
        const float *src = ordered.ptr();
        for (std::size_t i = 0; i < keep_count; i++) {
            append_ring_sample(src[start + static_cast<int64_t>(i)]);
        }
    }

    if (play_enabled && playback_offset >= ring_stored_samples) {
        set_play_enabled(false);
    }
    if (!play_enabled && playback_offset >= ring_stored_samples) {
        playback_offset = ring_stored_samples > 0 ? ring_stored_samples - 1 : 0;
    }
    refresh_playback_slider_range();
    update_status_labels();
}

void AudioReplay::append_ring_sample(float p_sample) {
    if (ring_capacity_samples == 0) {
        return;
    }
    ring_samples[ring_write_index] = p_sample;
    ring_write_index = (ring_write_index + 1) % ring_capacity_samples;
    ring_stored_samples = std::min<std::size_t>(ring_stored_samples + 1, ring_capacity_samples);
}

void AudioReplay::append_ring_frame(const PackedFloat32Array &p_frame) {
    const float *src = p_frame.ptr();
    for (int32_t i = 0; i < p_frame.size(); i++) {
        append_ring_sample(std::clamp(src[i], -1.0f, 1.0f));
    }
}

std::size_t AudioReplay::ordered_to_ring_index(std::size_t p_ordered_index) const {
    if (ring_capacity_samples == 0 || ring_stored_samples == 0) {
        return 0;
    }
    const std::size_t oldest = (ring_write_index + ring_capacity_samples - ring_stored_samples) % ring_capacity_samples;
    return (oldest + p_ordered_index) % ring_capacity_samples;
}

PackedFloat32Array AudioReplay::export_ordered_capture() const {
    PackedFloat32Array out;
    out.resize(static_cast<int32_t>(ring_stored_samples));
    float *out_ptr = out.ptrw();
    for (std::size_t i = 0; i < ring_stored_samples; i++) {
        out_ptr[static_cast<int32_t>(i)] = ring_samples[ordered_to_ring_index(i)];
    }
    return out;
}

String AudioReplay::format_bytes(double p_bytes) {
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = std::max(0.0, p_bytes);
    int32_t unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }
    return String::num(value, unit == 0 ? 0 : 2) + " " + units[unit];
}

void AudioReplay::update_status_labels() {
    if (status_label != nullptr) {
        String mode = "IDLE";
        if (play_enabled) {
            mode = "PLAY";
        } else if (record_enabled) {
            mode = "REC";
        }

        const double used_seconds = sample_rate_hz > 0 ? static_cast<double>(ring_stored_samples) / static_cast<double>(sample_rate_hz) : 0.0;
        const String passthrough_text = passthrough_enabled ? "ON" : "OFF";
        status_label->set_text(String("State: ") + mode + "  PT: " + passthrough_text + "  Used: " + String::num(used_seconds, 2) + " s");
    }

    if (memory_label != nullptr) {
        const double used_bytes = static_cast<double>(ring_stored_samples) * 4.0;
        const double capacity_bytes = static_cast<double>(ring_capacity_samples) * 4.0;
        memory_label->set_text(String("Memory: ") + format_bytes(used_bytes) + " / " + format_bytes(capacity_bytes));
    }

    refresh_playback_slider_range();
}

void AudioReplay::save_capture_to_path(const String &p_path) {
    const PackedFloat32Array ordered = export_ordered_capture();
    if (ordered.is_empty()) {
        set_problem_state(true, "No audio samples captured.");
        update_status_labels();
        return;
    }

    const String path = p_path.strip_edges();
    if (path.is_empty()) {
        set_problem_state(true, "Save path is empty.");
        update_status_labels();
        return;
    }

    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
    if (file.is_null()) {
        set_problem_state(true, String("Failed to open save path: ") + path);
        update_status_labels();
        return;
    }

    file->store_32(0x45534452); // ESDR
    file->store_32(0x41553130); // AU10
    file->store_64(sample_rate_hz);
    file->store_64(static_cast<int64_t>(ordered.size()));

    const float *src = ordered.ptr();
    for (int32_t i = 0; i < ordered.size(); i++) {
        file->store_float(src[i]);
    }
    file->flush();

    set_problem_state(false, String());
    if (status_label != nullptr) {
        status_label->set_text(String("Saved: ") + String::num_int64(ordered.size()) + " samples");
    }
    update_status_labels();
}

void AudioReplay::_on_sample_rate_changed(double p_value) {
    set_sample_rate_hz(static_cast<int64_t>(std::llround(p_value)));
}

void AudioReplay::_on_buffer_seconds_changed(double p_value) {
    set_buffer_seconds(p_value);
}

void AudioReplay::_on_record_toggled(bool p_enabled) {
    set_record_enabled(p_enabled);
}

void AudioReplay::_on_play_toggled(bool p_enabled) {
    set_play_enabled(p_enabled);
}

void AudioReplay::_on_passthrough_toggled(bool p_enabled) {
    set_passthrough_enabled(p_enabled);
}

void AudioReplay::_on_playback_slider_changed(double p_value) {
    if (ring_stored_samples == 0) {
        set_playback_offset(0, true);
        return;
    }
    const std::size_t target = static_cast<std::size_t>(std::llround(std::max(0.0, p_value)));
    set_playback_offset(target, true);
}

void AudioReplay::_on_clear_pressed() {
    ring_write_index = 0;
    ring_stored_samples = 0;
    playback_offset = 0;
    playback_pending_samples = 0.0;
    set_play_enabled(false);
    refresh_playback_slider_range();
    update_status_labels();
}

void AudioReplay::_on_save_pressed() {
    if (save_path_edit == nullptr) {
        return;
    }
    save_capture_to_path(save_path_edit->get_text());
}

void AudioReplay::push_audio_frame(const PackedFloat32Array &p_frame) {
    if (p_frame.is_empty()) {
        return;
    }

    last_live_frame = p_frame;

    if (record_enabled) {
        append_ring_frame(p_frame);
    }

    if (!play_enabled && passthrough_enabled) {
        emit_signal("audio_frame", p_frame);
    }

    update_status_labels();
}

void AudioReplay::push_audio_stereo_frame(const PackedVector2Array &p_frame) {
    if (p_frame.is_empty()) {
        return;
    }

    PackedFloat32Array mono;
    mono.resize(p_frame.size());
    float *dst = mono.ptrw();
    const Vector2 *src = p_frame.ptr();
    for (int32_t i = 0; i < p_frame.size(); i++) {
        dst[i] = std::clamp((src[i].x + src[i].y) * 0.5f, -1.0f, 1.0f);
    }
    push_audio_frame(mono);
}

void AudioReplay::push_audio_frame_from_source(const PackedFloat32Array &p_frame, const StringName & /*p_source_id*/) {
    push_audio_frame(p_frame);
}

void AudioReplay::push_audio_stereo_frame_from_source(const PackedVector2Array &p_frame, const StringName & /*p_source_id*/) {
    push_audio_stereo_frame(p_frame);
}

Variant AudioReplay::get_port_value(int64_t p_port) const {
    switch (p_port) {
        case 0:
            return buffer_seconds;
        case 1:
            return sample_rate_hz;
        case 3:
            return record_enabled;
        case 4:
            return play_enabled;
        case 5:
            return passthrough_enabled;
        default:
            return Variant();
    }
}

void AudioReplay::set_port_value(int64_t p_port, const Variant &p_value) {
    switch (p_port) {
        case 0:
            set_buffer_seconds(static_cast<double>(p_value));
            break;
        case 1:
            set_sample_rate_hz(static_cast<int64_t>(p_value));
            break;
        case 2:
            if (p_value.get_type() == Variant::PACKED_FLOAT32_ARRAY) {
                push_audio_frame(p_value);
            } else if (p_value.get_type() == Variant::PACKED_VECTOR2_ARRAY) {
                push_audio_stereo_frame(p_value);
            }
            break;
        case 3:
            set_record_enabled(static_cast<bool>(p_value));
            break;
        case 4:
            set_play_enabled(static_cast<bool>(p_value));
            break;
        case 5:
            set_passthrough_enabled(static_cast<bool>(p_value));
            break;
        default:
            break;
    }
}

void AudioReplay::ensure_ui() {
    if (has_node(NodePath("BufferRow/BufferPanel/BufferSecondsSpin")) &&
            has_node(NodePath("PlayRow/PlayButton")) &&
            has_node(NodePath("PassthroughRow/PassthroughButton")) &&
            has_node(NodePath("TimelineRow/PlaybackSlider")) &&
            has_node(NodePath("SaveRow/SavePathPanel/SavePath"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("Audio Replay");
    set_custom_minimum_size(Vector2(420, 300));
    set_resizable(true);

    HBoxContainer *buffer_row = memnew(HBoxContainer);
    buffer_row->set_name("BufferRow");
    add_child(buffer_row);

    Label *buffer_label = memnew(Label);
    buffer_label->set_name("BufferLabel");
    buffer_label->set_text("Buffer Seconds");
    buffer_row->add_child(buffer_label);

    PanelContainer *buffer_panel = memnew(PanelContainer);
    buffer_panel->set_name("BufferPanel");
    buffer_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    buffer_row->add_child(buffer_panel);

    SpinBox *buffer = memnew(SpinBox);
    buffer->set_name("BufferSecondsSpin");
    buffer->set_min(0.1);
    buffer->set_max(3600.0);
    buffer->set_step(0.1);
    buffer->set_value(buffer_seconds);
    buffer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    buffer_panel->add_child(buffer);

    HBoxContainer *sample_rate_row = memnew(HBoxContainer);
    sample_rate_row->set_name("SampleRateRow");
    add_child(sample_rate_row);

    Label *sample_rate_label = memnew(Label);
    sample_rate_label->set_name("SampleRateLabel");
    sample_rate_label->set_text("Sample Rate");
    sample_rate_row->add_child(sample_rate_label);

    PanelContainer *sample_rate_panel = memnew(PanelContainer);
    sample_rate_panel->set_name("SampleRatePanel");
    sample_rate_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    sample_rate_row->add_child(sample_rate_panel);

    SpinBox *sample_rate = memnew(SpinBox);
    sample_rate->set_name("SampleRateSpin");
    sample_rate->set_min(1000.0);
    sample_rate->set_max(384000.0);
    sample_rate->set_step(100.0);
    sample_rate->set_value(static_cast<double>(sample_rate_hz));
    sample_rate->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    sample_rate_panel->add_child(sample_rate);

    HBoxContainer *audio_row = memnew(HBoxContainer);
    audio_row->set_name("AudioRow");
    add_child(audio_row);

    Label *audio_io = memnew(Label);
    audio_io->set_name("AudioIOLabel");
    audio_io->set_text("Audio I/O");
    audio_io->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    audio_io->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    audio_row->add_child(audio_io);

    HBoxContainer *record_row = memnew(HBoxContainer);
    record_row->set_name("RecordRow");
    record_row->add_theme_constant_override("separation", 6);
    add_child(record_row);

    Label *record_label = memnew(Label);
    record_label->set_name("RecordLabel");
    record_label->set_text("Record");
    record_row->add_child(record_label);

    Button *record = memnew(Button);
    record->set_name("RecordButton");
    record->set_toggle_mode(true);
    record->set_text("ON");
    record_row->add_child(record);

    HBoxContainer *play_row = memnew(HBoxContainer);
    play_row->set_name("PlayRow");
    play_row->add_theme_constant_override("separation", 6);
    add_child(play_row);

    Label *play_label = memnew(Label);
    play_label->set_name("PlayLabel");
    play_label->set_text("Play");
    play_row->add_child(play_label);

    Button *play = memnew(Button);
    play->set_name("PlayButton");
    play->set_toggle_mode(true);
    play->set_text("ON");
    play_row->add_child(play);

    HBoxContainer *passthrough_row = memnew(HBoxContainer);
    passthrough_row->set_name("PassthroughRow");
    passthrough_row->add_theme_constant_override("separation", 6);
    add_child(passthrough_row);

    Label *passthrough_label = memnew(Label);
    passthrough_label->set_name("PassthroughLabel");
    passthrough_label->set_text("Pass Through");
    passthrough_row->add_child(passthrough_label);

    Button *passthrough = memnew(Button);
    passthrough->set_name("PassthroughButton");
    passthrough->set_toggle_mode(true);
    passthrough->set_text("ON");
    passthrough_row->add_child(passthrough);

    HBoxContainer *save_row = memnew(HBoxContainer);
    save_row->set_name("SaveRow");
    add_child(save_row);

    Label *save_label = memnew(Label);
    save_label->set_name("SaveLabel");
    save_label->set_text("Path");
    save_row->add_child(save_label);

    PanelContainer *save_panel = memnew(PanelContainer);
    save_panel->set_name("SavePathPanel");
    save_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    save_row->add_child(save_panel);

    LineEdit *save_path = memnew(LineEdit);
    save_path->set_name("SavePath");
    save_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    save_path->set_text("user://audio_capture.esdrau");
    save_panel->add_child(save_path);

    Button *save = memnew(Button);
    save->set_name("SaveButton");
    save->set_text("Save");
    save_row->add_child(save);

    HBoxContainer *action_row = memnew(HBoxContainer);
    action_row->set_name("ActionRow");
    action_row->add_theme_constant_override("separation", 6);
    add_child(action_row);

    Button *clear = memnew(Button);
    clear->set_name("ClearButton");
    clear->set_text("Clear");
    action_row->add_child(clear);

    Label *status = memnew(Label);
    status->set_name("StatusLabel");
    status->set_text("State: IDLE");
    add_child(status);

    Label *memory = memnew(Label);
    memory->set_name("MemoryLabel");
    memory->set_text("Memory: 0 B / 0 B");
    add_child(memory);

    HBoxContainer *timeline_row = memnew(HBoxContainer);
    timeline_row->set_name("TimelineRow");
    timeline_row->add_theme_constant_override("separation", 6);
    add_child(timeline_row);

    Label *timeline_label = memnew(Label);
    timeline_label->set_name("TimelineLabel");
    timeline_label->set_text("Timeline");
    timeline_row->add_child(timeline_label);

    HSlider *timeline_slider = memnew(HSlider);
    timeline_slider->set_name("PlaybackSlider");
    timeline_slider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    timeline_slider->set_min(0.0);
    timeline_slider->set_max(0.0);
    timeline_slider->set_step(1.0);
    timeline_slider->set_value(0.0);
    timeline_row->add_child(timeline_slider);

    configure_slots();
}

void AudioReplay::cache_ui_refs() {
    buffer_seconds_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("BufferRow/BufferPanel/BufferSecondsSpin")));
    sample_rate_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("SampleRateRow/SampleRatePanel/SampleRateSpin")));
    record_button = Object::cast_to<Button>(get_node_or_null(NodePath("RecordRow/RecordButton")));
    play_button = Object::cast_to<Button>(get_node_or_null(NodePath("PlayRow/PlayButton")));
    passthrough_button = Object::cast_to<Button>(get_node_or_null(NodePath("PassthroughRow/PassthroughButton")));
    clear_button = Object::cast_to<Button>(get_node_or_null(NodePath("ActionRow/ClearButton")));
    save_button = Object::cast_to<Button>(get_node_or_null(NodePath("SaveRow/SaveButton")));
    playback_slider = Object::cast_to<HSlider>(get_node_or_null(NodePath("TimelineRow/PlaybackSlider")));
    save_path_edit = Object::cast_to<LineEdit>(get_node_or_null(NodePath("SaveRow/SavePathPanel/SavePath")));
    status_label = Object::cast_to<Label>(get_node_or_null(NodePath("StatusLabel")));
    memory_label = Object::cast_to<Label>(get_node_or_null(NodePath("MemoryLabel")));
}

void AudioReplay::bind_ui() {
    if (sample_rate_spin != nullptr) {
        const Callable cb(this, "_on_sample_rate_changed");
        if (!sample_rate_spin->is_connected("value_changed", cb)) {
            sample_rate_spin->connect("value_changed", cb);
        }
    }

    if (buffer_seconds_spin != nullptr) {
        const Callable cb(this, "_on_buffer_seconds_changed");
        if (!buffer_seconds_spin->is_connected("value_changed", cb)) {
            buffer_seconds_spin->connect("value_changed", cb);
        }
    }

    if (record_button != nullptr) {
        const Callable cb(this, "_on_record_toggled");
        if (!record_button->is_connected("toggled", cb)) {
            record_button->connect("toggled", cb);
        }
    }

    if (play_button != nullptr) {
        const Callable cb(this, "_on_play_toggled");
        if (!play_button->is_connected("toggled", cb)) {
            play_button->connect("toggled", cb);
        }
    }

    if (passthrough_button != nullptr) {
        const Callable cb(this, "_on_passthrough_toggled");
        if (!passthrough_button->is_connected("toggled", cb)) {
            passthrough_button->connect("toggled", cb);
        }
    }

    if (clear_button != nullptr) {
        const Callable cb(this, "_on_clear_pressed");
        if (!clear_button->is_connected("pressed", cb)) {
            clear_button->connect("pressed", cb);
        }
    }

    if (save_button != nullptr) {
        const Callable cb(this, "_on_save_pressed");
        if (!save_button->is_connected("pressed", cb)) {
            save_button->connect("pressed", cb);
        }
    }

    if (playback_slider != nullptr) {
        const Callable cb(this, "_on_playback_slider_changed");
        if (!playback_slider->is_connected("value_changed", cb)) {
            playback_slider->connect("value_changed", cb);
        }
    }
}

void AudioReplay::configure_slots() {
    clear_all_slots();
    set_slot(0, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO), true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO));
    set_slot(3, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(4, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(5, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
}
