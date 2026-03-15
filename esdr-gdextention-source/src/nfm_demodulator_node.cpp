#include "nfm_demodulator_node.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

using namespace godot;

void NFMDemodulator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_offset_changed", "value"), &NFMDemodulator::_on_offset_changed);
    ClassDB::bind_method(D_METHOD("_on_bandwidth_changed", "value"), &NFMDemodulator::_on_bandwidth_changed);

    ClassDB::bind_method(D_METHOD("set_offset_hz", "value"), &NFMDemodulator::set_offset_hz);
    ClassDB::bind_method(D_METHOD("get_offset_hz"), &NFMDemodulator::get_offset_hz);

    ClassDB::bind_method(D_METHOD("set_bandwidth_hz", "value"), &NFMDemodulator::set_bandwidth_hz);
    ClassDB::bind_method(D_METHOD("get_bandwidth_hz"), &NFMDemodulator::get_bandwidth_hz);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "offset_hz", PROPERTY_HINT_RANGE, "-1200000,1200000,1"), "set_offset_hz", "get_offset_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bandwidth_hz", PROPERTY_HINT_RANGE, "1,1200000,1"), "set_bandwidth_hz", "get_bandwidth_hz");
}

void NFMDemodulator::_notification(int32_t p_what) {
    if (p_what != NOTIFICATION_READY) {
        return;
    }

    ensure_ui();
    cache_ui_refs();
    bind_ui();
}

void NFMDemodulator::set_offset_hz(double p_value) {
    offset_hz = p_value;
    if (offset_spin != nullptr) {
        offset_spin->set_value_no_signal(offset_hz);
    }
}

double NFMDemodulator::get_offset_hz() const {
    return offset_hz;
}

void NFMDemodulator::set_bandwidth_hz(double p_value) {
    bandwidth_hz = p_value < 1.0 ? 1.0 : p_value;
    if (bandwidth_spin != nullptr) {
        bandwidth_spin->set_value_no_signal(bandwidth_hz);
    }
}

double NFMDemodulator::get_bandwidth_hz() const {
    return bandwidth_hz;
}

void NFMDemodulator::_on_offset_changed(double p_value) {
    offset_hz = p_value;
}

void NFMDemodulator::_on_bandwidth_changed(double p_value) {
    bandwidth_hz = p_value;
}

void NFMDemodulator::cache_ui_refs() {
    offset_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("SpinBoxOffset")));
    bandwidth_spin = Object::cast_to<SpinBox>(get_node_or_null(NodePath("SpinBoxBandwidth")));
}

void NFMDemodulator::bind_ui() {
    if (offset_spin != nullptr) {
        const Callable cb(this, "_on_offset_changed");
        if (!offset_spin->is_connected("value_changed", cb)) {
            offset_spin->connect("value_changed", cb);
        }
    }

    if (bandwidth_spin != nullptr) {
        const Callable cb(this, "_on_bandwidth_changed");
        if (!bandwidth_spin->is_connected("value_changed", cb)) {
            bandwidth_spin->connect("value_changed", cb);
        }
    }
}

void NFMDemodulator::configure_slots() {
    set_slot(0, true, SIGNAL_INT, signal_color(SIGNAL_INT), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(2, true, SIGNAL_BASEBAND, signal_color(SIGNAL_BASEBAND), false, 0, Color(1.0, 1.0, 1.0, 1.0));
    set_slot(3, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_AUDIO, signal_color(SIGNAL_AUDIO));
}

void NFMDemodulator::ensure_ui() {
    if (has_node(NodePath("SpinBoxOffset")) && has_node(NodePath("SpinBoxBandwidth"))) {
        configure_slots();
        return;
    }

    set_title("NFM Demodulator");
    set_custom_minimum_size(Vector2(300, 300));
    set_resizable(true);

    SpinBox *offset = memnew(SpinBox);
    offset->set_name("SpinBoxOffset");
    offset->set_max(1200000.0);
    offset->set_value(offset_hz);
    offset->set_prefix("Offset:");
    offset->set_suffix("Hz");
    add_child(offset);

    SpinBox *bandwidth = memnew(SpinBox);
    bandwidth->set_name("SpinBoxBandwidth");
    bandwidth->set_min(1.0);
    bandwidth->set_max(1200000.0);
    bandwidth->set_value(bandwidth_hz);
    bandwidth->set_prefix("Bandwidth:");
    bandwidth->set_suffix("Hz");
    add_child(bandwidth);

    Label *baseband = memnew(Label);
    baseband->set_name("LabelBaseband");
    baseband->set_text("Baseband");
    add_child(baseband);

    Label *audio = memnew(Label);
    audio->set_name("LabelAudio");
    audio->set_text("Audio");
    audio->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    add_child(audio);

    if (!Engine::get_singleton()->is_editor_hint()) {
        set_offset_hz(offset_hz);
        set_bandwidth_hz(bandwidth_hz);
    }

    configure_slots();
}
