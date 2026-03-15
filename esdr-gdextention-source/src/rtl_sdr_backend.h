#ifndef ESDR_RTL_SDR_BACKEND_H
#define ESDR_RTL_SDR_BACKEND_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <atomic>
#include <complex>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

#ifdef ESDR_HAS_SOAPY
namespace SoapySDR {
class Device;
class Stream;
}
#endif

class RTLSDRBackend : public godot::RefCounted {
    GDCLASS(RTLSDRBackend, godot::RefCounted)

private:
    std::mutex state_mutex;
    std::mutex frame_mutex;
    mutable std::mutex status_mutex;

    std::thread worker_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> stop_requested{false};

    std::deque<std::complex<float>> sample_queue;
    std::size_t max_queued_samples = 262144;

    godot::String last_status_message;
    godot::String pending_error;

    godot::String driver = godot::String("rtlsdr");
    godot::String device_args;
    int32_t channel = 0;

    double frequency_hz = 433000000.0;
    double sample_rate = 2400000.0;
    float gain_db = 25.0f;

    bool bias_t_enabled = false;
    bool rtl_agc_enabled = false;
    bool tuner_agc_enabled = false;
    bool iq_correction_enabled = true;

#ifdef ESDR_HAS_SOAPY
    SoapySDR::Device *device = nullptr;
    SoapySDR::Stream *rx_stream = nullptr;
#endif

    void stream_loop();
    void set_status(const godot::String &p_status);
    void set_error(const godot::String &p_error);

#ifdef ESDR_HAS_SOAPY
    void cleanup_device_locked();
    void apply_runtime_config_locked();
#endif

protected:
    static void _bind_methods();

public:
    RTLSDRBackend();
    ~RTLSDRBackend() override;

    bool start();
    void stop();
    bool is_running() const;

    godot::PackedVector2Array pull_baseband_frame(int32_t p_max_samples = 16384);
    godot::String consume_error();
    godot::String get_last_status_message() const;
    godot::PackedStringArray enumerate_devices() const;

    void set_driver(const godot::String &p_driver);
    godot::String get_driver() const;

    void set_device_args(const godot::String &p_args);
    godot::String get_device_args() const;

    void set_channel(int32_t p_channel);
    int32_t get_channel() const;

    void set_frequency_hz(double p_frequency_hz);
    double get_frequency_hz() const;

    void set_sample_rate(double p_sample_rate);
    double get_sample_rate() const;

    void set_gain_db(float p_gain_db);
    float get_gain_db() const;

    void set_bias_t_enabled(bool p_enabled);
    bool get_bias_t_enabled() const;

    void set_rtl_agc_enabled(bool p_enabled);
    bool get_rtl_agc_enabled() const;

    void set_tuner_agc_enabled(bool p_enabled);
    bool get_tuner_agc_enabled() const;

    void set_iq_correction_enabled(bool p_enabled);
    bool get_iq_correction_enabled() const;
};

#endif // ESDR_RTL_SDR_BACKEND_H
