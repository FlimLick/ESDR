#include "rtl_sdr_backend.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef ESDR_HAS_SOAPY
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Errors.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Modules.hpp>
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

void log_rtl_info(const String &p_message);
void log_rtl_error(const String &p_message);

#if defined(_WIN32)
void append_path_if_missing(std::string &p_accumulator, const std::string &p_path, char p_separator) {
    if (p_path.empty()) {
        return;
    }

    const std::string normalized = trim_copy(p_path);
    if (normalized.empty()) {
        return;
    }

    auto path_exists = [&](const std::string &target) {
        std::stringstream ss(p_accumulator);
        std::string segment;
        while (std::getline(ss, segment, p_separator)) {
            if (trim_copy(segment) == target) {
                return true;
            }
        }
        return false;
    };

    if (path_exists(normalized)) {
        return;
    }

    if (!p_accumulator.empty()) {
        p_accumulator.push_back(p_separator);
    }
    p_accumulator += normalized;
}
#endif

void append_unique_path(std::vector<std::string> &p_paths, const String &p_path) {
    const std::string normalized = trim_copy(p_path.utf8().get_data());
    if (normalized.empty()) {
        return;
    }
    for (const std::string &existing : p_paths) {
        if (existing == normalized) {
            return;
        }
    }
    p_paths.push_back(normalized);
}

String absolutize_path(const String &p_path, const String &p_exe_dir) {
    String resolved = p_path.simplify_path();
    if (resolved.is_empty()) {
        return resolved;
    }
    if (resolved.begins_with("res://")) {
        ProjectSettings *project_settings = ProjectSettings::get_singleton();
        if (project_settings != nullptr) {
            resolved = project_settings->globalize_path(resolved);
        }
    }
    if (resolved.is_relative_path() && !p_exe_dir.is_empty()) {
        resolved = p_exe_dir.path_join(resolved);
    }
    return resolved.simplify_path();
}

std::vector<std::string> collect_candidate_bin_dirs() {
    std::vector<std::string> bin_dirs;

    OS *os = OS::get_singleton();
    String current_dir;
    try {
        current_dir = String(std::filesystem::current_path().string().c_str()).simplify_path();
    } catch (...) {
        current_dir = String();
    }
    String exe_dir;
    if (os != nullptr) {
        const String exe_path = os->get_executable_path().simplify_path();
        exe_dir = exe_path.get_base_dir();
        if (exe_dir.is_empty() || exe_dir == ".") {
            exe_dir = current_dir;
        } else if (exe_dir.is_relative_path() && !current_dir.is_empty()) {
            exe_dir = current_dir.path_join(exe_dir).simplify_path();
        } else {
            exe_dir = exe_dir.simplify_path();
        }
    }

    ProjectSettings *project_settings = ProjectSettings::get_singleton();
    if (project_settings != nullptr) {
        append_unique_path(bin_dirs, absolutize_path(project_settings->globalize_path("res://bin"), exe_dir));
    }

    if (!current_dir.is_empty()) {
        append_unique_path(bin_dirs, absolutize_path(current_dir.path_join("bin"), current_dir));
        append_unique_path(bin_dirs, absolutize_path(current_dir, current_dir));
    }

    if (!exe_dir.is_empty()) {
        append_unique_path(bin_dirs, absolutize_path(exe_dir.path_join("bin"), exe_dir));
        append_unique_path(bin_dirs, absolutize_path(exe_dir, exe_dir));
    }

    return bin_dirs;
}

std::vector<std::string> collect_candidate_module_dirs(const std::vector<std::string> &p_bin_dirs) {
    std::vector<std::string> module_dirs;

    for (const std::string &bin_dir_utf8 : p_bin_dirs) {
        const String bin_dir = String(bin_dir_utf8.c_str()).simplify_path();
        if (bin_dir.is_empty()) {
            continue;
        }

        // Some Godot export layouts copy dependency DLLs next to the .exe.
        append_unique_path(module_dirs, bin_dir);
        append_unique_path(module_dirs, bin_dir.path_join("soapy-modules"));
        append_unique_path(module_dirs, bin_dir.path_join("SoapySDR/modules0.8"));
        append_unique_path(module_dirs, bin_dir.path_join("SoapySDR/modules0.8-3"));

        if (bin_dir.get_file().to_lower() == "bin") {
            const String root_dir = bin_dir.get_base_dir();
            append_unique_path(module_dirs, root_dir);
            append_unique_path(module_dirs, root_dir.path_join("lib/SoapySDR/modules0.8"));
            append_unique_path(module_dirs, root_dir.path_join("lib/SoapySDR/modules0.8-3"));
        }
    }

#if defined(_WIN32)
    OS *os = OS::get_singleton();
    if (os != nullptr) {
        const PackedStringArray path_entries = os->get_environment("PATH").split(";", false);
        for (int32_t i = 0; i < path_entries.size(); i++) {
            const String path_entry = path_entries[i].strip_edges();
            if (path_entry.is_empty()) {
                continue;
            }
            const String lower = path_entry.to_lower();
            if (lower.contains("soapy") || lower.contains("sdr")) {
                append_unique_path(module_dirs, path_entry);
            }
            if (path_entry.get_file().to_lower() == "bin") {
                const String root_dir = path_entry.get_base_dir();
                append_unique_path(module_dirs, root_dir);
                append_unique_path(module_dirs, root_dir.path_join("lib/SoapySDR/modules0.8"));
                append_unique_path(module_dirs, root_dir.path_join("lib/SoapySDR/modules0.8-3"));
            }
        }
    }
#endif

    return module_dirs;
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

String kwargs_to_string(const SoapySDR::Kwargs &p_kwargs) {
    String out;
    bool first = true;
    for (const auto &kv : p_kwargs) {
        if (!first) {
            out += ", ";
        }
        out += String(kv.first.c_str()) + "=" + String(kv.second.c_str());
        first = false;
    }
    return out;
}

double clamp_gain_to_range(SoapySDR::Device *p_device, int32_t p_channel, const std::string &p_element, double p_gain) {
    if (p_device == nullptr) {
        return p_gain;
    }

    try {
        SoapySDR::Range range = p_element.empty()
                ? p_device->getGainRange(SOAPY_SDR_RX, p_channel)
                : p_device->getGainRange(SOAPY_SDR_RX, p_channel, p_element);
        double clamped = std::clamp(p_gain, range.minimum(), range.maximum());
        const double step = range.step();
        if (step > 0.0) {
            clamped = range.minimum() + (std::round((clamped - range.minimum()) / step) * step);
            clamped = std::clamp(clamped, range.minimum(), range.maximum());
        }
        return clamped;
    } catch (...) {
    }

    return p_gain;
}

void range_list_min_max(const SoapySDR::RangeList &p_ranges, double p_fallback_min, double p_fallback_max, double &r_min_out, double &r_max_out) {
    r_min_out = p_fallback_min;
    r_max_out = p_fallback_max;
    if (p_ranges.empty()) {
        return;
    }

    double min_value = std::numeric_limits<double>::infinity();
    double max_value = -std::numeric_limits<double>::infinity();
    for (const SoapySDR::Range &range : p_ranges) {
        min_value = std::min(min_value, range.minimum());
        max_value = std::max(max_value, range.maximum());
    }

    if (!std::isfinite(min_value) || !std::isfinite(max_value) || max_value <= min_value) {
        return;
    }

    r_min_out = min_value;
    r_max_out = max_value;
}

void set_manual_gain_with_fallback(SoapySDR::Device *p_device, int32_t p_channel, double p_gain_db) {
    if (p_device == nullptr) {
        return;
    }

    bool has_tuner = false;
    try {
        const std::vector<std::string> elements = p_device->listGains(SOAPY_SDR_RX, p_channel);
        for (const std::string &name : elements) {
            if (name == "TUNER") {
                has_tuner = true;
                break;
            }
        }
    } catch (...) {
    }

    if (has_tuner) {
        const double tuner_gain = clamp_gain_to_range(p_device, p_channel, "TUNER", p_gain_db);
        p_device->setGain(SOAPY_SDR_RX, p_channel, "TUNER", tuner_gain);
        UtilityFunctions::print(String("[ESDR][RTL Backend] manual gain applied element=TUNER requested=") + String::num(p_gain_db) +
                " applied=" + String::num(tuner_gain));
        return;
    }

    const double clamped = clamp_gain_to_range(p_device, p_channel, "", p_gain_db);
    p_device->setGain(SOAPY_SDR_RX, p_channel, clamped);
    UtilityFunctions::print(String("[ESDR][RTL Backend] manual gain applied requested=") + String::num(p_gain_db) +
            " applied=" + String::num(clamped));
}

void configure_soapy_runtime_paths() {
    const std::vector<std::string> bin_dirs = collect_candidate_bin_dirs();
    if (bin_dirs.empty()) {
        return;
    }
    const std::vector<std::string> module_dirs = collect_candidate_module_dirs(bin_dirs);

#if defined(_WIN32)
    std::string plugin_path_env;
    if (const char *existing = std::getenv("SOAPY_SDR_PLUGIN_PATH"); existing != nullptr) {
        plugin_path_env = existing;
    }
    for (const std::string &module_dir : module_dirs) {
        append_path_if_missing(plugin_path_env, module_dir, ';');
    }
    _putenv_s("SOAPY_SDR_PLUGIN_PATH", plugin_path_env.c_str());

    std::string path_env;
    if (const char *existing_path = std::getenv("PATH"); existing_path != nullptr) {
        path_env = existing_path;
    }
    for (const std::string &bin_dir : bin_dirs) {
        append_path_if_missing(path_env, bin_dir, ';');
    }
    for (const std::string &module_dir : module_dirs) {
        append_path_if_missing(path_env, module_dir, ';');
    }
    _putenv_s("PATH", path_env.c_str());
#else
    const std::string module_dir = !module_dirs.empty() ? module_dirs.front() : std::string();
    const std::string bin_path = bin_dirs.front();
    if (module_dir.empty() || bin_path.empty()) {
        return;
    }
    setenv("SOAPY_SDR_PLUGIN_PATH", module_dir.c_str(), 1);

    const char *ld_library_path = std::getenv("LD_LIBRARY_PATH");
    if (ld_library_path == nullptr || std::string(ld_library_path).find(bin_path) == std::string::npos) {
        std::string new_ld_library_path = bin_path;
        if (ld_library_path != nullptr && std::string(ld_library_path).size() > 0) {
            new_ld_library_path += ":";
            new_ld_library_path += ld_library_path;
        }
        setenv("LD_LIBRARY_PATH", new_ld_library_path.c_str(), 1);
    }
#endif
}

void initialize_soapy_modules_with_logging() {
    static std::once_flag once;
    std::call_once(once, []() {
        configure_soapy_runtime_paths();

        const std::vector<std::string> bin_dirs = collect_candidate_bin_dirs();
        const std::vector<std::string> explicit_module_dirs = collect_candidate_module_dirs(bin_dirs);
        if (explicit_module_dirs.empty()) {
            return;
        }

        if (const char *plugin_env = std::getenv("SOAPY_SDR_PLUGIN_PATH"); plugin_env != nullptr) {
            log_rtl_info(String("SOAPY_SDR_PLUGIN_PATH=") + String(plugin_env));
        } else {
            log_rtl_info("SOAPY_SDR_PLUGIN_PATH is not set.");
        }
        if (const char *path_env = std::getenv("PATH"); path_env != nullptr) {
            log_rtl_info(String("PATH contains: ") + String(path_env));
        }
        for (const std::string &bin_dir : bin_dirs) {
            const String path = String(bin_dir.c_str());
            log_rtl_info(String("Candidate bin dir: ") + path +
                    String(" exists=") + (DirAccess::dir_exists_absolute(path) ? String("true") : String("false")));
        }

        try {
            const auto search_paths = SoapySDR::listSearchPaths();
            String rendered;
            for (std::size_t i = 0; i < search_paths.size(); i++) {
                if (i > 0) {
                    rendered += "; ";
                }
                rendered += String(search_paths[i].c_str());
            }
            log_rtl_info(String("Soapy search paths: ") + rendered);
        } catch (const std::exception &e) {
            log_rtl_error(String("Failed to query Soapy search paths: ") + String(e.what()));
        }

        for (const std::string &dir : explicit_module_dirs) {
            try {
                const String dir_godot = String(dir.c_str());
                if (!DirAccess::dir_exists_absolute(dir_godot)) {
                    log_rtl_info(String("Module scan dir missing: ") + dir_godot);
                    continue;
                }
                const auto modules = SoapySDR::listModules(dir);
                log_rtl_info(String("Module scan dir=") + String(dir.c_str()) +
                        " count=" + String::num_int64(static_cast<int64_t>(modules.size())));
                for (const std::string &module_path : modules) {
                    const std::string err = SoapySDR::loadModule(module_path);
                    if (err.empty()) {
                        log_rtl_info(String("Loaded Soapy module: ") + String(module_path.c_str()));
                    } else {
                        log_rtl_error(String("Failed to load Soapy module: ") + String(module_path.c_str()) +
                                " err=" + String(err.c_str()));
                    }
                    const SoapySDR::Kwargs loader_results = SoapySDR::getLoaderResult(module_path);
                    for (const auto &entry : loader_results) {
                        log_rtl_info(String("Module registry entry: ") + String(entry.first.c_str()) +
                                " status=" + String(entry.second.c_str()));
                    }
                }
            } catch (const std::exception &e) {
                log_rtl_error(String("Module scan failed for dir=") + String(dir.c_str()) +
                        " err=" + String(e.what()));
            }
        }

        try {
            SoapySDR::loadModules();
        } catch (const std::exception &e) {
            log_rtl_error(String("Soapy loadModules failed: ") + String(e.what()));
        }
    });
}
#endif

void log_rtl_info(const String &p_message) {
    UtilityFunctions::print(String("[ESDR][RTL Backend] ") + p_message);
}

void log_rtl_error(const String &p_message) {
    UtilityFunctions::printerr(String("[ESDR][RTL Backend] ") + p_message);
}
} // namespace

void RTLSDRBackend::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start"), &RTLSDRBackend::start);
    ClassDB::bind_method(D_METHOD("stop"), &RTLSDRBackend::stop);
    ClassDB::bind_method(D_METHOD("is_running"), &RTLSDRBackend::is_running);

    ClassDB::bind_method(D_METHOD("pull_baseband_frame", "max_samples"), &RTLSDRBackend::pull_baseband_frame, DEFVAL(16384));
    ClassDB::bind_method(D_METHOD("get_queued_sample_count"), &RTLSDRBackend::get_queued_sample_count);
    ClassDB::bind_method(D_METHOD("get_effective_sample_rate"), &RTLSDRBackend::get_effective_sample_rate);
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
    ClassDB::bind_method(D_METHOD("get_effective_frequency_hz"), &RTLSDRBackend::get_effective_frequency_hz);
    ClassDB::bind_method(D_METHOD("get_min_frequency_hz"), &RTLSDRBackend::get_min_frequency_hz);
    ClassDB::bind_method(D_METHOD("get_max_frequency_hz"), &RTLSDRBackend::get_max_frequency_hz);

    ClassDB::bind_method(D_METHOD("set_sample_rate", "sample_rate"), &RTLSDRBackend::set_sample_rate);
    ClassDB::bind_method(D_METHOD("get_sample_rate"), &RTLSDRBackend::get_sample_rate);
    ClassDB::bind_method(D_METHOD("get_min_sample_rate"), &RTLSDRBackend::get_min_sample_rate);
    ClassDB::bind_method(D_METHOD("get_max_sample_rate"), &RTLSDRBackend::get_max_sample_rate);

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

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frequency_hz", PROPERTY_HINT_RANGE, "0,999000000000,1,or_greater"), "set_frequency_hz", "get_frequency_hz");
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
    log_rtl_info(String("status: ") + p_status);
}

void RTLSDRBackend::set_error(const String &p_error) {
    std::lock_guard<std::mutex> lock(status_mutex);
    pending_error = p_error;
    last_status_message = p_error;
    log_rtl_error(String("error: ") + p_error);
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

void RTLSDRBackend::update_capabilities_from_device_locked() {
    if (device == nullptr) {
        return;
    }

    try {
        double sr_min = min_sample_rate;
        double sr_max = max_sample_rate;
        range_list_min_max(device->getSampleRateRange(SOAPY_SDR_RX, channel), sr_min, sr_max, sr_min, sr_max);
        if (sr_max > sr_min) {
            min_sample_rate = sr_min;
            max_sample_rate = sr_max;
        }
    } catch (...) {
    }

    try {
        double freq_min = min_frequency_hz;
        double freq_max = max_frequency_hz;
        range_list_min_max(device->getFrequencyRange(SOAPY_SDR_RX, channel), freq_min, freq_max, freq_min, freq_max);
        if (freq_max > freq_min) {
            min_frequency_hz = std::max(0.0, freq_min);
            max_frequency_hz = std::max(min_frequency_hz, freq_max);
        }
    } catch (...) {
    }
}

void RTLSDRBackend::refresh_capabilities_locked() {
    // Reset to safe defaults first.
    min_frequency_hz = 0.0;
    max_frequency_hz = 999000000000.0;
    min_sample_rate = 1000.0;
    max_sample_rate = 999000000000.0;

    if (device != nullptr) {
        update_capabilities_from_device_locked();
        return;
    }

    SoapySDR::Kwargs kwargs = parse_device_args(device_args);
    if (!driver.is_empty()) {
        kwargs["driver"] = driver.utf8().get_data();
    }

    SoapySDR::Device *probe = nullptr;
    try {
        probe = SoapySDR::Device::make(kwargs);
    } catch (...) {
        probe = nullptr;
    }
    if (probe == nullptr) {
        return;
    }

    try {
        const size_t rx_channels = probe->getNumChannels(SOAPY_SDR_RX);
        if (channel < 0 || static_cast<size_t>(channel) >= rx_channels) {
            channel = 0;
        }
    } catch (...) {
        channel = 0;
    }

    try {
        double sr_min = min_sample_rate;
        double sr_max = max_sample_rate;
        range_list_min_max(probe->getSampleRateRange(SOAPY_SDR_RX, channel), sr_min, sr_max, sr_min, sr_max);
        if (sr_max > sr_min) {
            min_sample_rate = sr_min;
            max_sample_rate = sr_max;
        }
    } catch (...) {
    }

    try {
        double freq_min = min_frequency_hz;
        double freq_max = max_frequency_hz;
        range_list_min_max(probe->getFrequencyRange(SOAPY_SDR_RX, channel), freq_min, freq_max, freq_min, freq_max);
        if (freq_max > freq_min) {
            min_frequency_hz = std::max(0.0, freq_min);
            max_frequency_hz = std::max(min_frequency_hz, freq_max);
        }
    } catch (...) {
    }

    try {
        SoapySDR::Device::unmake(probe);
    } catch (...) {
    }
}

void RTLSDRBackend::apply_runtime_config_locked() {
    if (device == nullptr) {
        return;
    }
    update_capabilities_from_device_locked();
    sample_rate = std::clamp(sample_rate, min_sample_rate, max_sample_rate);
    frequency_hz = std::clamp(frequency_hz, min_frequency_hz, max_frequency_hz);
    log_rtl_info(String("apply config: ch=") + String::num_int64(channel) +
            " freq=" + String::num(frequency_hz) +
            " sr=" + String::num(sample_rate) +
            " gain=" + String::num(gain_db) +
            " bias_t=" + (bias_t_enabled ? String("true") : String("false")) +
            " rtl_agc=" + (rtl_agc_enabled ? String("true") : String("false")) +
            " tuner_agc=" + (tuner_agc_enabled ? String("true") : String("false")) +
            " iq_corr=" + (iq_correction_enabled ? String("true") : String("false")));

    try {
        device->setSampleRate(SOAPY_SDR_RX, channel, sample_rate);
        effective_sample_rate = device->getSampleRate(SOAPY_SDR_RX, channel);
    } catch (const std::exception &e) {
        set_error(String("setSampleRate failed: ") + String(e.what()));
        effective_sample_rate = sample_rate;
    }

    try {
        device->setFrequency(SOAPY_SDR_RX, channel, frequency_hz);
        effective_frequency_hz = device->getFrequency(SOAPY_SDR_RX, channel);
    } catch (const std::exception &e) {
        set_error(String("setFrequency failed: ") + String(e.what()));
    }

    try {
        device->setGainMode(SOAPY_SDR_RX, channel, tuner_agc_enabled);
        if (!tuner_agc_enabled) {
            set_manual_gain_with_fallback(device, channel, gain_db);
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
    log_rtl_info("start requested.");
    if (running.load()) {
        log_rtl_info("start ignored: already running.");
        return true;
    }

#ifndef ESDR_HAS_SOAPY
    log_rtl_error("start failed: build has no SoapySDR support.");
    set_error("SoapySDR was not linked into this build.");
    return false;
#else
    initialize_soapy_modules_with_logging();

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
    log_rtl_info(String("opening device with kwargs: ") + kwargs_to_string(kwargs));

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
    log_rtl_info(String("device open success. selected args: ") + get_device_args());

    try {
        const size_t rx_channels = device->getNumChannels(SOAPY_SDR_RX);
        log_rtl_info(String("device reports RX channels: ") + String::num_int64(static_cast<int64_t>(rx_channels)));
        if (channel < 0 || static_cast<size_t>(channel) >= rx_channels) {
            set_error("Invalid RX channel index.");
            cleanup_device_locked();
            return false;
        }
    } catch (const std::exception &e) {
        set_error(String("Failed to query channels: ") + String(e.what()));
        cleanup_device_locked();
        return false;
    }

    refresh_capabilities_locked();
    log_rtl_info(String("capabilities: freq=[") + String::num(min_frequency_hz) + ", " + String::num(max_frequency_hz) +
            "] sr=[" + String::num(min_sample_rate) + ", " + String::num(max_sample_rate) + "]");

    try {
        const std::vector<std::string> gain_elements = device->listGains(SOAPY_SDR_RX, channel);
        String gain_info;
        for (size_t i = 0; i < gain_elements.size(); i++) {
            if (i > 0) {
                gain_info += ", ";
            }
            gain_info += String(gain_elements[i].c_str());
        }
        log_rtl_info(String("available gain elements: [") + gain_info + "]");
    } catch (...) {
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
    log_rtl_info("stream thread started.");

    return true;
#endif
}

void RTLSDRBackend::stop() {
    log_rtl_info("stop requested.");
    stop_requested.store(true);

#ifdef ESDR_HAS_SOAPY
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (device != nullptr && rx_stream != nullptr) {
            try {
                device->deactivateStream(rx_stream, 0, 0);
            } catch (...) {
            }
        }
    }
#endif

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
    log_rtl_info("stop complete.");
}

bool RTLSDRBackend::is_running() const {
    return running.load();
}

PackedVector2Array RTLSDRBackend::pull_baseband_frame(int32_t p_max_samples) {
    PackedVector2Array output;
    if (p_max_samples <= 0) {
        return output;
    }

    std::lock_guard<std::mutex> lock(frame_mutex);
    const size_t count = std::min(sample_queue.size(), static_cast<size_t>(p_max_samples));
    output.resize(static_cast<int32_t>(count));
    Vector2 *out = output.ptrw();
    for (size_t i = 0; i < count; i++) {
        const std::complex<float> sample = sample_queue.front();
        sample_queue.pop_front();
        out[static_cast<int32_t>(i)] = Vector2(sample.real(), sample.imag());
    }

    return output;
}

int32_t RTLSDRBackend::get_queued_sample_count() const {
    std::lock_guard<std::mutex> lock(frame_mutex);
    const size_t size = sample_queue.size();
    if (size > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        return std::numeric_limits<int32_t>::max();
    }
    return static_cast<int32_t>(size);
}

double RTLSDRBackend::get_effective_sample_rate() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return effective_sample_rate;
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
    initialize_soapy_modules_with_logging();

    try {
        log_rtl_info("enumerate_devices begin.");
        const auto results = SoapySDR::Device::enumerate();
        log_rtl_info(String("enumerate_devices raw count: ") + String::num_int64(static_cast<int64_t>(results.size())));
        SoapySDR::Kwargs rtl_filter;
        rtl_filter["driver"] = "rtlsdr";
        const auto rtl_results = SoapySDR::Device::enumerate(rtl_filter);
        log_rtl_info(String("enumerate_devices driver=rtlsdr count: ") + String::num_int64(static_cast<int64_t>(rtl_results.size())));
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
            log_rtl_info(String("enumerate candidate: ") + line);
            devices.push_back(line);
        }
        log_rtl_info(String("enumerate_devices finished. exported count: ") + String::num_int64(devices.size()));
    } catch (const std::exception &e) {
        log_rtl_error(String("enumerate_devices exception: ") + String(e.what()));
    } catch (...) {
        log_rtl_error("enumerate_devices unknown exception.");
    }
#else
    log_rtl_error("enumerate_devices unavailable: extension built without SoapySDR.");
#endif

    return devices;
}

void RTLSDRBackend::set_driver(const String &p_driver) {
    driver = p_driver.strip_edges();
    log_rtl_info(String("set_driver: ") + driver);
#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!running.load()) {
        refresh_capabilities_locked();
    }
#endif
}

String RTLSDRBackend::get_driver() const {
    return driver;
}

void RTLSDRBackend::set_device_args(const String &p_args) {
    device_args = p_args.strip_edges();
    log_rtl_info(String("set_device_args: ") + device_args);
#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!running.load()) {
        refresh_capabilities_locked();
    }
#endif
}

String RTLSDRBackend::get_device_args() const {
    return device_args;
}

void RTLSDRBackend::set_channel(int32_t p_channel) {
    channel = std::max<int32_t>(0, p_channel);
    log_rtl_info(String("set_channel: ") + String::num_int64(channel));
#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!running.load()) {
        refresh_capabilities_locked();
    }
#endif
}

int32_t RTLSDRBackend::get_channel() const {
    return channel;
}

void RTLSDRBackend::set_frequency_hz(double p_frequency_hz) {
    const double clamped_hz = std::clamp(p_frequency_hz, min_frequency_hz, max_frequency_hz);

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    frequency_hz = clamped_hz;
    if (running.load() && device != nullptr) {
        frequency_update_pending = true;
    } else {
        effective_frequency_hz = frequency_hz;
        frequency_update_pending = false;
        log_rtl_info(String("set_frequency_hz: ") + String::num(frequency_hz));
    }
#else
    frequency_hz = clamped_hz;
    effective_frequency_hz = frequency_hz;
    log_rtl_info(String("set_frequency_hz: ") + String::num(frequency_hz));
#endif
}

double RTLSDRBackend::get_frequency_hz() const {
    return frequency_hz;
}

double RTLSDRBackend::get_effective_frequency_hz() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return effective_frequency_hz;
}

double RTLSDRBackend::get_min_frequency_hz() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return min_frequency_hz;
}

double RTLSDRBackend::get_max_frequency_hz() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return max_frequency_hz;
}

void RTLSDRBackend::set_sample_rate(double p_sample_rate) {
    sample_rate = std::clamp(p_sample_rate, min_sample_rate, max_sample_rate);
    effective_sample_rate = sample_rate;
    log_rtl_info(String("set_sample_rate: ") + String::num(sample_rate));

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (running.load() && device != nullptr) {
        try {
            device->setSampleRate(SOAPY_SDR_RX, channel, sample_rate);
            effective_sample_rate = device->getSampleRate(SOAPY_SDR_RX, channel);
        } catch (const std::exception &e) {
            set_error(String("setSampleRate failed: ") + String(e.what()));
        }
    }
#endif
}

double RTLSDRBackend::get_sample_rate() const {
    return sample_rate;
}

double RTLSDRBackend::get_min_sample_rate() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return min_sample_rate;
}

double RTLSDRBackend::get_max_sample_rate() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return max_sample_rate;
}

void RTLSDRBackend::set_gain_db(float p_gain_db) {
    gain_db = std::clamp(p_gain_db, 0.0f, 100.0f);
    log_rtl_info(String("set_gain_db: ") + String::num(gain_db));

#ifdef ESDR_HAS_SOAPY
    std::lock_guard<std::mutex> lock(state_mutex);
    if (running.load() && device != nullptr) {
        try {
            if (!tuner_agc_enabled) {
                set_manual_gain_with_fallback(device, channel, gain_db);
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
    log_rtl_info(String("set_bias_t_enabled: ") + (bias_t_enabled ? String("true") : String("false")));

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
    log_rtl_info(String("set_rtl_agc_enabled: ") + (rtl_agc_enabled ? String("true") : String("false")));

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
    log_rtl_info(String("set_tuner_agc_enabled: ") + (tuner_agc_enabled ? String("true") : String("false")));

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
    log_rtl_info(String("set_iq_correction_enabled: ") + (iq_correction_enabled ? String("true") : String("false")));

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
    log_rtl_info("stream_loop entered.");
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
        log_rtl_info(String("stream_loop MTU: ") + String::num_int64(static_cast<int64_t>(mtu)));
    }

    void *buffers[1] = { buffer.data() };
    bool queue_drop_logged = false;

    while (!stop_requested.load()) {
        SoapySDR::Device *active_device = nullptr;
        SoapySDR::Stream *active_stream = nullptr;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            active_device = device;
            active_stream = rx_stream;
        }
        if (active_device == nullptr || active_stream == nullptr) {
            break;
        }

        bool apply_frequency = false;
        double requested_frequency_hz = 0.0;
        int32_t active_channel = 0;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (frequency_update_pending && device != nullptr) {
                requested_frequency_hz = frequency_hz;
                active_channel = channel;
                frequency_update_pending = false;
                apply_frequency = true;
            }
        }
        if (apply_frequency) {
            try {
                active_device->setFrequency(SOAPY_SDR_RX, active_channel, requested_frequency_hz);
                const double tuned_hz = active_device->getFrequency(SOAPY_SDR_RX, active_channel);
                {
                    std::lock_guard<std::mutex> lock(state_mutex);
                    effective_frequency_hz = tuned_hz;
                }
            } catch (const std::exception &e) {
                set_error(String("setFrequency failed: ") + String(e.what()));
            }
        }

        int flags = 0;
        long long time_ns = 0;
        int result = SOAPY_SDR_TIMEOUT;

        try {
            result = active_device->readStream(active_stream, buffers, buffer.size(), flags, time_ns, 10000);
        } catch (const std::exception &e) {
            set_error(String("readStream exception: ") + String(e.what()));
            stop_requested.store(true);
            break;
        }

        if (result > 0) {
            std::lock_guard<std::mutex> queue_lock(frame_mutex);

            const size_t incoming = static_cast<size_t>(result);
            if (sample_queue.size() + incoming > max_queued_samples) {
                size_t to_drop = sample_queue.size() + incoming - max_queued_samples;
                if (!queue_drop_logged) {
                    log_rtl_info(String("sample queue overflow; dropping oldest samples. queue=") +
                            String::num_int64(static_cast<int64_t>(sample_queue.size())) +
                            " incoming=" + String::num_int64(static_cast<int64_t>(incoming)) +
                            " max=" + String::num_int64(static_cast<int64_t>(max_queued_samples)));
                    queue_drop_logged = true;
                }
                while (to_drop > 0 && !sample_queue.empty()) {
                    sample_queue.pop_front();
                    to_drop--;
                }
            }

            sample_queue.insert(sample_queue.end(), buffer.begin(), buffer.begin() + result);
            continue;
        }

        if (result == SOAPY_SDR_TIMEOUT || result == SOAPY_SDR_OVERFLOW) {
            continue;
        }

        log_rtl_error(String("readStream failed with code ") + String::num_int64(result));
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
    log_rtl_info("stream_loop exited.");
#endif
}
