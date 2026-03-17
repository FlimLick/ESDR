#ifndef ESDR_VALUE_TYPE_CONVERTER_NODE_H
#define ESDR_VALUE_TYPE_CONVERTER_NODE_H

#include "sdr_node.h"

#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/variant/variant.hpp>

class ValueTypeConverter : public SDR {
    GDCLASS(ValueTypeConverter, SDR)

private:
    enum ConvertMode {
        MODE_FLOAT_TO_INT = 0,
        MODE_FLOAT_TO_BOOL = 1,
        MODE_INT_TO_FLOAT = 2,
        MODE_INT_TO_BOOL = 3,
        MODE_BOOL_TO_INT = 4,
        MODE_BOOL_TO_FLOAT = 5,
    };

    godot::Label *output_label = nullptr;
    godot::OptionButton *mode_selector = nullptr;
    godot::HBoxContainer *input_row = nullptr;
    godot::Label *input_label = nullptr;
    godot::SpinBox *number_input = nullptr;
    godot::CheckBox *bool_input = nullptr;

    int32_t mode = MODE_FLOAT_TO_INT;
    double numeric_value = 0.0;
    bool boolean_value = false;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();
    void rebuild_input_control();
    void refresh_output_label();

    int32_t source_signal_kind() const;
    int32_t target_signal_kind() const;
    bool source_is_bool() const;
    bool source_is_int() const;
    int32_t wrap_index(int32_t p_index, int32_t p_count) const;

    void on_ready();
    void _on_mode_selected(int64_t p_index);
    void _on_number_changed(double p_value);
    void _on_bool_toggled(bool p_enabled);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    godot::Variant get_port_value(int64_t p_port) const;
    void set_port_value(int64_t p_port, const godot::Variant &p_value);
};

#endif // ESDR_VALUE_TYPE_CONVERTER_NODE_H
