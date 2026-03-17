#ifndef ESDR_MATH_OPERATOR_NODE_H
#define ESDR_MATH_OPERATOR_NODE_H

#include "sdr_node.h"

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/templates/vector.hpp>

class MathOperator : public SDR {
    GDCLASS(MathOperator, SDR)

private:
    enum Operation {
        OP_ADD = 0,
        OP_SUBTRACT = 1,
        OP_MULTIPLY = 2,
        OP_DIVIDE = 3,
        OP_MOD = 4,
        OP_MIN = 5,
        OP_MAX = 6,
        OP_CLAMP = 7,
        OP_POW = 8,
        OP_ABS = 9,
        OP_NEGATE = 10,
    };

    godot::OptionButton *operation_selector = nullptr;
    godot::Label *result_label = nullptr;
    godot::Vector<godot::SpinBox *> input_spins;
    godot::Vector<godot::Control *> input_rows;
    godot::Vector<double> input_values;

    int32_t operation = OP_ADD;
    int32_t connected_input_count = 0;
    int32_t input_slot_count = 2;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();
    void rebuild_input_rows();
    void refresh_result();

    bool operation_is_expandable() const;
    int32_t get_required_input_count() const;
    godot::String get_input_label_for_index(int32_t p_index) const;
    double evaluate_result() const;
    int32_t wrap_index(int32_t p_index, int32_t p_count) const;

    void on_ready();
    void _on_operation_selected(int64_t p_index);
    void _on_input_changed(double p_value, int32_t p_index);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    void set_connected_input_count(int32_t p_count);
    int32_t get_connected_input_count() const;
    bool is_input_port(int64_t p_port) const;
    void push_number_input(double p_value, int32_t p_input_index);
    godot::Variant get_port_value(int64_t p_port) const;
    void set_port_value(int64_t p_port, const godot::Variant &p_value);
};

#endif // ESDR_MATH_OPERATOR_NODE_H
