#include "rtl_sdr_backend.h"

#include <godot_cpp/core/class_db.hpp>

#include <algorithm>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#ifdef ESDR_HAS_SOAPY
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Errors.hpp>
#include <SoapySDR/Formats.hpp>
#endif

using namespace godot;

namespace {
std::string trim_copy(const std::string &p_value) {
    const auto first = p_value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = p_value.find_last_not_of(" \t\n\r");
    return p_value.substr(first, last - first + 1);
}

#ifdef ESDR_HAS_SOAPY
SoapySDR::Kwargs parse_device_args(const String &p_args) {
    SoapySDR::Kwargs kwargs;

    const std::string raw = p_args.utf8().get_data();
    std::stringstream ss(raw);
    std::string pair;

    while (std::getline(ss, pair, ',')) {
        pair = trim_copy(pair);
        if (pair.empty()) {
            continue;
        }

        const auto eq = pair.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = trim_copy(pair.substr(0, eq));
        std::string value = trim_copy(pair.substr(eq + 1));
        if (key.empty()) {
            continue;
        }

        kwargs[key] = value;
    }

    return kwargs;
}
#endif
} // namespace

void RTLSDRBackend::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start"), &RTLSDRBackend::start);
    ClassDB::bind_method(D_METHOD("stop"), &RTLSDRBackend::stop);
    ClassDB::bind_method(D_METHOD("is_running"), &RTLSDRBackend::is_running);

    ClassDB::bind_method(D_METHOD("pull_baseband_frame", "max_samples"), &RTLSDRBackend::pull_baseband_frame, DEFVAL(16384));
    ClassDB::bind_method(D_METHOD("consume_error"), &RTLSDRBackend::consume_error);
    ClassDB::bind_method(D_METHOD("get_last_status_message"), &RTLSDRBackend::get_last_status_message);
    ClassDB::bind_method(D_METHOD("enumerate_devices"), &RTLSDRBackend::enumerate_devices);

    ClassDB::bind_method(D_METHOD("set_driver", "driver"), &RTLSDRBackend::set_driver);
    ClassDB::bind_method(D_METHOD("get_driver"), &RTLSDRBackend::get_driver);

    ClassDB::bind_method(D_METHOD("set_device_args", "device_args"), &RTLSDRBackend::set_device_args);
    ClassDB::bind_method(D_METHOD("get_device_args"), &RTLSDRBackend::get_device_args);

    ClassDB::bind_method(D_METHOD("set_channel", "channel"), &RTLSDRBackend::set_channel);
    ClassDB::bind_method(D_METHOD("get_channel"), &RTLSDRBackend::get_channel);

    ClassDB::bind_method(D_METHOD("set_frequency_hz", "frequency_hz"), &RTLSDRBackend::set_frequency_hz);
    ClassDB::bind_method(D_METHOD("get_frequency_hz"), &RTLSDRBackend::get_frequency_hz);

    ClassDB::bind_method(D_METHOD("set_sample_rate", "sample_rate"), &RTLSDRBackend::set_sample_rate);
    ClassDB::bind_method(D_METHOD("get_sample_rate"), &RTLSDRBackend::get_sample_rate);

    ClassDB::bind_method(D_METHOD("set_gain_db", "gain_db"), &RTLSDRBackend::set_gain_db);
    ClassDB::bind_method(D_METHOD("get_gain_db"), &RTLSDRBackend::get_gain_db);

    ClassDB::bind_method(D_METHOD("set_bias_t_enabled", "enabled"), &RTLSDRBackend::set_bias_t_enabled);
    ClassDB::bind_method(D_METHOD("get_bias_t_enabled"), &RTLSDRBackend::get_bias_t_enabled);

    ClassDB::bind_method(D_METHOD("set_rtl_agc_enabled", "enabled"), &RTLSDRBackend::set_rtl_agc_enabled);
    ClassDB::bind_method(D_METHOD("get_rtl_agc_enabled"), &RTLSDRBackend::get_rtl_agc_enabled);

    ClassDB::bind_method(D_METHOD("set_tuner_agc_enabled", "enabled"), &RTLSDRBackend::set_tuner_agc_enabled);
    ClassDB::bind_method(D_METHOD("get_tuner_agc_enabled"), &RTLSDRBackend::get_tuner_agc_enabled);

    ClassDB::bind_method(D_METHOD("set_iq_correction_enabled", "enabled"), &RTLSDRBackend::set_iq_correction_enabled);
    ClassDB::bind_method(D_METHOD("get_iq_correction_enabled"), &RTLSDRBackend::get_iq_correction_enabled);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "driver"), "set_driver", "get_driver");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "device_args"), "set_device_args", "get_device_args");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_RANGE, "0,32,1"), "set_channel", "get_channel");

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frequency_hz", PROPERTY_HINT_RANGE, "0,3000000000,1,or_greater"), "set_frequency_hz", "get_frequency_hz");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sample_rate", PROPERTY_HINT_RANGE, "1000,100000000,1,or_greater"), "set_sample_rate", "get_sample_rate");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gain_db", PROPERTY_HINT_RANGE, "0,100,0.1"), "set_gain_db", "get_gain_db");

    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bias_t_enabled"), "set_bias_t_enabled", "get_bias_t_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rtl_agc_enabled"), "set_rtl_agc_enabled", "get_rtl_agc_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "tuner_agc_enabled"), "set_tuner_agc_enabled", "get_tuner_agc_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "iq_correction_enabled"), "set_iq_correction_enabled", "get_iq_correction_enabled");
}

RTLSDRBackend::RTLSDRBackend() {
    set_status("Idle");
}

RTLSDRBackend::~RTLSDRBackend() {
    stop();
}

void RTLSDRBackend::set_status(const String &p_status) {
    std::lock_guard<std::mutex> lock(status_mutex);
    last_status_message = p_status;
}

void RTLSDRBackend::set_error(const String &p_error) {
    std::lock_guard<std::mutex> lock(status_mutex);
    pending_error = p_error;
    last_status_message = p_error;
}

#ifdef ESDR_HAS_SOAPY
void RTLSDRBackend::cleanup_device_locked() {
    if (device == nullptr) {
        rx_stream = nullptr;
        return;
    }

    if (rx_stream != nullptr) {
        try {
            device->deactivateStream(rx_stream);
        } catch (...) {
        }

        try {
            device->closeStream(rx_stream);
        } catch (...) {
        }
        rx_stream = nullptr;
    }

    try {
        SoapySDR::Device::unmake(device);
    } catch (...) {
    }

    device = nullptr;
}

void RTLSDRBackend::apply_runtime_config_locked() {
    if (device == nullptr) {
        return;
    }

    try {
        device->setSampleRate(SOAPY_SDR_RX, channel, sample_rate);
    } catch (const std::exception &e) {
        set_error(String("setSampleRate failed: ") + String(e.what()));
    }

    try {
        device->setFrequency(SOAPY_SDR_RX, channel, frequency_hz);
    } catch (const std::exception &e) {
        set_error(String("setFrequency failed: ") + String(e.what()));
    }

    try {
        device->setGainMode(SOAPY_SDR_RX, channel, tuner_agc_enabled);
        if (!tuner_agc_enabled) {
            device->setGain(SOAPY_SDR_RX, channel, gain_db);
        }
    } catch (const std::exception &e) {
        set_error(String("setGain failed: ") + String(e.what()));
    }

    // Different SoapyRTLSDR builds expose these keys with different capitalization.
    const char *bias_keys[] = { "biastee", "bias_t", "BiasT" };
    const char *bias_value = bias_t_enabled ? "true" : "false";
    for (const char *key : bias_keys) {
        try {
            device->writeSetting(SOAPY_SDR_RX, channel, key, bias_value);
        } catch (...) {
        }
    }

    const char *rtl_agc_keys[] = { "rtl_agc", "RTLAGC", "agc" };
    const char *rtl_agc_value = rtl_agc_enabled ? "true" : "false";
    for (const char *key : rtl_agc_keys) {
        try {
            device->writeSetting(SOAPY_SDR_RX, channel, key, rtl_agc_value);
        } catch (...) {
        }
    }

    const char *iq_corr_keys[] = { "iq_balance_mode", "iq_correction", "iq_corr" };
    const char *iq_corr_value = iq_correction_enabled ? "true" : "false";
    for (const char *key : iq_corr_keys) {
        try {
            device->writeSetting(SOAPY_SDR_RX, channel, key, iq_corr_value);
        } catch (...) {
        }
    }
}
#endif

bool RTLSDRBackend::start() {
    if (running.load()) {
        return true;
    }

#ifndef ESDR_HAS_SOAPY
    set_error("SoapySDR was not linked into this build.");
    return false;
#else
    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        sample_queue.clear();
    }

    {
        std::lock_guard<std::mutex> lock(status_mutex);
        pending_error = String();
    }

    std::lock_guard<std::mutex> lock(state_mutex);
    cleanup_device_locked();

    SoapySDR::Kwargs kwargs = parse_device_args(device_args);
    if (!driver.is_empty()) {
        kwargs["driver"] = driver.utf8().get_data();
    }

    try {
        device = SoapySDR::Device::make(kwargs);
    } catch (const std::exception &e) {
        set_error(String("Device open failed: ") + String(e.what()));
        return false;
    }

    if (device == nullptr) {
        set_error("Device open failed: no matching SDR device.");
        return false;
    }

    try {
        if (channel < 0 || static_cast<size_t>(channel) >= device->getNumChannels(SOAPY_SDR_RX)) {
            set_error("Invalid RX channel index.");
            cleanup_device_locked();
            return false;
        }
    } catch (const std::exception &e) {
        set_error(String("Failed to query channels: ") + String(e.what()));
        cleanup_device_locked();
        return false;
    }

    apply_runtime_config_locked();

    try {
        rx_stream = device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
    } catch (const std::exception &e) {
        set_error(String("setupStream failed: ") + String(e.what()));
        cleanup_device_locked();
        return false;
    }

    if (rx_stream == nullptr) {
        set_error("setupStream failed: returned null stream.");
        cleanup_device_locked();
        return false;
    }

    try {
        const int result = device->activateStream(rx_stream);
        if (result != 0) {
            set_error(String("activateStream failed: ") + String(SoapySDR::errToStr(result)));
            cleanup_device_locked();
            return false;
        }
    } catch (const std::exception &e) {
        set_error(String("activateStream failed: ") + String(e.what()));
        cleanup_device_locked();
        return false;
    }

    stop_requested.store(false);
    running.store(true);
    worker_thread = std::thread(&RTLSDRBackend::stream_loop, this);
    set_status("Running");

    return true;
#endif
}

void RTLSDRBackend::stop() {
    stop_requested.store(true);

    if (worker_thread.joinable()) {
        worker_thread.join();
    }

#ifdef ESDR_HAS_SOAPY
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        cleanup_device_locked();
    }
#endif

    running.store(false);
    if (get_last_status_message() == "Running") {
        set_status("Stopped");
    }
}

bool RTLSDRBackend::is_running() const {
    return running.load();
}

PackedVector2Array RTLSDRBackend::pull_baseband_frame(int32_t p_max_samples) {
    PackedVector2Array output;
    if (p_max_samples <= 0) {
        return output;
    }

    std::vector<std::complex<float>> local;
    local.reserve(static_cast<size_t>(p_max_samples));

    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        const size_t count = std::min(sample_queue.size(), static_cast<size_t>(p_max_samples));
        local.resize(count);

        for (size_t i = 0; i < count; i++) {
            local[i] = sample_queue.front();
            sample_queue.pop_front();
        }
    }

    output.resize(static_cast<int32_t>(local.size()));
    for (size_t i = 0; i < local.size(); i++) {
        output.set(static_cast<int32_t>(i), Vector2(local[i].real(), local[i].imag()));
    }

    return output;
}

String RTLSDRBackend::consume_error() {
    std::lock_guard<std::mutex> lock(status_mutex);
    const String error = pending_error;
    pending_error = String();
    return error;
}

String RTLSDRBackend::get_last_status_message() const {
    std::lock_guard<std::mutex> lock(status_mutex);
    return last_status_message;
}

PackedStringArray RTLSDRBackend::enumerate_devices() const {
    PackedStringArray devices;

#ifdef ESDR_HAS_SOAPY
    try {
        const auto results = SoapySDR::Device::enumerate();
        for (const auto &entry : results) {
            String line;
            bool first = true;
            for (const auto &kv : entry) {
                if (!first) {
                    line += ", ";
                }
                line += String(kv.first.c_str()) + "=" + String(kv.second.c_str());
                first = false;
            }
            devices.push_back(line);
        }
    } catch (...) {
    }
#endif

    return devices;
}

void RTLSDRBackend::set_driver(const String &p_driver) {
    driver = p_driver.strip_edges();
}

String RTLSDRBackend::get_driver() const {
    return driver;
}

void RTLSDRBackend::set_device_args(const String &p_args) {
    device_args = p_args.strip_edges();
}

String RTLSDRBackend::get_device_args() const {
    return device_args;
}

void RTLSDRBackend::set_channel(int32_t p_channel) {
    channel = std::max<int32_t>(0, p_channel);
}

int32_t RTLSDRBackend::get_channel() const {
    return channel;
}

void RTLSDRBackend::set_frequency_hz(double p_frequency_hz) {
    frequency_hz = std::max(0.0, p_frequency_hz);

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (running.load() && device != nullptr) {
        try {
            device->setFrequency(SOAPY_SDR_RX, channel, frequency_hz);
        } catch (const std::exception &e) {
            set_error(String("setFrequency failed: ") + String(e.what()));
        }
    }
#endif
}

double RTLSDRBackend::get_frequency_hz() const {
    return frequency_hz;
}

void RTLSDRBackend::set_sample_rate(double p_sample_rate) {
    sample_rate = std::max(1000.0, p_sample_rate);

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (running.load() && device != nullptr) {
        try {
            device->setSampleRate(SOAPY_SDR_RX, channel, sample_rate);
        } catch (const std::exception &e) {
            set_error(String("setSampleRate failed: ") + String(e.what()));
        }
    }
#endif
}

double RTLSDRBackend::get_sample_rate() const {
    return sample_rate;
}

void RTLSDRBackend::set_gain_db(float p_gain_db) {
    gain_db = std::clamp(p_gain_db, 0.0f, 100.0f);

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (running.load() && device != nullptr) {
        try {
            if (!tuner_agc_enabled) {
                device->setGain(SOAPY_SDR_RX, channel, gain_db);
            }
        } catch (const std::exception &e) {
            set_error(String("setGain failed: ") + String(e.what()));
        }
    }
#endif
}

float RTLSDRBackend::get_gain_db() const {
    return gain_db;
}

void RTLSDRBackend::set_bias_t_enabled(bool p_enabled) {
    bias_t_enabled = p_enabled;

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (running.load() && device != nullptr) {
        apply_runtime_config_locked();
    }
#endif
}

bool RTLSDRBackend::get_bias_t_enabled() const {
    return bias_t_enabled;
}

void RTLSDRBackend::set_rtl_agc_enabled(bool p_enabled) {
    rtl_agc_enabled = p_enabled;

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (running.load() && device != nullptr) {
        apply_runtime_config_locked();
    }
#endif
}

bool RTLSDRBackend::get_rtl_agc_enabled() const {
    return rtl_agc_enabled;
}

void RTLSDRBackend::set_tuner_agc_enabled(bool p_enabled) {
    tuner_agc_enabled = p_enabled;

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (running.load() && device != nullptr) {
        apply_runtime_config_locked();
    }
#endif
}

bool RTLSDRBackend::get_tuner_agc_enabled() const {
    return tuner_agc_enabled;
}

void RTLSDRBackend::set_iq_correction_enabled(bool p_enabled) {
    iq_correction_enabled = p_enabled;

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (running.load() && device != nullptr) {
        apply_runtime_config_locked();
    }
#endif
}

bool RTLSDRBackend::get_iq_correction_enabled() const {
    return iq_correction_enabled;
}

void RTLSDRBackend::stream_loop() {
#ifndef ESDR_HAS_SOAPY
    running.store(false);
#else
    std::vector<std::complex<float>> buffer;

    {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (device == nullptr || rx_stream == nullptr) {
            set_error("Stream loop started without an active stream.");
            running.store(false);
            return;
        }

        size_t mtu = 4096;
        try {
            const size_t queried_mtu = device->getStreamMTU(rx_stream);
            if (queried_mtu > 0 && queried_mtu < (1u << 20)) {
                mtu = queried_mtu;
            }
        } catch (...) {
        }

        buffer.resize(mtu);
    }

    void *buffers[1] = { buffer.data() };

    while (!stop_requested.load()) {
        int flags = 0;
        long long time_ns = 0;
        int result = SOAPY_SDR_TIMEOUT;

        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (device == nullptr || rx_stream == nullptr) {
                break;
            }

            try {
                result = device->readStream(rx_stream, buffers, buffer.size(), flags, time_ns, 100000);
            } catch (const std::exception &e) {
                set_error(String("readStream exception: ") + String(e.what()));
                stop_requested.store(true);
                break;
            }
        }

        if (result > 0) {
            std::lock_guard<std::mutex> queue_lock(frame_mutex);

            const size_t incoming = static_cast<size_t>(result);
            if (sample_queue.size() + incoming > max_queued_samples) {
                size_t to_drop = sample_queue.size() + incoming - max_queued_samples;
                while (to_drop > 0 && !sample_queue.empty()) {
                    sample_queue.pop_front();
                    to_drop--;
                }
            }

            for (int i = 0; i < result; i++) {
                sample_queue.push_back(buffer[static_cast<size_t>(i)]);
            }
            continue;
        }

        if (result == SOAPY_SDR_TIMEOUT || result == SOAPY_SDR_OVERFLOW) {
            continue;
        }

        set_error(String("readStream failed: ") + String(SoapySDR::errToStr(result)));
        stop_requested.store(true);
        break;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex);
        cleanup_device_locked();
    }

    running.store(false);
    if (get_last_status_message() == "Running") {
        set_status("Stopped");
    }
#endif
}
