#include "math_operator_node.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

namespace {
constexpr double EPSILON = 1e-12;
}

void MathOperator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_connected_input_count", "count"), &MathOperator::set_connected_input_count);
    ClassDB::bind_method(D_METHOD("get_connected_input_count"), &MathOperator::get_connected_input_count);
    ClassDB::bind_method(D_METHOD("is_input_port", "port"), &MathOperator::is_input_port);
    ClassDB::bind_method(D_METHOD("push_number_input", "value", "input_index"), &MathOperator::push_number_input);
    ClassDB::bind_method(D_METHOD("get_port_value", "port"), &MathOperator::get_port_value);
    ClassDB::bind_method(D_METHOD("set_port_value", "port", "value"), &MathOperator::set_port_value);

    ClassDB::bind_method(D_METHOD("_on_operation_selected", "index"), &MathOperator::_on_operation_selected);
    ClassDB::bind_method(D_METHOD("_on_input_changed", "value", "index"), &MathOperator::_on_input_changed);

    ADD_SIGNAL(MethodInfo("float_value", PropertyInfo(Variant::FLOAT, "value")));
}

void MathOperator::_notification(int32_t p_what) {
    if (p_what != NOTIFICATION_READY) {
        return;
    }

    on_ready();
}

void MathOperator::on_ready() {
    ensure_ui();
    cache_ui_refs();
    bind_ui();
    rebuild_input_rows();
}

void MathOperator::set_connected_input_count(int32_t p_count) {
    connected_input_count = std::max(0, p_count);
    if (operation_is_expandable()) {
        rebuild_input_rows();
    } else {
        refresh_result();
    }
}

int32_t MathOperator::get_connected_input_count() const {
    return connected_input_count;
}

void MathOperator::push_number_input(double p_value, int32_t p_input_index) {
    if (p_input_index < 0 || p_input_index >= input_values.size()) {
        return;
    }

    input_values.write[p_input_index] = p_value;
    if (p_input_index < input_spins.size() && input_spins[p_input_index] != nullptr) {
        input_spins[p_input_index]->set_value_no_signal(p_value);
    }

    refresh_result();
}

int32_t MathOperator::wrap_index(int32_t p_index, int32_t p_count) const {
    if (p_count <= 0) {
        return 0;
    }

    int32_t wrapped = p_index % p_count;
    if (wrapped < 0) {
        wrapped += p_count;
    }
    return wrapped;
}

void MathOperator::_on_operation_selected(int64_t p_index) {
    const int32_t selected = static_cast<int32_t>(p_index);
    operation = std::clamp(selected, static_cast<int32_t>(OP_ADD), static_cast<int32_t>(OP_NEGATE));
    rebuild_input_rows();
}

void MathOperator::_on_input_changed(double p_value, int32_t p_index) {
    if (p_index < 0 || p_index >= input_values.size()) {
        return;
    }

    input_values.write[p_index] = p_value;
    refresh_result();
}

Variant MathOperator::get_port_value(int64_t p_port) const {
    if (p_port == 0) {
        return evaluate_result();
    }
    if (p_port == 1) {
        return operation;
    }

    const int64_t input_start_port = 2;
    const int64_t input_index = p_port - input_start_port;
    if (input_index >= 0 && input_index < input_values.size()) {
        return input_values[static_cast<int32_t>(input_index)];
    }

    return Variant();
}

void MathOperator::set_port_value(int64_t p_port, const Variant &p_value) {
    if (p_port == 1) {
        if (operation_selector == nullptr || operation_selector->get_item_count() <= 0) {
            return;
        }
        const int32_t wrapped = wrap_index(static_cast<int32_t>(p_value), operation_selector->get_item_count());
        operation_selector->select(wrapped);
        _on_operation_selected(wrapped);
        return;
    }

    const int64_t input_start_port = 2;
    const int64_t input_index = p_port - input_start_port;
    if (input_index < 0 || input_index >= input_values.size()) {
        return;
    }

    const int32_t index = static_cast<int32_t>(input_index);
    const double value = static_cast<double>(p_value);
    input_values.write[index] = value;
    if (index < input_spins.size() && input_spins[index] != nullptr) {
        input_spins[index]->set_value_no_signal(value);
    }
    refresh_result();
}

void MathOperator::ensure_ui() {
    if (has_node(NodePath("OperationSelector")) && has_node(NodePath("ResultLabel"))) {
        return;
    }

    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    set_title("Math");
    set_custom_minimum_size(Vector2(400.0, 300.0));
    set_resizable(true);

    Label *output = memnew(Label);
    output->set_name("ResultLabel");
    output->set_text("Result: 0");
    output->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
    add_child(output);

    OptionButton *selector = memnew(OptionButton);
    selector->set_name("OperationSelector");
    selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    selector->set_fit_to_longest_item(false);
    selector->set_clip_text(true);
    selector->add_item("Add", OP_ADD);
    selector->add_item("Subtract", OP_SUBTRACT);
    selector->add_item("Multiply", OP_MULTIPLY);
    selector->add_item("Divide", OP_DIVIDE);
    selector->add_item("Mod", OP_MOD);
    selector->add_item("Min", OP_MIN);
    selector->add_item("Max", OP_MAX);
    selector->add_item("Clamp", OP_CLAMP);
    selector->add_item("Pow", OP_POW);
    selector->add_item("Abs", OP_ABS);
    selector->add_item("Negate", OP_NEGATE);
    selector->select(operation);
    add_child(selector);

}

void MathOperator::cache_ui_refs() {
    operation_selector = Object::cast_to<OptionButton>(get_node_or_null(NodePath("OperationSelector")));
    result_label = Object::cast_to<Label>(get_node_or_null(NodePath("ResultLabel")));
}

void MathOperator::bind_ui() {
    if (operation_selector != nullptr) {
        const Callable cb(this, "_on_operation_selected");
        if (!operation_selector->is_connected("item_selected", cb)) {
            operation_selector->connect("item_selected", cb);
        }
    }
}

void MathOperator::configure_slots() {
    clear_all_slots();
    set_slot(0, false, 0, Color(1.0, 1.0, 1.0, 1.0), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    set_slot(1, true, SIGNAL_INT, signal_color(SIGNAL_INT), true, SIGNAL_INT, signal_color(SIGNAL_INT));
    for (int32_t i = 0; i < input_slot_count; i++) {
        set_slot(i + 2, true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT), true, SIGNAL_FLOAT, signal_color(SIGNAL_FLOAT));
    }
}

bool MathOperator::operation_is_expandable() const {
    return operation == OP_ADD || operation == OP_MULTIPLY;
}

int32_t MathOperator::get_required_input_count() const {
    switch (operation) {
        case OP_ADD:
        case OP_MULTIPLY:
            return std::max(2, connected_input_count + 1);
        case OP_CLAMP:
            return 3;
        case OP_ABS:
        case OP_NEGATE:
            return 1;
        default:
            return 2;
    }
}

String MathOperator::get_input_label_for_index(int32_t p_index) const {
    switch (operation) {
        case OP_CLAMP:
            if (p_index == 0) {
                return "Value";
            }
            if (p_index == 1) {
                return "Min";
            }
            return "Max";
        case OP_POW:
            if (p_index == 0) {
                return "Base";
            }
            return "Exponent";
        case OP_ABS:
        case OP_NEGATE:
            return "Value";
        default:
            return String("Input ") + String::num_int64(p_index + 1);
    }
}

double MathOperator::evaluate_result() const {
    if (input_values.is_empty()) {
        return 0.0;
    }

    switch (operation) {
        case OP_ADD: {
            double result = 0.0;
            for (int32_t i = 0; i < input_values.size(); i++) {
                result += input_values[i];
            }
            return result;
        }
        case OP_SUBTRACT:
            return input_values.size() >= 2 ? (input_values[0] - input_values[1]) : input_values[0];
        case OP_MULTIPLY: {
            double result = 1.0;
            for (int32_t i = 0; i < input_values.size(); i++) {
                result *= input_values[i];
            }
            return result;
        }
        case OP_DIVIDE:
            if (input_values.size() < 2) {
                return input_values[0];
            }
            if (std::abs(input_values[1]) <= EPSILON) {
                return 0.0;
            }
            return input_values[0] / input_values[1];
        case OP_MOD:
            if (input_values.size() < 2 || std::abs(input_values[1]) <= EPSILON) {
                return 0.0;
            }
            return std::fmod(input_values[0], input_values[1]);
        case OP_MIN: {
            if (input_values.size() < 2) {
                return input_values[0];
            }
            return std::min(input_values[0], input_values[1]);
        }
        case OP_MAX: {
            if (input_values.size() < 2) {
                return input_values[0];
            }
            return std::max(input_values[0], input_values[1]);
        }
        case OP_CLAMP:
            if (input_values.size() < 3) {
                return input_values[0];
            }
            return std::clamp(input_values[0], input_values[1], input_values[2]);
        case OP_POW:
            if (input_values.size() < 2) {
                return input_values[0];
            }
            return std::pow(input_values[0], input_values[1]);
        case OP_ABS:
            return std::abs(input_values[0]);
        case OP_NEGATE:
            return -input_values[0];
        default:
            return 0.0;
    }
}

void MathOperator::refresh_result() {
    bool has_problem = false;
    String problem_message;
    if (operation == OP_DIVIDE && input_values.size() >= 2 && std::abs(input_values[1]) <= EPSILON) {
        has_problem = true;
        problem_message = "Division by zero.";
    } else if (operation == OP_MOD && input_values.size() >= 2 && std::abs(input_values[1]) <= EPSILON) {
        has_problem = true;
        problem_message = "Modulo by zero.";
    }
    set_problem_state(has_problem, problem_message);

    const double result = evaluate_result();
    if (result_label != nullptr) {
        result_label->set_text(String("Result: ") + String::num(result));
    }
    emit_signal("float_value", result);
}

void MathOperator::rebuild_input_rows() {
    Vector<double> old_values = input_values;
    input_spins.clear();
    for (int32_t i = 0; i < input_rows.size(); i++) {
        Control *row = input_rows[i];
        if (row != nullptr && row->get_parent() == this) {
            remove_child(row);
            memdelete(row);
        }
    }
    input_rows.clear();
    input_values.clear();

    input_slot_count = get_required_input_count();
    input_values.resize(input_slot_count);

    for (int32_t i = 0; i < input_slot_count; i++) {
        double default_value = 0.0;
        if (i < old_values.size()) {
            default_value = old_values[i];
        }
        input_values.write[i] = default_value;

        HBoxContainer *row = memnew(HBoxContainer);
        row->set_name(String("InputRow") + String::num_int64(i));
        row->add_theme_constant_override("separation", 8);
        add_child(row);
        move_child(row, i + 2);

        Label *label = memnew(Label);
        label->set_name(String("InputLabel") + String::num_int64(i));
        label->set_text(get_input_label_for_index(i));
        row->add_child(label);

        SpinBox *spin = memnew(SpinBox);
        spin->set_name(String("InputSpin") + String::num_int64(i));
        spin->set_min(-1000000000000.0);
        spin->set_max(1000000000000.0);
        spin->set_step(0.001);
        spin->set_value(default_value);
        spin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        row->add_child(spin);

        const Callable cb = Callable(this, "_on_input_changed").bind(i);
        spin->connect("value_changed", cb);
        input_spins.push_back(spin);
        input_rows.push_back(row);
    }

    set_custom_minimum_size(Vector2(400.0, 300.0 + static_cast<double>(input_slot_count * 24)));
    if (operation_selector != nullptr && operation_selector->get_parent() == this) {
        move_child(operation_selector, 1);
    }
    configure_slots();
    refresh_result();
}

bool MathOperator::is_input_port(int64_t p_port) const {
    if (p_port < 2) {
        return false;
    }
    return p_port < (input_slot_count + 2);
}
