#ifndef ESDR_RTL_SDR_SOURCE_NODE_H
#define ESDR_RTL_SDR_SOURCE_NODE_H

#include "rtl_sdr_backend.h"
#include "sdr_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/string.hpp>

class RTLSDRSource : public SDR {
    GDCLASS(RTLSDRSource, SDR)

private:
    godot::Ref<RTLSDRBackend> backend;

    godot::SpinBox *frequency_spin = nullptr;
    godot::Button *start_button = nullptr;
    godot::Label *status_label = nullptr;
    godot::CheckBox *bias_t_checkbox = nullptr;
    godot::CheckBox *rtl_agc_checkbox = nullptr;
    godot::CheckBox *tuner_agc_checkbox = nullptr;
    godot::CheckBox *iq_correction_checkbox = nullptr;
    godot::SpinBox *sample_rate_spin = nullptr;
    godot::HSlider *gain_slider = nullptr;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();
    void push_ui_to_backend();
    void set_status(bool p_is_running, const godot::String &p_message);

    void process_tick();
    void on_ready();
    void on_exit_tree();

    void _on_start_toggled(bool p_enabled);
    void _on_frequency_changed(double p_value);
    void _on_sample_rate_changed(double p_value);
    void _on_gain_changed(double p_value);
    void _on_bias_t_toggled(bool p_enabled);
    void _on_rtl_agc_toggled(bool p_enabled);
    void _on_tuner_agc_toggled(bool p_enabled);
    void _on_iq_correction_toggled(bool p_enabled);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);
};

#endif // ESDR_RTL_SDR_SOURCE_NODE_H
