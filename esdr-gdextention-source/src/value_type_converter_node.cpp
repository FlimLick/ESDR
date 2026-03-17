#include "value_type_converter_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

void ValueTypeConverter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_mode_selected", "index"), &ValueTypeConverter::_on_mode_selected);
    ClassDB::bind_method(D_METHOD("_on_number_changed", "value"), &ValueTypeConverter::_on_number_changed);
    ClassDB::bind_method(D_METHOD("_on_bool_toggled", "enabled"), &ValueTypeConverter::_on_bool_toggled);

    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &ValueTypeConverter::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &ValueTypeConverter::set_port_value);
}

void ValueTypeConverter::_notification(int32_t p_what) {
    if (p_what != NOTIFICATION_READY) {
        return;
    }

    on_ready();
}

void ValueTypeConverter::on_ready() {
    ensure_ui();
    cache_ui_refs();
    bind_ui();
    rebuild_input_control();
    refresh_output_label();
}

int32_t ValueTypeConverter::wrap_index(int32_t p_index, int32_t p_count) const {
    if (p_count <= 0) {
        return 0;
    }

    int32_t wrapped = p_index % p_count;
    if (wrapped < 0) {
        wrapped += p_count;
    }
    return wrapped;
}

int32_t ValueTypeConverter::source_signal_kind() const {
    switch (mode) {
        case MODE_BOOL_TO_INT:
        case MODE_BOOL_TO_FLOAT:
            return SIGNAL_BOOL;
        case MODE_INT_TO_FLOAT:
        case MODE_INT_TO_BOOL:
            return SIGNAL_INT;
        default:
            return SIGNAL_FLOAT;
    }
}

int32_t ValueTypeConverter::target_signal_kind() const {
    switch (mode) {
        case MODE_FLOAT_TO_BOOL:
        case MODE_INT_TO_BOOL:
            return SIGNAL_BOOL;
        case MODE_FLOAT_TO_INT:
        case MODE_BOOL_TO_INT:
            return SIGNAL_INT;
        default:
            return SIGNAL_FLOAT;
    }
}

bool ValueTypeConverter::source_is_bool() const {
    return source_signal_kind() == SIGNAL_BOOL;
}

bool ValueTypeConverter::source_is_int() const {
    return source_signal_kind() == SIGNAL_INT;
}

void ValueTypeConverter::ensure_ui() {
    if (has_node(NodePath("OutputLabel")) && has_node(NodePath("ModeRow/ModeSelector")) && has_node(NodePath("InputRow"))) {
        configure_slots();
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("Value Converter");
    set_custom_minimum_size(Vector2(400.0, 300.0));
    set_resizable(true);

    Label *output = memnew(Label);
    output->set_name("OutputLabel");
    output->set_text("Output: 0");
    output->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    add_child(output);

    HBoxContainer *mode_row = memnew(HBoxContainer);
    mode_row->set_name("ModeRow");
    mode_row->add_theme_constant_override("separation", 8);
    add_child(mode_row);

    Label *mode_label = memnew(Label);
    mode_label->set_name("ModeLabel");
    mode_label->set_text("Convert");
    mode_row->add_child(mode_label);

    OptionButton *mode_select = memnew(OptionButton);
    mode_select->set_name("ModeSelector");
    mode_select->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    mode_select->set_fit_to_longest_item(false);
    mode_select->set_clip_text(true);
    mode_select->add_item("Float -> Int", MODE_FLOAT_TO_INT);
    mode_select->add_item("Float -> Bool", MODE_FLOAT_TO_BOOL);
    mode_select->add_item("Int -> Float", MODE_INT_TO_FLOAT);
    mode_select->add_item("Int -> Bool", MODE_INT_TO_BOOL);
    mode_select->add_item("Bool -> Int", MODE_BOOL_TO_INT);
    mode_select->add_item("Bool -> Float", MODE_BOOL_TO_FLOAT);
    mode_select->select(mode);
    mode_row->add_child(mode_select);

    HBoxContainer *input_container = memnew(HBoxContainer);
    input_container->set_name("InputRow");
    input_container->add_theme_constant_override("separation", 8);
    add_child(input_container);

    configure_slots();
}

void ValueTypeConverter::cache_ui_refs() {
    output_label = Object::cast_to<Label>(get_node_or_null(NodePath("OutputLabel")));
    mode_selector = Object::cast_to<OptionButton>(get_node_or_null(NodePath("ModeRow/ModeSelector")));
    input_row = Object::cast_to<HBoxContainer>(get_node_or_null(NodePath("InputRow")));
}

void ValueTypeConverter::bind_ui() {
    if (mode_selector != nullptr) {
        const Callable cb(this, "_on_mode_selected");
        if (!mode_selector->is_connected("item_selected", cb)) {
            mode_selector->connect("item_selected", cb);
        }
    }
}

void ValueTypeConverter::configure_slots() {
    clear_all_slots();
    set_slot(0, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, target_signal_kind(), signal_color(target_signal_kind()));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    set_slot(2, true, source_signal_kind(), signal_color(source_signal_kind()), true, source_signal_kind(), signal_color(source_signal_kind()));
}

void ValueTypeConverter::rebuild_input_control() {
    if (input_row == nullptr) {
        return;
    }

    while (input_row->get_child_count() > 0) {
        Node *child = input_row->get_child(0);
        input_row->remove_child(child);
        memdelete(child);
    }

    input_label = memnew(Label);
    input_label->set_name("InputLabel");
    if (source_is_bool()) {
        input_label->set_text("Bool In");
    } else if (source_is_int()) {
        input_label->set_text("Int In");
    } else {
        input_label->set_text("Float In");
    }
    input_row->add_child(input_label);

    number_input = nullptr;
    bool_input = nullptr;

    if (source_is_bool()) {
        CheckBox *checkbox = memnew(CheckBox);
        checkbox->set_name("InputBool");
        checkbox->set_text("True");
        checkbox->set_pressed(boolean_value);
        checkbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        input_row->add_child(checkbox);
        bool_input = checkbox;

        const Callable cb(this, "_on_bool_toggled");
        checkbox->connect("toggled", cb);
    } else {
        SpinBox *spin = memnew(SpinBox);
        spin->set_name("InputNumber");
        spin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        spin->set_min(-1000000000000.0);
        spin->set_max(1000000000000.0);
        spin->set_step(source_is_int() ? 1.0 : 0.001);
        spin->set_value(numeric_value);
        input_row->add_child(spin);
        number_input = spin;

        const Callable cb(this, "_on_number_changed");
        spin->connect("value_changed", cb);
    }

    configure_slots();
    refresh_output_label();
}

void ValueTypeConverter::_on_mode_selected(int64_t p_index) {
    mode = std::clamp(static_cast<int32_t>(p_index), static_cast<int32_t>(MODE_FLOAT_TO_INT), static_cast<int32_t>(MODE_BOOL_TO_FLOAT));
    rebuild_input_control();
}

void ValueTypeConverter::_on_number_changed(double p_value) {
    if (source_is_int()) {
        numeric_value = static_cast<double>(std::llround(p_value));
        if (number_input != nullptr) {
            number_input->set_value_no_signal(numeric_value);
        }
    } else {
        numeric_value = p_value;
    }

    refresh_output_label();
}

void ValueTypeConverter::_on_bool_toggled(bool p_enabled) {
    boolean_value = p_enabled;
    refresh_output_label();
}

Variant ValueTypeConverter::get_port_value(int64_t p_port) const {
    if (p_port == 1) {
        return mode;
    }

    if (p_port == 2) {
        if (source_is_bool()) {
            return boolean_value;
        }
        if (source_is_int()) {
            return static_cast<int64_t>(std::llround(numeric_value));
        }
        return numeric_value;
    }

    if (p_port != 0) {
        return Variant();
    }

    switch (mode) {
        case MODE_FLOAT_TO_INT:
            return static_cast<int64_t>(std::llround(numeric_value));
        case MODE_FLOAT_TO_BOOL:
            return std::abs(numeric_value) > 1e-12;
        case MODE_INT_TO_FLOAT:
            return static_cast<double>(std::llround(numeric_value));
        case MODE_INT_TO_BOOL:
            return std::llround(numeric_value) != 0;
        case MODE_BOOL_TO_INT:
            return boolean_value ? int64_t(1) : int64_t(0);
        case MODE_BOOL_TO_FLOAT:
            return boolean_value ? 1.0 : 0.0;
        default:
            return Variant();
    }
}

void ValueTypeConverter::set_port_value(int64_t p_port, const Variant &p_value) {
    if (p_port == 1) {
        if (mode_selector == nullptr || mode_selector->get_item_count() <= 0) {
            return;
        }

        const int32_t wrapped = wrap_index(static_cast<int32_t>(p_value), mode_selector->get_item_count());
        mode_selector->select(wrapped);
        _on_mode_selected(wrapped);
        return;
    }

    if (p_port != 2) {
        return;
    }

    if (source_is_bool()) {
        boolean_value = static_cast<bool>(p_value);
        if (bool_input != nullptr) {
            bool_input->set_pressed_no_signal(boolean_value);
        }
        refresh_output_label();
        return;
    }

    double next_value = static_cast<double>(p_value);
    if (source_is_int()) {
        next_value = static_cast<double>(std::llround(next_value));
    }

    numeric_value = next_value;
    if (number_input != nullptr) {
        number_input->set_value_no_signal(numeric_value);
    }
    refresh_output_label();
}

void ValueTypeConverter::refresh_output_label() {
    if (output_label == nullptr) {
        return;
    }

    const Variant value = get_port_value(0);
    String value_text;
    switch (value.get_type()) {
        case Variant::BOOL:
            value_text = static_cast<bool>(value) ? "true" : "false";
            break;
        case Variant::INT:
            value_text = String::num_int64(static_cast<int64_t>(value));
            break;
        case Variant::FLOAT:
            value_text = String::num(static_cast<double>(value));
            break;
        default:
            value_text = UtilityFunctions::str(value);
            break;
    }

    output_label->set_text(String("Output: ") + value_text);
}
