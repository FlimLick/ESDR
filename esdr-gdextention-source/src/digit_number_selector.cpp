#include "digit_number_selector.h"

#include <godot_cpp/classes/box_container.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

void DigitNumberSelector::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_value", "value"), &DigitNumberSelector::set_value);
    ClassDB::bind_method(D_METHOD("set_value_no_signal", "value"), &DigitNumberSelector::set_value_no_signal);
    ClassDB::bind_method(D_METHOD("get_value"), &DigitNumberSelector::get_value);

    ClassDB::bind_method(D_METHOD("set_limits", "min_value", "max_value"), &DigitNumberSelector::set_limits);
    ClassDB::bind_method(D_METHOD("set_min_value", "value"), &DigitNumberSelector::set_min_value);
    ClassDB::bind_method(D_METHOD("get_min_value"), &DigitNumberSelector::get_min_value);
    ClassDB::bind_method(D_METHOD("set_max_value", "value"), &DigitNumberSelector::set_max_value);
    ClassDB::bind_method(D_METHOD("get_max_value"), &DigitNumberSelector::get_max_value);

    ClassDB::bind_method(D_METHOD("set_digit_count", "count"), &DigitNumberSelector::set_digit_count);
    ClassDB::bind_method(D_METHOD("get_digit_count"), &DigitNumberSelector::get_digit_count);
    ClassDB::bind_method(D_METHOD("set_group_size", "size"), &DigitNumberSelector::set_group_size);
    ClassDB::bind_method(D_METHOD("get_group_size"), &DigitNumberSelector::get_group_size);

    ClassDB::bind_method(D_METHOD("set_group_labels", "labels"), &DigitNumberSelector::set_group_labels);
    ClassDB::bind_method(D_METHOD("get_group_labels"), &DigitNumberSelector::get_group_labels);

    ClassDB::bind_method(D_METHOD("set_show_group_labels", "enabled"), &DigitNumberSelector::set_show_group_labels);
    ClassDB::bind_method(D_METHOD("get_show_group_labels"), &DigitNumberSelector::get_show_group_labels);
    ClassDB::bind_method(D_METHOD("set_show_separators", "enabled"), &DigitNumberSelector::set_show_separators);
    ClassDB::bind_method(D_METHOD("get_show_separators"), &DigitNumberSelector::get_show_separators);

    ClassDB::bind_method(D_METHOD("set_suffix_text", "text"), &DigitNumberSelector::set_suffix_text);
    ClassDB::bind_method(D_METHOD("get_suffix_text"), &DigitNumberSelector::get_suffix_text);

    ClassDB::bind_method(D_METHOD("_on_digit_gui_input", "event", "digit_index"), &DigitNumberSelector::_on_digit_gui_input);
    ClassDB::bind_method(D_METHOD("_on_digit_mouse_entered", "digit_index"), &DigitNumberSelector::_on_digit_mouse_entered);

    ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::INT, "value")));
}

void DigitNumberSelector::_notification(int32_t p_what) {
    if (p_what == NOTIFICATION_READY) {
        if (root_row == nullptr) {
            rebuild_ui();
        }
        refresh_digits();
    }
}

void DigitNumberSelector::set_value(int64_t p_value) {
    const int64_t clamped = std::clamp<int64_t>(p_value, min_value, max_value);
    if (clamped == value) {
        refresh_digits();
        return;
    }

    value = clamped;
    refresh_digits();
    emit_signal("value_changed", value);
}

void DigitNumberSelector::set_value_no_signal(int64_t p_value) {
    const int64_t clamped = std::clamp<int64_t>(p_value, min_value, max_value);
    if (clamped == value) {
        refresh_digits();
        return;
    }

    value = clamped;
    refresh_digits();
}

int64_t DigitNumberSelector::get_value() const {
    return value;
}

void DigitNumberSelector::set_limits(int64_t p_min_value, int64_t p_max_value) {
    if (p_min_value <= p_max_value) {
        min_value = p_min_value;
        max_value = p_max_value;
    } else {
        min_value = p_max_value;
        max_value = p_min_value;
    }

    set_value_no_signal(value);
}

void DigitNumberSelector::set_min_value(int64_t p_value) {
    set_limits(p_value, max_value);
}

int64_t DigitNumberSelector::get_min_value() const {
    return min_value;
}

void DigitNumberSelector::set_max_value(int64_t p_value) {
    set_limits(min_value, p_value);
}

int64_t DigitNumberSelector::get_max_value() const {
    return max_value;
}

void DigitNumberSelector::set_digit_count(int32_t p_count) {
    const int32_t clamped = std::clamp(p_count, 1, 18);
    if (digit_count == clamped) {
        return;
    }

    digit_count = clamped;
    rebuild_ui();
}

int32_t DigitNumberSelector::get_digit_count() const {
    return digit_count;
}

void DigitNumberSelector::set_group_size(int32_t p_size) {
    const int32_t clamped = std::clamp(p_size, 1, 9);
    if (group_size == clamped) {
        return;
    }

    group_size = clamped;
    rebuild_ui();
}

int32_t DigitNumberSelector::get_group_size() const {
    return group_size;
}

void DigitNumberSelector::set_group_labels(const PackedStringArray &p_labels) {
    group_labels = p_labels;
    rebuild_ui();
}

PackedStringArray DigitNumberSelector::get_group_labels() const {
    return group_labels;
}

void DigitNumberSelector::set_show_group_labels(bool p_enabled) {
    if (show_group_labels == p_enabled) {
        return;
    }

    show_group_labels = p_enabled;
    rebuild_ui();
}

bool DigitNumberSelector::get_show_group_labels() const {
    return show_group_labels;
}

void DigitNumberSelector::set_show_separators(bool p_enabled) {
    if (show_separators == p_enabled) {
        return;
    }

    show_separators = p_enabled;
    rebuild_ui();
}

bool DigitNumberSelector::get_show_separators() const {
    return show_separators;
}

void DigitNumberSelector::set_suffix_text(const String &p_text) {
    suffix_text = p_text;

    if (suffix_label != nullptr) {
        if (suffix_text.is_empty()) {
            suffix_label->set_visible(false);
        } else {
            suffix_label->set_text(String(" ") + suffix_text);
            suffix_label->set_visible(true);
        }
    }
}

String DigitNumberSelector::get_suffix_text() const {
    return suffix_text;
}

void DigitNumberSelector::rebuild_ui() {
    while (get_child_count() > 0) {
        Node *child = get_child(0);
        remove_child(child);
        memdelete(child);
    }

    root_row = nullptr;
    sign_label = nullptr;
    sections_row = nullptr;
    suffix_label = nullptr;
    digit_buttons.clear();
    focused_digit_index = -1;

    set_h_size_flags(Control::SIZE_EXPAND_FILL);
    set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    set_alignment(BoxContainer::ALIGNMENT_CENTER);
    add_theme_constant_override("separation", 0);

    root_row = memnew(HBoxContainer);
    root_row->set_name("RootRow");
    root_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    root_row->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    root_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    root_row->add_theme_constant_override("separation", 1);
    add_child(root_row);

    if (min_value < 0) {
        sign_label = memnew(Label);
        sign_label->set_name("SignLabel");
        sign_label->set_custom_minimum_size(Vector2(8.0, 0.0));
        sign_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        root_row->add_child(sign_label);
    }

    sections_row = memnew(HBoxContainer);
    sections_row->set_name("SectionsRow");
    sections_row->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    sections_row->add_theme_constant_override("separation", 1);
    root_row->add_child(sections_row);

    const int32_t actual_group_size = std::max(1, group_size);
    const int32_t group_count = (digit_count + actual_group_size - 1) / actual_group_size;
    const int32_t leading_group_digits = (digit_count % actual_group_size == 0) ? actual_group_size : (digit_count % actual_group_size);

    PackedStringArray labels = group_labels;
    if (labels.is_empty() && group_count == 4) {
        labels.push_back("GHz");
        labels.push_back("MHz");
        labels.push_back("kHz");
        labels.push_back("Hz");
    }

    Ref<StyleBoxEmpty> digit_style;
    digit_style.instantiate();

    int32_t global_digit_index = 0;
    for (int32_t group_index = 0; group_index < group_count; group_index++) {
        const int32_t digits_in_group = (group_index == 0) ? leading_group_digits : actual_group_size;

        VBoxContainer *group_box = memnew(VBoxContainer);
        group_box->set_name(String("GroupBox") + String::num_int64(group_index));
        group_box->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
        group_box->add_theme_constant_override("separation", 0);
        sections_row->add_child(group_box);

        Label *group_label = memnew(Label);
        group_label->set_name(String("GroupLabel") + String::num_int64(group_index));
        group_label->add_theme_font_size_override("font_size", 10);
        group_label->add_theme_color_override("font_color", Color(0.72, 0.72, 0.72, 1.0));
        group_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        if (show_group_labels && group_index < labels.size()) {
            group_label->set_text(labels[group_index]);
        } else {
            group_label->set_text(" ");
        }
        group_box->add_child(group_label);

        HBoxContainer *digits_row = memnew(HBoxContainer);
        digits_row->set_name("DigitsRow");
        digits_row->add_theme_constant_override("separation", 0);
        group_box->add_child(digits_row);

        for (int32_t local_digit = 0; local_digit < digits_in_group; local_digit++) {
            Button *digit_button = memnew(Button);
            digit_button->set_name(String("Digit") + String::num_int64(global_digit_index));
            digit_button->set_text("0");
            digit_button->set_flat(true);
            digit_button->set_focus_mode(FOCUS_ALL);
            digit_button->set_custom_minimum_size(Vector2(8.0, 0.0));
            digit_button->add_theme_stylebox_override("normal", digit_style);
            digit_button->add_theme_stylebox_override("hover", digit_style);
            digit_button->add_theme_stylebox_override("pressed", digit_style);
            digit_button->add_theme_stylebox_override("focus", digit_style);
            digit_button->add_theme_stylebox_override("disabled", digit_style);
            digits_row->add_child(digit_button);

            const Callable bound = Callable(this, "_on_digit_gui_input").bind(global_digit_index);
            digit_button->connect("gui_input", bound);
            const Callable hover_cb = Callable(this, "_on_digit_mouse_entered").bind(global_digit_index);
            digit_button->connect("mouse_entered", hover_cb);

            digit_buttons.push_back(digit_button);
            global_digit_index++;
        }

        if (show_separators && group_index < group_count - 1) {
            VBoxContainer *dot_box = memnew(VBoxContainer);
            dot_box->set_name(String("DotBox") + String::num_int64(group_index));
            dot_box->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
            dot_box->add_theme_constant_override("separation", 0);
            sections_row->add_child(dot_box);

            Label *dot_spacer = memnew(Label);
            dot_spacer->set_name(String("DotSpacer") + String::num_int64(group_index));
            dot_spacer->set_text(" ");
            dot_spacer->add_theme_font_size_override("font_size", 10);
            dot_box->add_child(dot_spacer);

            Label *dot_label = memnew(Label);
            dot_label->set_name(String("DotLabel") + String::num_int64(group_index));
            dot_label->set_text(".");
            dot_box->add_child(dot_label);
        }
    }

    suffix_label = memnew(Label);
    suffix_label->set_name("SuffixLabel");
    root_row->add_child(suffix_label);
    set_suffix_text(suffix_text);

    refresh_digits();
}

void DigitNumberSelector::refresh_digits() {
    if (digit_buttons.is_empty()) {
        return;
    }

    int64_t abs_value = value;
    if (abs_value < 0) {
        abs_value = -abs_value;
    }

    String value_text = String::num_int64(abs_value);
    while (value_text.length() < digit_count) {
        value_text = String("0") + value_text;
    }

    if (value_text.length() > digit_count) {
        value_text = value_text.substr(value_text.length() - digit_count, digit_count);
    }

    for (int32_t i = 0; i < digit_buttons.size() && i < value_text.length(); i++) {
        Button *digit_button = digit_buttons[i];
        if (digit_button == nullptr) {
            continue;
        }

        digit_button->set_text(value_text.substr(i, 1));
    }

    if (sign_label != nullptr) {
        sign_label->set_text(value < 0 ? "-" : " ");
    }
}

int64_t DigitNumberSelector::get_step_for_digit(int32_t p_digit_index) const {
    if (p_digit_index < 0 || p_digit_index >= digit_count) {
        return 1;
    }

    const int32_t exponent = digit_count - 1 - p_digit_index;
    int64_t step = 1;
    for (int32_t i = 0; i < exponent; i++) {
        step *= 10;
    }

    return step;
}

void DigitNumberSelector::adjust_digit(int32_t p_digit_index, int32_t p_delta) {
    if (p_delta == 0) {
        return;
    }

    const int64_t step = get_step_for_digit(p_digit_index);
    const int64_t next_value = value + static_cast<int64_t>(p_delta) * step;
    set_value(next_value);
}

void DigitNumberSelector::set_digit_value(int32_t p_digit_index, int32_t p_digit) {
    if (p_digit_index < 0 || p_digit_index >= digit_count) {
        return;
    }

    const int32_t digit = std::clamp(p_digit, 0, 9);
    const bool negative = value < 0;
    int64_t abs_value = value;
    if (abs_value < 0) {
        abs_value = -abs_value;
    }

    const int64_t step = get_step_for_digit(p_digit_index);
    const int32_t current_digit = static_cast<int32_t>((abs_value / step) % 10);
    const int64_t next_abs = abs_value + static_cast<int64_t>(digit - current_digit) * step;

    int64_t next_value = negative ? -next_abs : next_abs;
    if (next_value == 0 && min_value < 0 && max_value > 0) {
        next_value = 0;
    }
    set_value(next_value);
}

void DigitNumberSelector::focus_digit(int32_t p_digit_index, bool p_select) {
    if (p_digit_index < 0 || p_digit_index >= digit_buttons.size()) {
        return;
    }

    Button *digit_button = digit_buttons[p_digit_index];
    if (digit_button == nullptr) {
        return;
    }

    focused_digit_index = p_digit_index;
    digit_button->grab_focus();
    if (p_select) {
        digit_button->set_pressed_no_signal(true);
        digit_button->set_pressed_no_signal(false);
    }
}

void DigitNumberSelector::_on_digit_gui_input(const Ref<InputEvent> &p_event, int32_t p_digit_index) {
    Ref<InputEventMouseButton> mouse_event = p_event;
    if (mouse_event.is_valid() && mouse_event->is_pressed()) {
        const MouseButton button = static_cast<MouseButton>(mouse_event->get_button_index());
        if (button == MOUSE_BUTTON_LEFT) {
            focus_digit(p_digit_index, true);
            accept_event();
            return;
        }
        if (button == MOUSE_BUTTON_WHEEL_UP) {
            adjust_digit(p_digit_index, 1);
            accept_event();
            return;
        }
        if (button == MOUSE_BUTTON_WHEEL_DOWN) {
            adjust_digit(p_digit_index, -1);
            accept_event();
            return;
        }
    }

    Ref<InputEventKey> key_event = p_event;
    if (key_event.is_null() || !key_event->is_pressed() || key_event->is_echo()) {
        return;
    }

    const Key keycode = static_cast<Key>(key_event->get_keycode());
    if (keycode == KEY_LEFT) {
        focus_digit(std::max(0, p_digit_index - 1), true);
        accept_event();
        return;
    }
    if (keycode == KEY_RIGHT) {
        focus_digit(std::min(digit_count - 1, p_digit_index + 1), true);
        accept_event();
        return;
    }
    if (keycode == KEY_UP) {
        adjust_digit(p_digit_index, 1);
        accept_event();
        return;
    }
    if (keycode == KEY_DOWN) {
        adjust_digit(p_digit_index, -1);
        accept_event();
        return;
    }

    int32_t typed_digit = -1;
    if (keycode >= KEY_0 && keycode <= KEY_9) {
        typed_digit = static_cast<int32_t>(keycode - KEY_0);
    } else if (keycode >= KEY_KP_0 && keycode <= KEY_KP_9) {
        typed_digit = static_cast<int32_t>(keycode - KEY_KP_0);
    }

    if (typed_digit >= 0) {
        set_digit_value(p_digit_index, typed_digit);
        focus_digit(std::min(digit_count - 1, p_digit_index + 1), true);
        accept_event();
    }
}

void DigitNumberSelector::_on_digit_mouse_entered(int32_t p_digit_index) {
    focus_digit(p_digit_index, false);
}
