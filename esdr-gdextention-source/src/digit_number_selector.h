#ifndef ESDR_DIGIT_NUMBER_SELECTOR_H
#define ESDR_DIGIT_NUMBER_SELECTOR_H

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

class DigitNumberSelector : public godot::HBoxContainer {
    GDCLASS(DigitNumberSelector, godot::HBoxContainer)

private:
    int64_t value = 0;
    int64_t min_value = 0;
    int64_t max_value = 999000000000LL;
    int32_t digit_count = 12;
    int32_t group_size = 3;
    bool show_group_labels = true;
    bool show_separators = true;
    godot::String suffix_text = "Hz";
    godot::PackedStringArray group_labels;

    godot::HBoxContainer *root_row = nullptr;
    godot::Label *sign_label = nullptr;
    godot::HBoxContainer *sections_row = nullptr;
    godot::Label *suffix_label = nullptr;
    godot::Vector<godot::Button *> digit_buttons;
    int32_t focused_digit_index = -1;

    void rebuild_ui();
    void refresh_digits();
    int64_t get_step_for_digit(int32_t p_digit_index) const;
    void adjust_digit(int32_t p_digit_index, int32_t p_delta);
    void set_digit_value(int32_t p_digit_index, int32_t p_digit);
    void focus_digit(int32_t p_digit_index, bool p_select);

    void _on_digit_gui_input(const godot::Ref<godot::InputEvent> &p_event, int32_t p_digit_index);
    void _on_digit_mouse_entered(int32_t p_digit_index);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    void set_value(int64_t p_value);
    void set_value_no_signal(int64_t p_value);
    int64_t get_value() const;

    void set_limits(int64_t p_min_value, int64_t p_max_value);
    void set_min_value(int64_t p_value);
    int64_t get_min_value() const;
    void set_max_value(int64_t p_value);
    int64_t get_max_value() const;

    void set_digit_count(int32_t p_count);
    int32_t get_digit_count() const;
    void set_group_size(int32_t p_size);
    int32_t get_group_size() const;

    void set_group_labels(const godot::PackedStringArray &p_labels);
    godot::PackedStringArray get_group_labels() const;

    void set_show_group_labels(bool p_enabled);
    bool get_show_group_labels() const;
    void set_show_separators(bool p_enabled);
    bool get_show_separators() const;

    void set_suffix_text(const godot::String &p_text);
    godot::String get_suffix_text() const;
};

#endif // ESDR_DIGIT_NUMBER_SELECTOR_H
