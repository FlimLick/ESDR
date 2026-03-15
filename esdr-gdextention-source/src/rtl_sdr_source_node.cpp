#include "rtl_sdr_source_node.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

using namespace godot;

void RTLSDRSource::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_start_toggled", "enabled"), &RTLSDRSource::_on_start_toggled);
    ClassDB::bind_method(D_METHOD("_on_frequency_changed", "value"), &RTLSDRSource::_on_frequency_changed);
    ClassDB::bind_method(D_METHOD("_on_sample_rate_changed", "value"), &RTLSDRSource::_on_sample_rate_changed);
    ClassDB::bind_method(D_METHOD("_on_gain_changed", "value"), &RTLSDRSource::_on_gain_changed);
    ClassDB::bind_method(D_METHOD("_on_bias_t_toggled", "enabled"), &RTLSDRSource::_on_bias_t_toggled);
    ClassDB::bind_method(D_METHOD("_on_rtl_agc_toggled", "enabled"), &RTLSDRSource::_on_rtl_agc_toggled);
    ClassDB::bind_method(D_METHOD("_on_tuner_agc_toggled", "enabled"), &RTLSDRSource::_on_tuner_agc_toggled);
    ClassDB::bind_method(D_METHOD("_on_iq_correction_toggled", "enabled"), &RTLSDRSource::_on_iq_correction_toggled);

    ADD_SIGNAL(MethodInfo("baseband_frame", PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "frame")));
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
    ensure_ui();
    cache_ui_refs();
    set_status(false, "Status: OFF");

    if (Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        return;
    }

    backend.instantiate();
    if (backend.is_null()) {
        set_status(false, "Status: EXT Error");
        return;
    }

    bind_ui();
    push_ui_to_backend();
    set_process(true);
}

void RTLSDRSource::on_exit_tree() {
    if (backend.is_valid()) {
        backend->stop();
    }
}

void RTLSDRSource::process_tick() {
    if (backend.is_null()) {
        return;
    }

    PackedVector2Array frame = backend->pull_baseband_frame(16384);
    if (!frame.is_empty()) {
        emit_signal("baseband_frame", frame);
    }

    String error_text = backend->consume_error();
    if (!error_text.is_empty()) {
        set_status(false, "Status: ERROR");
        if (status_label != nullptr) {
            status_label->set_tooltip_text(error_text);
        }
        if (start_button != nullptr && start_button->is_pressed()) {
            start_button->set_pressed_no_signal(false);
        }
        emit_signal("source_status_changed", false, error_text);
    }

    if (start_button != nullptr && !backend->is_running() && start_button->is_pressed()) {
        start_button->set_pressed_no_signal(false);
        set_status(false, "Status: OFF");
        emit_signal("source_status_changed", false, "Stopped");
    }
}

void RTLSDRSource::cache_ui_refs() {
    frequency_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("Control/VBoxContainer/SpinBox2")));
    start_button = Object::cast_to<Button>(get_node_or_null(NodePath("HBoxContainer/Button")));
    status_label = Object::cast_to<Label>(get_node_or_null(NodePath("HBoxContainer/Label")));
    bias_t_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("CheckBox")));
    rtl_agc_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("CheckBox2")));
    tuner_agc_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("CheckBox3")));
    iq_correction_checkbox = Object::cast_to<CheckBox>(get_node_or_null(NodePath("CheckBox4")));
    sample_rate_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("PanelContainer2/VBoxContainer/SpinBox")));
    gain_slider = Object::cast_to<HSlider>(get_node_or_null(NodePath("PanelContainer/VBoxContainer/HSlider2")));
}

void RTLSDRSource::bind_ui() {
    if (start_button != nullptr) {
        const Callable cb(this, "_on_start_toggled");
        if (!start_button->is_connected("toggled", cb)) {
            start_button->connect("toggled", cb);
        }
    }

    if (frequency_spin != nullptr) {
        const Callable cb(this, "_on_frequency_changed");
        if (!frequency_spin->is_connected("value_changed", cb)) {
            frequency_spin->connect("value_changed", cb);
        }
    }

    if (sample_rate_spin != nullptr) {
        const Callable cb(this, "_on_sample_rate_changed");
        if (!sample_rate_spin->is_connected("value_changed", cb)) {
            sample_rate_spin->connect("value_changed", cb);
        }
    }

    if (gain_slider != nullptr) {
        const Callable cb(this, "_on_gain_changed");
        if (!gain_slider->is_connected("value_changed", cb)) {
            gain_slider->connect("value_changed", cb);
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

void RTLSDRSource::push_ui_to_backend() {
    if (backend.is_null()) {
        return;
    }

    backend->set_driver("rtlsdr");

    if (frequency_spin != nullptr) {
        backend->set_frequency_hz(frequency_spin->get_value());
    }

    if (sample_rate_spin != nullptr) {
        backend->set_sample_rate(sample_rate_spin->get_value());
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

void RTLSDRSource::set_status(bool p_is_running, const String &p_message) {
    if (status_label == nullptr) {
        return;
    }

    status_label->set_text(p_message);

    if (p_is_running) {
        status_label->add_theme_color_override("font_color", signal_color(SIGNAL_AUDIO));
    } else {
        status_label->add_theme_color_override("font_color", signal_color(SIGNAL_FLOAT));
    }
}

void RTLSDRSource::_on_start_toggled(bool p_enabled) {
    if (backend.is_null()) {
        if (start_button != nullptr) {
            start_button->set_pressed_no_signal(false);
        }
        set_status(false, "Status: EXT Missing");
        return;
    }

    if (p_enabled) {
        push_ui_to_backend();

        if (backend->start()) {
            set_status(true, "Status: ON");
            emit_signal("source_status_changed", true, "Running");
            return;
        }

        if (start_button != nullptr) {
            start_button->set_pressed_no_signal(false);
        }

        String error_text = backend->consume_error();
        if (error_text.is_empty()) {
            error_text = backend->get_last_status_message();
        }

        set_status(false, "Status: OFF");
        if (status_label != nullptr) {
            status_label->set_tooltip_text(error_text);
        }

        emit_signal("source_status_changed", false, error_text);
        return;
    }

    backend->stop();
    set_status(false, "Status: OFF");
    emit_signal("source_status_changed", false, "Stopped");
}

void RTLSDRSource::_on_frequency_changed(double p_value) {
    if (backend.is_valid()) {
        backend->set_frequency_hz(p_value);
    }
}

void RTLSDRSource::_on_sample_rate_changed(double p_value) {
    if (backend.is_valid()) {
        backend->set_sample_rate(p_value);
    }
}

void RTLSDRSource::_on_gain_changed(double p_value) {
    if (backend.is_valid()) {
        backend->set_gain_db(static_cast<float>(p_value));
    }
}

void RTLSDRSource::_on_bias_t_toggled(bool p_enabled) {
    if (backend.is_valid()) {
        backend->set_bias_t_enabled(p_enabled);
    }
}

void RTLSDRSource::_on_rtl_agc_toggled(bool p_enabled) {
    if (backend.is_valid()) {
        backend->set_rtl_agc_enabled(p_enabled);
    }
}

void RTLSDRSource::_on_tuner_agc_toggled(bool p_enabled) {
    if (backend.is_valid()) {
        backend->set_tuner_agc_enabled(p_enabled);
    }
}

void RTLSDRSource::_on_iq_correction_toggled(bool p_enabled) {
    if (backend.is_valid()) {
        backend->set_iq_correction_enabled(p_enabled);
    }
}

void RTLSDRSource::configure_slots() {
    set_slot(0, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(1, false, 0, Color(1.0, 1.0, 1.0, 1.0), false, 0, signal_color(SIGNAL_BASEBAND));
    set_slot(2, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_BASEBAND, signal_color(SIGNAL_BASEBAND));
    set_slot(3, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(4, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(5, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(6, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL));
    set_slot(7, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(8, true, SIGNAL_BOOL, signal_color(SIGNAL_BOOL), true, 0, Color(1.0, 1.0, 1.0, 1.0));
}

void RTLSDRSource::ensure_ui() {
    if (has_node(NodePath("Control/VBoxContainer/SpinBox2")) && has_node(NodePath("HBoxContainer/Button"))) {
        configure_slots();
        return;
    }

    set_title("RTL-SDR Source");
    set_custom_minimum_size(Vector2(300, 600));
    set_resizable(true);
    set("theme_override_constants/separation", 6);

    PanelContainer *top_panel = memnew(PanelContainer);
    top_panel->set_name("Control");
    add_child(top_panel);

    VBoxContainer *top_vbox = memnew(VBoxContainer);
    top_vbox->set_name("VBoxContainer");
    top_panel->add_child(top_vbox);

    SpinBox *freq = memnew(SpinBox);
    freq->set_name("SpinBox2");
    freq->set_max(1500000000.0);
    freq->set_value(433000000.0);
    freq->set_prefix("Freqency:");
    freq->set_suffix("Hz");
    top_vbox->add_child(freq);

    HBoxContainer *top_hbox = memnew(HBoxContainer);
    top_hbox->set_name("HBoxContainer");
    top_vbox->add_child(top_hbox);

    SpinBox *chan = memnew(SpinBox);
    chan->set_name("SpinBox");
    chan->set_max(9.0);
    chan->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    top_hbox->add_child(chan);

    HBoxContainer *start_hbox = memnew(HBoxContainer);
    start_hbox->set_name("HBoxContainer");
    add_child(start_hbox);

    Button *start = memnew(Button);
    start->set_name("Button");
    start->set_toggle_mode(true);
    start->set_text("START");
    start_hbox->add_child(start);

    Label *status = memnew(Label);
    status->set_name("Label");
    status->set_text("Status: OFF");
    start_hbox->add_child(status);

    Label *baseband = memnew(Label);
    baseband->set_name("Label");
    baseband->set_text("Baseband");
    baseband->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    add_child(baseband);

    CheckBox *bias = memnew(CheckBox);
    bias->set_name("CheckBox");
    bias->set_text("Bias T");
    add_child(bias);

    CheckBox *rtl_agc = memnew(CheckBox);
    rtl_agc->set_name("CheckBox2");
    rtl_agc->set_text("RTL AGC");
    add_child(rtl_agc);

    CheckBox *tuner_agc = memnew(CheckBox);
    tuner_agc->set_name("CheckBox3");
    tuner_agc->set_text("TUNER AGC");
    add_child(tuner_agc);

    CheckBox *iq_corr = memnew(CheckBox);
    iq_corr->set_name("CheckBox4");
    iq_corr->set_text("IQ Correction");
    iq_corr->set_pressed(true);
    add_child(iq_corr);

    PanelContainer *sample_panel = memnew(PanelContainer);
    sample_panel->set_name("PanelContainer2");
    add_child(sample_panel);

    VBoxContainer *sample_vbox = memnew(VBoxContainer);
    sample_vbox->set_name("VBoxContainer");
    sample_panel->add_child(sample_vbox);

    Label *sample_label = memnew(Label);
    sample_label->set_name("Label2");
    sample_label->set_text("Sample Rate");
    sample_vbox->add_child(sample_label);

    SpinBox *sample = memnew(SpinBox);
    sample->set_name("SpinBox");
    sample->set_min(256000.0);
    sample->set_max(3200000.0);
    sample->set_value(2400000.0);
    sample->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    sample->set_suffix("Samples/Second");
    sample_vbox->add_child(sample);

    PanelContainer *gain_panel = memnew(PanelContainer);
    gain_panel->set_name("PanelContainer");
    add_child(gain_panel);

    VBoxContainer *gain_vbox = memnew(VBoxContainer);
    gain_vbox->set_name("VBoxContainer");
    gain_panel->add_child(gain_vbox);

    Label *gain_label = memnew(Label);
    gain_label->set_name("Label");
    gain_label->set_text("Gain");
    gain_vbox->add_child(gain_label);

    HSlider *gain = memnew(HSlider);
    gain->set_name("HSlider2");
    gain->set_max(100.0);
    gain->set_value(50.0);
    gain_vbox->add_child(gain);

    Node *ordered_children[] = { top_panel, start_hbox, baseband, bias, rtl_agc, tuner_agc, iq_corr, sample_panel, gain_panel };
    const int32_t count = sizeof(ordered_children) / sizeof(ordered_children[0]);
    for (int32_t i = 0; i < count; i++) {
        if (ordered_children[i]->get_parent() == this) {
            move_child(ordered_children[i], i);
        }
    }

    configure_slots();
}
