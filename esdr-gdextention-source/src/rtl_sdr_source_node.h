#ifndef ESDR_RTL_SDR_SOURCE_NODE_H
#define ESDR_RTL_SDR_SOURCE_NODE_H

#include "digit_number_selector.h"
#include "rtl_sdr_backend.h"
#include "sdr_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <atomic>
#include <cstdint>
#include <thread>

class RTLSDRSource : public SDR {
    GDCLASS(RTLSDRSource, SDR)

private:
    godot::Ref<RTLSDRBackend> backend;

    godot::OptionButton *device_selector = nullptr;
    godot::Control *device_row = nullptr;
    godot::Label *device_label = nullptr;
    DigitNumberSelector *frequency_selector = nullptr;

    godot::Button *status_button = nullptr;
    godot::Label *scanning_label = nullptr;
    godot::CheckBox *bias_t_checkbox = nullptr;
    godot::CheckBox *rtl_agc_checkbox = nullptr;
    godot::CheckBox *tuner_agc_checkbox = nullptr;
    godot::CheckBox *iq_correction_checkbox = nullptr;
    DigitNumberSelector *sample_rate_selector = nullptr;
    godot::HSlider *gain_slider = nullptr;
    godot::Label *gain_min_label = nullptr;
    godot::SpinBox *gain_value_spin = nullptr;
    godot::Label *gain_max_label = nullptr;

    int64_t frequency_hz_value = 433000000;
    int64_t sample_rate_value = 2400000;
    int64_t effective_sample_rate_hz_value = 2400000;
    static constexpr int64_t frequency_min_hz_default = 0;
    static constexpr int64_t frequency_max_hz_default = 999000000000LL;
    static constexpr int64_t sample_rate_min_default = 1;
    static constexpr int64_t sample_rate_max_default = 999000000000LL;
    int64_t frequency_min_hz_value = frequency_min_hz_default;
    int64_t frequency_max_hz_value = frequency_max_hz_default;
    int64_t sample_rate_min_hz_value = sample_rate_min_default;
    int64_t sample_rate_max_hz_value = sample_rate_max_default;
    bool has_enumerated_devices = false;
    godot::Vector<godot::String> device_args_by_index;
    uint64_t last_pull_time_us = 0;
    uint64_t debug_last_stream_log_us = 0;
    int64_t debug_stream_samples = 0;
    float debug_stream_iq_peak = 0.0f;
    int64_t last_emitted_tuned_frequency_hz = -1;
    bool frequency_control_locked = false;
    std::thread backend_transition_thread;
    std::atomic<bool> backend_transition_active{false};
    std::atomic<bool> backend_transition_result_ready{false};
    std::atomic<bool> backend_transition_target_running{false};
    std::atomic<bool> backend_transition_succeeded{false};

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();
    void refresh_device_selector();
    void refresh_capability_limits();
    void apply_selector_limits();

    void set_frequency_hz_value(int64_t p_frequency_hz, bool p_push_backend);
    void set_sample_rate_value(int64_t p_sample_rate, bool p_push_backend);
    void set_device_index_value(int32_t p_index, bool p_push_backend);
    void refresh_gain_header();
    void style_metric_label(godot::Label *p_label);
    int32_t wrap_index(int32_t p_index, int32_t p_count) const;

    void push_ui_to_backend();
    void set_status(bool p_is_running, const godot::String &p_message, bool p_is_loading = false);
    void refresh_scan_status_label();
    godot::String get_device_args_for_index(int32_t p_index) const;
    static godot::String extract_friendly_device_label(const godot::String &p_device_args);
    void apply_status_button_theme(const godot::Color &p_color);
    void begin_backend_transition(bool p_target_running);
    void poll_backend_transition();

    void process_tick();
    void on_ready();
    void on_exit_tree();

    void _on_start_toggled(bool p_enabled);
    void _on_frequency_selector_value_changed(int64_t p_value);
    void _on_device_selector_item_selected(int64_t p_index);
    void _on_sample_rate_selector_value_changed(int64_t p_value);
    void _on_gain_changed(double p_value);
    void _on_bias_t_toggled(bool p_enabled);
    void _on_rtl_agc_toggled(bool p_enabled);
    void _on_tuner_agc_toggled(bool p_enabled);
    void _on_iq_correction_toggled(bool p_enabled);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    int64_t get_current_sample_rate_hz() const;
    int64_t get_min_sample_rate_hz() const;
    int64_t get_max_sample_rate_hz() const;
    int64_t get_min_frequency_hz() const;
    int64_t get_max_frequency_hz() const;
    void set_frequency_control_locked(bool p_locked);
    bool get_frequency_control_locked() const;
    void set_scan_frequency_hz(int64_t p_frequency_hz);
    godot::Variant get_port_value(int64_t p_port) const;
    void set_port_value(int64_t p_port, const godot::Variant &p_value);
};

#endif // ESDR_RTL_SDR_SOURCE_NODE_H
