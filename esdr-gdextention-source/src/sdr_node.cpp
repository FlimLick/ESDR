#include "sdr_node.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void SDR::_bind_methods() {
    ClassDB::bind_static_method("SDR", D_METHOD("signal_color", "kind"), &SDR::signal_color);
    ClassDB::bind_method(D_METHOD("set_problem_state", "enabled", "message"), &SDR::set_problem_state, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("has_problem_state"), &SDR::has_problem_state);
    ClassDB::bind_method(D_METHOD("get_problem_message"), &SDR::get_problem_message);

    BIND_ENUM_CONSTANT(SIGNAL_INT);
    BIND_ENUM_CONSTANT(SIGNAL_BOOL);
    BIND_ENUM_CONSTANT(SIGNAL_FLOAT);
    BIND_ENUM_CONSTANT(SIGNAL_BASEBAND);
    BIND_ENUM_CONSTANT(SIGNAL_AUDIO);
}

Color SDR::signal_color(int32_t p_kind) {
    switch (p_kind) {
        case SIGNAL_INT:
            return Color(1.0, 0.93333334, 0.0, 1.0);
        case SIGNAL_BOOL:
            return Color(0.65, 0.65, 0.65, 1.0);
        case SIGNAL_FLOAT:
            return Color(1.0, 0.22, 0.22, 1.0);
        case SIGNAL_BASEBAND:
            return Color(0.0, 0.46666667, 1.0, 1.0);
        case SIGNAL_AUDIO:
            return Color(0.36078432, 1.0, 0.43529412, 1.0);
        default:
            return Color(1.0, 1.0, 1.0, 1.0);
    }
}

void SDR::set_problem_state(bool p_enabled, const String &p_message) {
    problem_state = p_enabled;
    problem_message = p_message;

    if (p_enabled) {
        // Tint the entire node red while preserving the active theme/stylebox.
        set_self_modulate(Color(1.0, 0.68, 0.68, 1.0));
        set_tooltip_text(problem_message);
        return;
    }

    set_self_modulate(Color(1.0, 1.0, 1.0, 1.0));
    set_tooltip_text(String());
}

bool SDR::has_problem_state() const {
    return problem_state;
}

String SDR::get_problem_message() const {
    return problem_message;
}
