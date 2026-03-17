#ifndef ESDR_AM_DEMODULATOR_NODE_H
#define ESDR_AM_DEMODULATOR_NODE_H

#include "demod_dsp_utils.h"
#include "digit_number_selector.h"
#include "sdr_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cstdint>

class AMDemodulator : public SDR {
    GDCLASS(AMDemodulator, SDR)

private:
    double offset_hz = 0.0;
    double bandwidth_hz = 10000.0;
    double output_volume = 1.0;
    bool lock_to_source_frequency = false;
    int64_t upstream_tuned_frequency_hz = 0;
    double input_sample_rate_hz = 2400000.0;

    DigitNumberSelector *offset_selector = nullptr;
    godot::SpinBox *bandwidth_spin = nullptr;
    godot::SpinBox *output_volume_spin = nullptr;
    godot::Button *tune_to_source_button = nullptr;
    godot::Button *lock_to_source_button = nullptr;
    uint64_t debug_last_log_us = 0;
    int64_t debug_in_samples = 0;
    int64_t debug_out_samples = 0;
    int64_t debug_out_clip_samples = 0;
    float debug_iq_peak = 0.0f;
    float debug_out_peak = 0.0f;
    bool debug_logging_enabled = false;

    bool dsp_config_dirty = true;
    esdr_demod::AMDemodCore demod_core;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();
    void refresh_dsp_config();
    void log_debug_stats_if_due();

    void _on_offset_changed(int64_t p_value);
    void _on_bandwidth_changed(double p_value);
    void _on_output_volume_changed(double p_value);
    void _on_tune_to_source_pressed();
    void _on_lock_to_source_toggled(bool p_enabled);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    void push_baseband_frame(const godot::PackedVector2Array &p_frame);
    void set_upstream_tuned_frequency_hz(int64_t p_frequency_hz);
    void set_input_sample_rate_hz(double p_value);
    double get_input_sample_rate_hz() const;
    void set_offset_hz(double p_value);
    double get_offset_hz() const;

    void set_bandwidth_hz(double p_value);
    double get_bandwidth_hz() const;
    void set_output_volume(double p_value);
    double get_output_volume() const;
    void set_lock_to_source_frequency(bool p_enabled);
    bool get_lock_to_source_frequency() const;
    void set_debug_logging_enabled(bool p_enabled);
    bool get_debug_logging_enabled() const;

    godot::Variant get_port_value(int64_t p_port) const;
    void set_port_value(int64_t p_port, const godot::Variant &p_value);
};

#endif // ESDR_AM_DEMODULATOR_NODE_H
