#include "audio_stereo_matrix_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

#include <algorithm>

using namespace godot;

void AudioStereoMatrix::_bind_methods() {
    ClassDB::bind_method(D_METHOD("push_audio_frame", "frame"), &AudioStereoMatrix::push_audio_frame);
    ClassDB::bind_method(D_METHOD("push_audio_stereo_frame", "frame"), &AudioStereoMatrix::push_audio_stereo_frame);
    ClassDB::bind_method(D_METHOD("push_audio_frame_from_source", "frame", "source_id"), &AudioStereoMatrix::push_audio_frame_from_source);
    ClassDB::bind_method(D_METHOD("push_audio_stereo_frame_from_source", "frame", "source_id"), &AudioStereoMatrix::push_audio_stereo_frame_from_source);
    ClassDB::bind_method(D_METHOD("get_stereo_enabled"), &AudioStereoMatrix::get_stereo_enabled);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &AudioStereoMatrix::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &AudioStereoMatrix::set_port_value);

    ClassDB::bind_method(D_METHOD("_on_input_gain_changed", "value"), &AudioStereoMatrix::_on_input_gain_changed);
    ClassDB::bind_method(D_METHOD("_on_output_gain_changed", "value"), &AudioStereoMatrix::_on_output_gain_changed);
    ClassDB::bind_method(D_METHOD("_on_matrix_ll_changed", "value"), &AudioStereoMatrix::_on_matrix_ll_changed);
    ClassDB::bind_method(D_METHOD("_on_matrix_lr_changed", "value"), &AudioStereoMatrix::_on_matrix_lr_changed);
    ClassDB::bind_method(D_METHOD("_on_matrix_rl_changed", "value"), &AudioStereoMatrix::_on_matrix_rl_changed);
    ClassDB::bind_method(D_METHOD("_on_matrix_rr_changed", "value"), &AudioStereoMatrix::_on_matrix_rr_changed);

    ADD_SIGNAL(MethodInfo("audio_frame", PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "frame")));
    ADD_SIGNAL(MethodInfo("audio_stereo_frame", PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "frame")));
}

void AudioStereoMatrix::_notification(int32_t p_what) {
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

void AudioStereoMatrix::on_ready() {
    ensure_ui();
    cache_ui_refs();
    bind_ui();

    set_input_gain(input_gain);
    set_output_gain(output_gain);
    set_matrix_ll(matrix_ll);
    set_matrix_lr(matrix_lr);
    set_matrix_rl(matrix_rl);
    set_matrix_rr(matrix_rr);
    update_status();

    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }

    set_process(true);
}

void AudioStereoMatrix::on_exit_tree() {
    std::lock_guard<std::mutex> lock(input_mutex);
    pending_mono_frame = PackedFloat32Array();
    pending_stereo_frame = PackedVector2Array();
    has_pending_mono = false;
    has_pending_stereo = false;
}

void AudioStereoMatrix::process_tick() {
    PackedFloat32Array mono_frame;
    PackedVector2Array stereo_frame;
    bool use_stereo = false;

    {
        std::lock_guard<std::mutex> lock(input_mutex);
        if (!has_pending_mono && !has_pending_stereo) {
            return;
        }

        if (has_pending_stereo && !pending_stereo_frame.is_empty()) {
            stereo_frame = pending_stereo_frame;
            use_stereo = true;
        } else if (has_pending_mono && !pending_mono_frame.is_empty()) {
            mono_frame = pending_mono_frame;
        } else {
            return;
        }

        has_pending_mono = false;
        has_pending_stereo = false;
        pending_mono_frame = PackedFloat32Array();
        pending_stereo_frame = PackedVector2Array();
    }

    const int32_t frame_len = use_stereo ? stereo_frame.size() : mono_frame.size();
    if (frame_len <= 0) {
        return;
    }

    PackedVector2Array out_stereo;
    PackedFloat32Array out_mono;
    out_stereo.resize(frame_len);
    out_mono.resize(frame_len);

    Vector2 *out_stereo_ptr = out_stereo.ptrw();
    float *out_mono_ptr = out_mono.ptrw();
    const Vector2 *stereo_ptr = use_stereo ? stereo_frame.ptr() : nullptr;
    const float *mono_ptr = !use_stereo ? mono_frame.ptr() : nullptr;

    for (int32_t i = 0; i < frame_len; i++) {
        float in_l = 0.0f;
        float in_r = 0.0f;
        if (use_stereo) {
            in_l = stereo_ptr[i].x;
            in_r = stereo_ptr[i].y;
        } else {
            in_l = mono_ptr[i];
            in_r = mono_ptr[i];
        }

        in_l *= input_gain;
        in_r *= input_gain;

        float out_l = (in_l * matrix_ll) + (in_r * matrix_lr);
        float out_r = (in_l * matrix_rl) + (in_r * matrix_rr);

        out_l = std::clamp(out_l * output_gain, -1.0f, 1.0f);
        out_r = std::clamp(out_r * output_gain, -1.0f, 1.0f);

        out_stereo_ptr[i] = Vector2(out_l, out_r);
        out_mono_ptr[i] = std::clamp((out_l + out_r) * 0.5f, -1.0f, 1.0f);
    }

    emit_signal("audio_stereo_frame", out_stereo);
    emit_signal("audio_frame", out_mono);
}

void AudioStereoMatrix::push_audio_frame(const PackedFloat32Array &p_frame) {
    if (p_frame.is_empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(input_mutex);
    pending_mono_frame = p_frame;
    has_pending_mono = true;
}

void AudioStereoMatrix::push_audio_stereo_frame(const PackedVector2Array &p_frame) {
    if (p_frame.is_empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(input_mutex);
    pending_stereo_frame = p_frame;
    has_pending_stereo = true;
}

void AudioStereoMatrix::push_audio_frame_from_source(const PackedFloat32Array &p_frame, const StringName & /*p_source_id*/) {
    push_audio_frame(p_frame);
}

void AudioStereoMatrix::push_audio_stereo_frame_from_source(const PackedVector2Array &p_frame, const StringName & /*p_source_id*/) {
    push_audio_stereo_frame(p_frame);
}

bool AudioStereoMatrix::get_stereo_enabled() const {
    return true;
}

void AudioStereoMatrix::set_input_gain(float p_value) {
    input_gain = std::clamp(p_value, 0.0f, 4.0f);
    if (input_gain_spin != nullptr) {
        input_gain_spin->set_value_no_signal(static_cast<double>(input_gain));
    }
    update_status();
}

void AudioStereoMatrix::set_output_gain(float p_value) {
    output_gain = std::clamp(p_value, 0.0f, 4.0f);
    if (output_gain_spin != nullptr) {
        output_gain_spin->set_value_no_signal(static_cast<double>(output_gain));
    }
    update_status();
}

void AudioStereoMatrix::set_matrix_ll(float p_value) {
    matrix_ll = std::clamp(p_value, -2.0f, 2.0f);
    if (ll_spin != nullptr) {
        ll_spin->set_value_no_signal(static_cast<double>(matrix_ll));
    }
    update_status();
}

void AudioStereoMatrix::set_matrix_lr(float p_value) {
    matrix_lr = std::clamp(p_value, -2.0f, 2.0f);
    if (lr_spin != nullptr) {
        lr_spin->set_value_no_signal(static_cast<double>(matrix_lr));
    }
    update_status();
}

void AudioStereoMatrix::set_matrix_rl(float p_value) {
    matrix_rl = std::clamp(p_value, -2.0f, 2.0f);
    if (rl_spin != nullptr) {
        rl_spin->set_value_no_signal(static_cast<double>(matrix_rl));
    }
    update_status();
}

void AudioStereoMatrix::set_matrix_rr(float p_value) {
    matrix_rr = std::clamp(p_value, -2.0f, 2.0f);
    if (rr_spin != nullptr) {
        rr_spin->set_value_no_signal(static_cast<double>(matrix_rr));
    }
    update_status();
}

void AudioStereoMatrix::_on_input_gain_changed(double p_value) {
    set_input_gain(static_cast<float>(p_value));
}

void AudioStereoMatrix::_on_output_gain_changed(double p_value) {
    set_output_gain(static_cast<float>(p_value));
}

void AudioStereoMatrix::_on_matrix_ll_changed(double p_value) {
    set_matrix_ll(static_cast<float>(p_value));
}

void AudioStereoMatrix::_on_matrix_lr_changed(double p_value) {
    set_matrix_lr(static_cast<float>(p_value));
}

void AudioStereoMatrix::_on_matrix_rl_changed(double p_value) {
    set_matrix_rl(static_cast<float>(p_value));
}

void AudioStereoMatrix::_on_matrix_rr_changed(double p_value) {
    set_matrix_rr(static_cast<float>(p_value));
}

Variant AudioStereoMatrix::get_port_value(int64_t p_port) const {
    switch (p_port) {
        case 2:
            return input_gain;
        case 3:
            return output_gain;
        case 4:
            return matrix_ll;
        case 5:
            return matrix_lr;
        case 6:
            return matrix_rl;
        case 7:
            return matrix_rr;
        default:
            return Variant();
    }
}

void AudioStereoMatrix::set_port_value(int64_t p_port, const Variant &p_value) {
    switch (p_port) {
        case 0:
            if (p_value.get_type() == Variant::PACKED_VECTOR2_ARRAY) {
                push_audio_stereo_frame(p_value);
            } else if (p_value.get_type() == Variant::PACKED_FLOAT32_ARRAY) {
                push_audio_frame(p_value);
            }
            break;
        case 2:
            set_input_gain(static_cast<float>(static_cast<double>(p_value)));
            break;
        case 3:
            set_output_gain(static_cast<float>(static_cast<double>(p_value)));
            break;
        case 4:
            set_matrix_ll(static_cast<float>(static_cast<double>(p_value)));
            break;
        case 5:
            set_matrix_lr(static_cast<float>(static_cast<double>(p_value)));
            break;
        case 6:
            set_matrix_rl(static_cast<float>(static_cast<double>(p_value)));
            break;
        case 7:
            set_matrix_rr(static_cast<float>(static_cast<double>(p_value)));
            break;
        default:
            break;
    }
}

void AudioStereoMatrix::update_status() {
    if (status_label == nullptr) {
        return;
    }
    status_label->set_text(
            String("L = ") + String::num(matrix_ll, 2) + "L + " + String::num(matrix_lr, 2) + "R,  "
            + "R = " + String::num(matrix_rl, 2) + "L + " + String::num(matrix_rr, 2) + "R");
}

void AudioStereoMatrix::ensure_ui() {
    if (has_node(NodePath("InputGainRow/InputGainPanel/InputGainSpin")) &&
            has_node(NodePath("OutputGainRow/OutputGainPanel/OutputGainSpin")) &&
            has_node(NodePath("MatrixLLRow/MatrixLLPanel/MatrixLLSpin")) &&
            has_node(NodePath("MatrixLRRow/MatrixLRPanel/MatrixLRSpin")) &&
            has_node(NodePath("MatrixRLRow/MatrixRLPanel/MatrixRLSpin")) &&
            has_node(NodePath("MatrixRRRow/MatrixRRPanel/MatrixRRSpin"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("Audio Stereo Matrix");
    set_custom_minimum_size(Vector2(420, 280));
    set_resizable(true);

    Label *input_label = memnew(Label);
    input_label->set_name("InputLabel");
    input_label->set_text("Audio In");
    add_child(input_label);

    Label *output_label = memnew(Label);
    output_label->set_name("OutputLabel");
    output_label->set_text("Stereo Out");
    output_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    add_child(output_label);

    auto add_float_row = [&](const String &row_name, const String &label_text, const String &spin_name, double min_v, double max_v, double step, double value) {
        HBoxContainer *row = memnew(HBoxContainer);
        row->set_name(row_name);
        add_child(row);

        Label *label = memnew(Label);
        label->set_name(row_name + String("Label"));
        label->set_text(label_text);
        row->add_child(label);

        PanelContainer *panel = memnew(PanelContainer);
        panel->set_name(row_name + String("Panel"));
        panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        row->add_child(panel);

        SpinBox *spin = memnew(SpinBox);
        spin->set_name(spin_name);
        spin->set_min(min_v);
        spin->set_max(max_v);
        spin->set_step(step);
        spin->set_value(value);
        spin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        panel->add_child(spin);
    };

    add_float_row("InputGainRow", "Input Gain", "InputGainSpin", 0.0, 4.0, 0.01, input_gain);
    add_float_row("OutputGainRow", "Output Gain", "OutputGainSpin", 0.0, 4.0, 0.01, output_gain);
    add_float_row("MatrixLLRow", "L <- L", "MatrixLLSpin", -2.0, 2.0, 0.01, matrix_ll);
    add_float_row("MatrixLRRow", "L <- R", "MatrixLRSpin", -2.0, 2.0, 0.01, matrix_lr);
    add_float_row("MatrixRLRow", "R <- L", "MatrixRLSpin", -2.0, 2.0, 0.01, matrix_rl);
    add_float_row("MatrixRRRow", "R <- R", "MatrixRRSpin", -2.0, 2.0, 0.01, matrix_rr);

    Label *status = memnew(Label);
    status->set_name("StatusLabel");
    status->set_text("");
    add_child(status);

    configure_slots();
}

void AudioStereoMatrix::cache_ui_refs() {
    input_gain_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("InputGainRow/InputGainPanel/InputGainSpin")));
    output_gain_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("OutputGainRow/OutputGainPanel/OutputGainSpin")));
    ll_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("MatrixLLRow/MatrixLLPanel/MatrixLLSpin")));
    lr_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("MatrixLRRow/MatrixLRPanel/MatrixLRSpin")));
    rl_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("MatrixRLRow/MatrixRLPanel/MatrixRLSpin")));
    rr_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("MatrixRRRow/MatrixRRPanel/MatrixRRSpin")));
    status_label = Object::cast_to<Label>(get_node_or_null(NodePath("StatusLabel")));
}

void AudioStereoMatrix::bind_ui() {
    if (input_gain_spin != nullptr) {
        const Callable cb(this, "_on_input_gain_changed");
        if (!input_gain_spin->is_connected("value_changed", cb)) {
            input_gain_spin->connect("value_changed", cb);
        }
    }
    if (output_gain_spin != nullptr) {
        const Callable cb(this, "_on_output_gain_changed");
        if (!output_gain_spin->is_connected("value_changed", cb)) {
            output_gain_spin->connect("value_changed", cb);
        }
    }
    if (ll_spin != nullptr) {
        const Callable cb(this, "_on_matrix_ll_changed");
        if (!ll_spin->is_connected("value_changed", cb)) {
            ll_spin->connect("value_changed", cb);
        }
    }
    if (lr_spin != nullptr) {
        const Callable cb(this, "_on_matrix_lr_changed");
        if (!lr_spin->is_connected("value_changed", cb)) {
            lr_spin->connect("value_changed", cb);
        }
    }
    if (rl_spin != nullptr) {
        const Callable cb(this, "_on_matrix_rl_changed");
        if (!rl_spin->is_connected("value_changed", cb)) {
            rl_spin->connect("value_changed", cb);
        }
    }
    if (rr_spin != nullptr) {
        const Callable cb(this, "_on_matrix_rr_changed");
        if (!rr_spin->is_connected("value_changed", cb)) {
            rr_spin->connect("value_changed", cb);
        }
    }
}

void AudioStereoMatrix::configure_slots() {
    clear_all_slots();
    set_slot(0, true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(1, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO));
    set_slot(2, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(3, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(4, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(5, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(6, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(7, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
}
