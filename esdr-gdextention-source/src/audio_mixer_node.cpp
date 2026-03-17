#include "audio_mixer_node.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

using namespace godot;

void AudioMixer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("push_audio_input", "frame", "source_id"), &AudioMixer::push_audio_input);
    ClassDB::bind_method(D_METHOD("set_connected_source_count", "count"), &AudioMixer::set_connected_source_count);
    ClassDB::bind_method(D_METHOD("get_connected_source_count"), &AudioMixer::get_connected_source_count);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &AudioMixer::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &AudioMixer::set_port_value);

    ClassDB::bind_method(D_METHOD("_on_mix_mode_selected", "index"), &AudioMixer::_on_mix_mode_selected);
    ClassDB::bind_method(D_METHOD("_on_gain_changed", "value"), &AudioMixer::_on_gain_changed);

    ADD_SIGNAL(MethodInfo("audio_frame", PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "frame")));
}

void AudioMixer::_notification(int32_t p_what) {
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

void AudioMixer::on_ready() {
    ensure_ui();
    cache_ui_refs();
    bind_ui();
    refresh_gain_header();
    update_status();

    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }

    set_process(true);
}

void AudioMixer::on_exit_tree() {
    std::lock_guard<std::mutex> lock(frames_mutex);
    source_frames.clear();
}

void AudioMixer::process_tick() {
    std::unordered_map<std::string, PackedFloat32Array> frames_copy;
    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        if (source_frames.empty()) {
            update_status();
            return;
        }
        frames_copy.swap(source_frames);
    }

    int32_t max_len = 0;
    for (const auto &entry : frames_copy) {
        max_len = std::max(max_len, static_cast<int32_t>(entry.second.size()));
    }
    if (max_len <= 0) {
        update_status();
        return;
    }

    std::vector<float> accum(static_cast<size_t>(max_len), 0.0f);
    const float inv_count = 1.0f / static_cast<float>(std::max<size_t>(1, frames_copy.size()));
    const float scale = mix_mode == MIX_ADD ? output_gain : (output_gain * inv_count);

    for (const auto &entry : frames_copy) {
        const PackedFloat32Array &frame = entry.second;
        const int32_t frame_len = frame.size();
        if (frame_len <= 0) {
            continue;
        }

        const float *src = frame.ptr();
        int32_t i = 0;
#if defined(__AVX2__)
        for (; i + 8 <= frame_len; i += 8) {
            __m256 va = _mm256_loadu_ps(accum.data() + i);
            __m256 vb = _mm256_loadu_ps(src + i);
            va = _mm256_add_ps(va, vb);
            _mm256_storeu_ps(accum.data() + i, va);
        }
#endif
        for (; i < frame_len; i++) {
            accum[static_cast<size_t>(i)] += src[i];
        }
    }

    PackedFloat32Array mixed;
    mixed.resize(max_len);
    float *out = mixed.ptrw();
#if defined(__AVX2__)
    const __m256 vscale = _mm256_set1_ps(scale);
    const __m256 vmin = _mm256_set1_ps(-1.0f);
    const __m256 vmax = _mm256_set1_ps(1.0f);
    int32_t i = 0;
    for (; i + 8 <= max_len; i += 8) {
        __m256 v = _mm256_loadu_ps(accum.data() + i);
        v = _mm256_mul_ps(v, vscale);
        v = _mm256_max_ps(vmin, _mm256_min_ps(vmax, v));
        _mm256_storeu_ps(out + i, v);
    }
    for (; i < max_len; i++) {
        out[i] = std::clamp(accum[static_cast<size_t>(i)] * scale, -1.0f, 1.0f);
    }
#else
    for (int32_t i = 0; i < max_len; i++) {
        out[i] = std::clamp(accum[static_cast<size_t>(i)] * scale, -1.0f, 1.0f);
    }
#endif

    emit_signal("audio_frame", mixed);
    update_status();
}

void AudioMixer::push_audio_input(const PackedFloat32Array &p_frame, const StringName &p_source_id) {
    if (p_frame.is_empty()) {
        return;
    }

    const String source_string = String(p_source_id);
    const std::string source_key = source_string.utf8().get_data();

    std::lock_guard<std::mutex> lock(frames_mutex);
    source_frames[source_key] = p_frame;
}

void AudioMixer::set_connected_source_count(int32_t p_count) {
    connected_source_count = std::max(0, p_count);
    const int32_t target_inputs = std::max(1, connected_source_count + 1);
    if (target_inputs == input_slot_count) {
        update_status();
        return;
    }

    input_slot_count = target_inputs;
    rebuild_input_rows();
    configure_slots();
    update_status();
}

int32_t AudioMixer::wrap_index(int32_t p_index, int32_t p_count) const {
    if (p_count <= 0) {
        return 0;
    }

    int32_t wrapped = p_index % p_count;
    if (wrapped < 0) {
        wrapped += p_count;
    }
    return wrapped;
}

void AudioMixer::_on_mix_mode_selected(int64_t p_index) {
    mix_mode = std::clamp(static_cast<int32_t>(p_index), static_cast<int32_t>(MIX_AVERAGE), static_cast<int32_t>(MIX_ADD));
}

int32_t AudioMixer::get_connected_source_count() const {
    return connected_source_count;
}

void AudioMixer::_on_gain_changed(double p_value) {
    if (gain_slider != nullptr && std::abs(gain_slider->get_value() - p_value) > 1e-9) {
        gain_slider->set_value_no_signal(p_value);
    }
    output_gain = std::clamp(static_cast<float>(p_value), 0.0f, 2.0f);
    refresh_gain_header();
}

Variant AudioMixer::get_port_value(int64_t p_port) const {
    if (p_port == 1) {
        return mix_mode;
    }
    if (p_port == 2) {
        return output_gain;
    }
    return Variant();
}

void AudioMixer::set_port_value(int64_t p_port, const Variant &p_value) {
    if (p_port == 1) {
        if (mode_selector == nullptr || mode_selector->get_item_count() <= 0) {
            return;
        }
        const int32_t wrapped = wrap_index(static_cast<int32_t>(p_value), mode_selector->get_item_count());
        mode_selector->select(wrapped);
        _on_mix_mode_selected(wrapped);
        return;
    }

    if (p_port == 2) {
        const double value = static_cast<double>(p_value);
        if (gain_slider != nullptr) {
            gain_slider->set_value_no_signal(value);
        }
        _on_gain_changed(value);
        return;
    }
}

void AudioMixer::cache_ui_refs() {
    output_label = Object::cast_to<Label>(get_node_or_null(NodePath("OutputLabel")));
    status_label = Object::cast_to<Label>(get_node_or_null(NodePath("StatusLabel")));
    mode_selector = Object::cast_to<OptionButton>(get_node_or_null(NodePath("MixModeRow/MixModeSelector")));
    gain_slider = Object::cast_to<HSlider>(get_node_or_null(NodePath("GainSlider")));
    gain_min_label = Object::cast_to<Label>(get_node_or_null(NodePath("GainHeaderRow/GainMinLabel")));
    gain_value_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("GainHeaderRow/GainValueSpin")));
    gain_max_label = Object::cast_to<Label>(get_node_or_null(NodePath("GainHeaderRow/GainMaxLabel")));
}

void AudioMixer::bind_ui() {
    if (mode_selector != nullptr) {
        const Callable cb(this, "_on_mix_mode_selected");
        if (!mode_selector->is_connected("item_selected", cb)) {
            mode_selector->connect("item_selected", cb);
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
}

void AudioMixer::update_status() {
    if (status_label == nullptr) {
        return;
    }

    status_label->set_text(String("Sources: ") + String::num_int64(connected_source_count));
}

void AudioMixer::refresh_gain_header() {
    if (gain_slider == nullptr) {
        return;
    }

    if (gain_min_label != nullptr) {
        gain_min_label->set_text(String("min ") + String::num(gain_slider->get_min()));
        gain_min_label->add_theme_font_size_override("font_size", 10);
        gain_min_label->add_theme_color_override("font_color", Color(0.67, 0.67, 0.67, 1.0));
    }
    if (gain_value_spin != nullptr) {
        gain_value_spin->set_min(gain_slider->get_min());
        gain_value_spin->set_max(gain_slider->get_max());
        gain_value_spin->set_step(gain_slider->get_step());
        gain_value_spin->set_value_no_signal(gain_slider->get_value());
    }
    if (gain_max_label != nullptr) {
        gain_max_label->set_text(String("max ") + String::num(gain_slider->get_max()));
        gain_max_label->add_theme_font_size_override("font_size", 10);
        gain_max_label->add_theme_color_override("font_color", Color(0.67, 0.67, 0.67, 1.0));
    }
}

void AudioMixer::configure_slots() {
    clear_all_slots();
    set_slot(0, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(3, false, 0, Color(1.0, 1.0, 1.0, 1.0), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(4, false, 0, Color(1.0, 1.0, 1.0, 1.0), false, 0, Color(1.0, 1.0, 1.0, 1.0));

    for (int32_t i = 0; i < input_slot_count; i++) {
        set_slot(5 + i, true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    }
}

void AudioMixer::rebuild_input_rows() {
    for (int32_t i = 0; i < input_labels.size(); i++) {
        Label *label = input_labels[i];
        if (label != nullptr && label->get_parent() == this) {
            remove_child(label);
            memdelete(label);
        }
    }
    input_labels.clear();

    for (int32_t i = 0; i < input_slot_count; i++) {
        Label *input = memnew(Label);
        input->set_name(String("InputLabel") + String::num_int64(i));
        input->set_text(String("Input ") + String::num_int64(i + 1));
        add_child(input);
        move_child(input, 5 + i);
        input_labels.push_back(input);
    }

    set_custom_minimum_size(Vector2(400.0, 300.0 + static_cast<double>(input_slot_count * 24)));
}

void AudioMixer::ensure_ui() {
    if (has_node(NodePath("OutputLabel")) && has_node(NodePath("MixModeRow/MixModeSelector")) && has_node(NodePath("GainSlider"))) {
        rebuild_input_rows();
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("Audio Mixer");
    set_custom_minimum_size(Vector2(400, 300));
    set_resizable(true);

    Label *output = memnew(Label);
    output->set_name("OutputLabel");
    output->set_text("Mixed Audio");
    output->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    add_child(output);

    HBoxContainer *mix_mode_row = memnew(HBoxContainer);
    mix_mode_row->set_name("MixModeRow");
    add_child(mix_mode_row);

    Label *mix_mode_label = memnew(Label);
    mix_mode_label->set_name("MixModeLabel");
    mix_mode_label->set_text("Mix Mode");
    mix_mode_row->add_child(mix_mode_label);

    OptionButton *mix_mode_select = memnew(OptionButton);
    mix_mode_select->set_name("MixModeSelector");
    mix_mode_select->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    mix_mode_select->set_fit_to_longest_item(false);
    mix_mode_select->set_clip_text(true);
    mix_mode_select->add_item("Average", MIX_AVERAGE);
    mix_mode_select->add_item("Add", MIX_ADD);
    mix_mode_select->select(mix_mode);
    mix_mode_row->add_child(mix_mode_select);

    HBoxContainer *gain_header = memnew(HBoxContainer);
    gain_header->set_name("GainHeaderRow");
    gain_header->add_theme_constant_override("separation", 6);
    add_child(gain_header);

    Label *gain_label = memnew(Label);
    gain_label->set_name("GainLabel");
    gain_label->set_text("Output Gain");
    gain_header->add_child(gain_label);

    Control *gain_spacer = memnew(Control);
    gain_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    gain_header->add_child(gain_spacer);

    Label *gain_min = memnew(Label);
    gain_min->set_name("GainMinLabel");
    gain_min->add_theme_font_size_override("font_size", 10);
    gain_min->add_theme_color_override("font_color", Color(0.67, 0.67, 0.67, 1.0));
    gain_header->add_child(gain_min);

    SpinBox *gain_value = memnew(SpinBox);
    gain_value->set_name("GainValueSpin");
    gain_value->set_min(0.0);
    gain_value->set_max(2.0);
    gain_value->set_step(0.01);
    gain_value->set_custom_minimum_size(Vector2(86.0, 0.0));
    gain_header->add_child(gain_value);

    Label *gain_max = memnew(Label);
    gain_max->set_name("GainMaxLabel");
    gain_max->add_theme_font_size_override("font_size", 10);
    gain_max->add_theme_color_override("font_color", Color(0.67, 0.67, 0.67, 1.0));
    gain_header->add_child(gain_max);

    HSlider *gain = memnew(HSlider);
    gain->set_name("GainSlider");
    gain->set_min(0.0);
    gain->set_max(2.0);
    gain->set_step(0.01);
    gain->set_value(output_gain);
    add_child(gain);

    Label *status = memnew(Label);
    status->set_name("StatusLabel");
    status->set_text("Sources: 0");
    add_child(status);

    rebuild_input_rows();
    configure_slots();
}
