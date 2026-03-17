#ifndef ESDR_DEMOD_DSP_UTILS_H
#define ESDR_DEMOD_DSP_UTILS_H

#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

// Demod chain structure is inspired by SDR++ demod blocks:
// https://github.com/AlexandreRouma/SDRPlusPlus
// (core/src/dsp/demod/fm.h, broadcast_fm.h, am.h)
// This is a clean-room adaptation for ESDR's Godot node runtime.
namespace esdr_demod {

constexpr double PI = 3.14159265358979323846;

class OnePoleLowPass {
public:
    void configure(double p_cutoff_hz, double p_sample_rate_hz) {
        const double cutoff = std::max(1.0, p_cutoff_hz);
        const double sample_rate = std::max(1.0, p_sample_rate_hz);
        const double exponent = -2.0 * PI * cutoff / sample_rate;
        feedback = std::exp(exponent);
        feedforward = 1.0 - feedback;
    }

    float process(float p_sample) {
        if (!initialized) {
            state = static_cast<double>(p_sample);
            initialized = true;
            return p_sample;
        }
        state = (feedforward * static_cast<double>(p_sample)) + (feedback * state);
        return static_cast<float>(state);
    }

    void reset() {
        initialized = false;
        state = 0.0;
    }

private:
    double feedforward = 1.0;
    double feedback = 0.0;
    double state = 0.0;
    bool initialized = false;
};

class MultiPoleLowPass {
public:
    void configure(double p_cutoff_hz, double p_sample_rate_hz, int32_t p_poles) {
        pole_count = std::clamp(p_poles, 1, 4);
        for (int32_t i = 0; i < pole_count; i++) {
            stages[i].configure(p_cutoff_hz, p_sample_rate_hz);
        }
    }

    float process(float p_sample) {
        float value = p_sample;
        for (int32_t i = 0; i < pole_count; i++) {
            value = stages[i].process(value);
        }
        return value;
    }

    void reset() {
        for (int32_t i = 0; i < 4; i++) {
            stages[i].reset();
        }
    }

private:
    OnePoleLowPass stages[4];
    int32_t pole_count = 1;
};

class DCBlocker {
public:
    void configure(double p_rate) {
        rate = std::clamp(p_rate, 0.0, 0.999999);
    }

    float process(float p_sample) {
        const double x = static_cast<double>(p_sample);
        const double y = x - prev_x + (rate * prev_y);
        prev_x = x;
        prev_y = y;
        return static_cast<float>(y);
    }

    void reset() {
        prev_x = 0.0;
        prev_y = 0.0;
    }

private:
    double rate = 0.995;
    double prev_x = 0.0;
    double prev_y = 0.0;
};

class DeemphasisFilter {
public:
    void configure(double p_tau_seconds, double p_sample_rate_hz) {
        const double tau = std::max(1e-9, p_tau_seconds);
        const double sample_rate = std::max(1.0, p_sample_rate_hz);
        alpha = 1.0 - std::exp(-1.0 / (sample_rate * tau));
    }

    float process(float p_sample) {
        if (!initialized) {
            state = static_cast<double>(p_sample);
            initialized = true;
            return p_sample;
        }
        state += alpha * (static_cast<double>(p_sample) - state);
        return static_cast<float>(state);
    }

    void reset() {
        initialized = false;
        state = 0.0;
    }

private:
    double alpha = 1.0;
    double state = 0.0;
    bool initialized = false;
};

class BiquadNotch {
public:
    void configure(double p_center_hz, double p_q, double p_sample_rate_hz) {
        const double sample_rate = std::max(1.0, p_sample_rate_hz);
        const double center_hz = std::clamp(p_center_hz, 1.0, sample_rate * 0.49);
        const double q = std::max(0.05, p_q);
        const double w0 = 2.0 * PI * center_hz / sample_rate;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double cos_w0 = std::cos(w0);

        const double b0 = 1.0;
        const double b1 = -2.0 * cos_w0;
        const double b2 = 1.0;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cos_w0;
        const double a2 = 1.0 - alpha;

        // Direct Form I normalized coefficients.
        nb0 = b0 / a0;
        nb1 = b1 / a0;
        nb2 = b2 / a0;
        na1 = a1 / a0;
        na2 = a2 / a0;
    }

    float process(float p_sample) {
        const double x = static_cast<double>(p_sample);
        const double y = (nb0 * x) + (nb1 * x1) + (nb2 * x2) - (na1 * y1) - (na2 * y2);
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return static_cast<float>(y);
    }

    void reset() {
        x1 = 0.0;
        x2 = 0.0;
        y1 = 0.0;
        y2 = 0.0;
    }

private:
    double nb0 = 1.0;
    double nb1 = 0.0;
    double nb2 = 0.0;
    double na1 = 0.0;
    double na2 = 0.0;
    double x1 = 0.0;
    double x2 = 0.0;
    double y1 = 0.0;
    double y2 = 0.0;
};

class LinearResampler {
public:
    void configure(double p_in_sample_rate_hz, double p_out_sample_rate_hz) {
        in_sample_rate_hz = std::max(1.0, p_in_sample_rate_hz);
        out_sample_rate_hz = std::max(1.0, p_out_sample_rate_hz);
        step = in_sample_rate_hz / out_sample_rate_hz;
        if (step <= 0.0) {
            step = 1.0;
        }
        if (!has_previous_sample) {
            phase = step;
        }
    }

    void process_block(const std::vector<float> &p_in, godot::PackedFloat32Array &r_out) {
        r_out.resize(0);
        if (p_in.empty()) {
            return;
        }

        if (!has_previous_sample) {
            previous_sample = p_in.front();
            has_previous_sample = true;
        }

        scratch_out.clear();
        scratch_out.reserve(static_cast<size_t>(std::max<double>(1.0, static_cast<double>(p_in.size()) / step + 8.0)));

        const int64_t source_size = static_cast<int64_t>(p_in.size()) + 1; // + previous sample
        while ((phase + 1.0) < static_cast<double>(source_size)) {
            const int64_t idx = static_cast<int64_t>(std::floor(phase));
            const double frac = phase - static_cast<double>(idx);
            const float a = (idx == 0) ? previous_sample : p_in[static_cast<size_t>(idx - 1)];
            const float b = p_in[static_cast<size_t>(idx)];
            scratch_out.push_back(static_cast<float>((a * (1.0 - frac)) + (b * frac)));
            phase += step;
        }

        phase -= static_cast<double>(p_in.size());
        previous_sample = p_in.back();

        const int32_t out_count = static_cast<int32_t>(scratch_out.size());
        r_out.resize(out_count);
        float *out = r_out.ptrw();
        for (int32_t i = 0; i < out_count; i++) {
            out[i] = scratch_out[static_cast<size_t>(i)];
        }
    }

    void reset() {
        phase = step;
        previous_sample = 0.0f;
        has_previous_sample = false;
    }

private:
    double in_sample_rate_hz = 48000.0;
    double out_sample_rate_hz = 48000.0;
    double step = 1.0;
    double phase = 1.0;
    float previous_sample = 0.0f;
    bool has_previous_sample = false;
    std::vector<float> scratch_out;
};

struct FMParams {
    double input_sample_rate_hz = 2400000.0;
    double audio_sample_rate_hz = 48000.0;
    double offset_hz = 0.0;
    double deviation_hz = 5000.0;
    double rf_lowpass_hz = 12000.0;
    double audio_lowpass_hz = 7000.0;
    bool deemphasis_enabled = false;
    double deemphasis_tau_seconds = 75e-6;
};

class FMDemodCore {
public:
    void configure(const FMParams &p_params) {
        params = p_params;
        const double input_sr = std::max(1000.0, params.input_sample_rate_hz);
        const double audio_sr = std::max(8000.0, params.audio_sample_rate_hz);
        const double deviation = std::max(100.0, params.deviation_hz);

        const double min_if_sr = std::max({audio_sr * 4.0, params.rf_lowpass_hz * 8.0, 96000.0});
        decimation = std::max<int32_t>(1, static_cast<int32_t>(std::floor(input_sr / min_if_sr)));
        if_sample_rate = input_sr / static_cast<double>(decimation);
        inv_deviation = if_sample_rate / (2.0 * PI * deviation);
        nco_phase_inc = -2.0 * PI * (params.offset_hz / input_sr);
        nco_inc_cos = std::cos(nco_phase_inc);
        nco_inc_sin = std::sin(nco_phase_inc);

        const double rf_cutoff = std::clamp(params.rf_lowpass_hz, 500.0, if_sample_rate * 0.45);
        const double audio_cutoff = std::clamp(params.audio_lowpass_hz, 300.0, if_sample_rate * 0.45);

        i_lpf.configure(rf_cutoff, input_sr, 3);
        q_lpf.configure(rf_cutoff, input_sr, 3);
        audio_lpf.configure(audio_cutoff, if_sample_rate, 2);
        deemphasis.configure(params.deemphasis_tau_seconds, if_sample_rate);
        resampler.configure(if_sample_rate, audio_sr);
        decimation_phase = 0;
        configured = true;
    }

    void process_frame(const godot::PackedVector2Array &p_frame, godot::PackedFloat32Array &r_audio_out) {
        r_audio_out.resize(0);
        if (!configured || p_frame.is_empty()) {
            return;
        }

        scratch_mono.clear();
        scratch_mono.reserve(static_cast<size_t>(p_frame.size() / std::max<int32_t>(1, decimation) + 8));

        int32_t nco_renorm_counter = 0;
        for (int32_t i = 0; i < p_frame.size(); i++) {
            const godot::Vector2 sample = p_frame[i];
            const double iq_i = static_cast<double>(sample.x);
            const double iq_q = static_cast<double>(sample.y);
            double shifted_i = (iq_i * nco_cos) - (iq_q * nco_sin);
            double shifted_q = (iq_i * nco_sin) + (iq_q * nco_cos);

            const double next_nco_cos = (nco_cos * nco_inc_cos) - (nco_sin * nco_inc_sin);
            nco_sin = (nco_cos * nco_inc_sin) + (nco_sin * nco_inc_cos);
            nco_cos = next_nco_cos;
            nco_renorm_counter++;
            if (nco_renorm_counter >= 256) {
                const double inv_nco_mag = 1.0 / std::sqrt((nco_cos * nco_cos) + (nco_sin * nco_sin));
                nco_cos *= inv_nco_mag;
                nco_sin *= inv_nco_mag;
                nco_renorm_counter = 0;
            }

            shifted_i = static_cast<double>(i_lpf.process(static_cast<float>(shifted_i)));
            shifted_q = static_cast<double>(q_lpf.process(static_cast<float>(shifted_q)));

            decimation_phase++;
            if (decimation_phase < decimation) {
                continue;
            }
            decimation_phase = 0;

            const double mag_sq = (shifted_i * shifted_i) + (shifted_q * shifted_q);
            if (mag_sq > 1e-18) {
                const double inv_mag = 1.0 / std::sqrt(mag_sq);
                shifted_i *= inv_mag;
                shifted_q *= inv_mag;
            }

            if (!has_previous) {
                previous_i = shifted_i;
                previous_q = shifted_q;
                has_previous = true;
                continue;
            }

            const double cross_real = (shifted_i * previous_i) + (shifted_q * previous_q);
            const double cross_imag = (shifted_q * previous_i) - (shifted_i * previous_q);
            previous_i = shifted_i;
            previous_q = shifted_q;
            const double phase_delta = std::atan2(cross_imag, cross_real);
            float value = static_cast<float>(phase_delta * inv_deviation);
            value = audio_lpf.process(value);
            if (params.deemphasis_enabled) {
                value = deemphasis.process(value);
            }

            // Preserve demod dynamics and avoid compressor-like pumping.
            scratch_mono.push_back(std::clamp(value * 0.92f, -1.0f, 1.0f));
        }

        resampler.process_block(scratch_mono, r_audio_out);
    }

    void reset() {
        i_lpf.reset();
        q_lpf.reset();
        audio_lpf.reset();
        deemphasis.reset();
        resampler.reset();
        previous_i = 0.0;
        previous_q = 0.0;
        has_previous = false;
        nco_cos = 1.0;
        nco_sin = 0.0;
        decimation_phase = 0;
    }

private:
    FMParams params;
    MultiPoleLowPass i_lpf;
    MultiPoleLowPass q_lpf;
    MultiPoleLowPass audio_lpf;
    DeemphasisFilter deemphasis;
    LinearResampler resampler;
    double previous_i = 0.0;
    double previous_q = 0.0;
    bool has_previous = false;
    double nco_phase_inc = 0.0;
    double nco_cos = 1.0;
    double nco_sin = 0.0;
    double nco_inc_cos = 1.0;
    double nco_inc_sin = 0.0;
    double if_sample_rate = 48000.0;
    double inv_deviation = 1.0;
    int32_t decimation = 1;
    int32_t decimation_phase = 0;
    std::vector<float> scratch_mono;
    bool configured = false;
};

struct WFMParams {
    double input_sample_rate_hz = 2400000.0;
    double audio_sample_rate_hz = 48000.0;
    double offset_hz = 0.0;
    double deviation_hz = 75000.0;
    double rf_lowpass_hz = 120000.0;
    double audio_lowpass_hz = 16000.0;
    bool deemphasis_enabled = true;
    double deemphasis_tau_seconds = 75e-6;
    bool stereo_enabled = true;
};

class WFMStereoDemodCore {
public:
    void configure(const WFMParams &p_params) {
        params = p_params;
        const double input_sr = std::max(1000.0, params.input_sample_rate_hz);
        const double audio_sr = std::max(8000.0, params.audio_sample_rate_hz);
        const double deviation = std::max(1000.0, params.deviation_hz);

        const double min_if_sr = params.stereo_enabled ? 228000.0 : std::max(audio_sr * 4.0, 96000.0);
        const double constrained_if = std::max(min_if_sr, params.rf_lowpass_hz * 4.0);
        decimation = std::max<int32_t>(1, static_cast<int32_t>(std::floor(input_sr / constrained_if)));
        if_sample_rate = input_sr / static_cast<double>(decimation);
        inv_deviation = if_sample_rate / (2.0 * PI * deviation);
        nco_phase_inc = -2.0 * PI * (params.offset_hz / input_sr);
        nco_inc_cos = std::cos(nco_phase_inc);
        nco_inc_sin = std::sin(nco_phase_inc);

        const double rf_cutoff = std::clamp(params.rf_lowpass_hz, 5000.0, if_sample_rate * 0.45);
        const double mono_cutoff = std::clamp(params.audio_lowpass_hz, 300.0, if_sample_rate * 0.45);
        const double stereo_cutoff = std::clamp(std::min(15000.0, mono_cutoff), 300.0, if_sample_rate * 0.45);
        const double mpx_cutoff = std::clamp(std::min(rf_cutoff * 0.95, 120000.0), 1000.0, if_sample_rate * 0.45);

        i_lpf.configure(rf_cutoff, input_sr, 4);
        q_lpf.configure(rf_cutoff, input_sr, 4);
        mpx_lpf.configure(mpx_cutoff, if_sample_rate, 2);
        mono_lpf.configure(mono_cutoff, if_sample_rate, 3);
        lpr_lpf.configure(stereo_cutoff, if_sample_rate, 4);
        lmr_lpf.configure(stereo_cutoff, if_sample_rate, 4);
        // Reject the 19 kHz pilot whistle which becomes very audible when de-emphasis is off.
        mono_pilot_notch.configure(19000.0, 10.0, if_sample_rate);
        left_pilot_notch.configure(19000.0, 10.0, if_sample_rate);
        right_pilot_notch.configure(19000.0, 10.0, if_sample_rate);
        const double post_audio_cutoff = std::clamp(std::min(stereo_cutoff, audio_sr * 0.45), 300.0, if_sample_rate * 0.45);
        post_mono_lpf.configure(post_audio_cutoff, if_sample_rate, 3);
        post_left_lpf.configure(post_audio_cutoff, if_sample_rate, 3);
        post_right_lpf.configure(post_audio_cutoff, if_sample_rate, 3);
        mono_deemphasis.configure(params.deemphasis_tau_seconds, if_sample_rate);
        left_deemphasis.configure(params.deemphasis_tau_seconds, if_sample_rate);
        right_deemphasis.configure(params.deemphasis_tau_seconds, if_sample_rate);
        mono_resampler.configure(if_sample_rate, audio_sr);
        left_resampler.configure(if_sample_rate, audio_sr);
        right_resampler.configure(if_sample_rate, audio_sr);
        stereo_phase_inc = 2.0 * PI * (38000.0 / if_sample_rate);
        stereo_inc_cos = std::cos(stereo_phase_inc);
        stereo_inc_sin = std::sin(stereo_phase_inc);
        decimation_phase = 0;
        configured = true;
    }

    void process_frame(const godot::PackedVector2Array &p_frame,
            godot::PackedFloat32Array &r_mono_out,
            godot::PackedVector2Array &r_stereo_out) {
        r_mono_out.resize(0);
        r_stereo_out.resize(0);
        if (!configured || p_frame.is_empty()) {
            return;
        }

        mono_if.clear();
        left_if.clear();
        right_if.clear();
        const size_t reserve_count = static_cast<size_t>(p_frame.size() / std::max<int32_t>(1, decimation) + 8);
        mono_if.reserve(reserve_count);
        left_if.reserve(reserve_count);
        right_if.reserve(reserve_count);

        int32_t nco_renorm_counter = 0;
        int32_t stereo_renorm_counter = 0;
        for (int32_t i = 0; i < p_frame.size(); i++) {
            const godot::Vector2 sample = p_frame[i];
            const double iq_i = static_cast<double>(sample.x);
            const double iq_q = static_cast<double>(sample.y);
            double shifted_i = (iq_i * nco_cos) - (iq_q * nco_sin);
            double shifted_q = (iq_i * nco_sin) + (iq_q * nco_cos);

            const double next_nco_cos = (nco_cos * nco_inc_cos) - (nco_sin * nco_inc_sin);
            nco_sin = (nco_cos * nco_inc_sin) + (nco_sin * nco_inc_cos);
            nco_cos = next_nco_cos;
            nco_renorm_counter++;
            if (nco_renorm_counter >= 256) {
                const double inv_nco_mag = 1.0 / std::sqrt((nco_cos * nco_cos) + (nco_sin * nco_sin));
                nco_cos *= inv_nco_mag;
                nco_sin *= inv_nco_mag;
                nco_renorm_counter = 0;
            }

            shifted_i = static_cast<double>(i_lpf.process(static_cast<float>(shifted_i)));
            shifted_q = static_cast<double>(q_lpf.process(static_cast<float>(shifted_q)));

            decimation_phase++;
            if (decimation_phase < decimation) {
                continue;
            }
            decimation_phase = 0;

            const double mag_sq = (shifted_i * shifted_i) + (shifted_q * shifted_q);
            if (mag_sq > 1e-18) {
                const double inv_mag = 1.0 / std::sqrt(mag_sq);
                shifted_i *= inv_mag;
                shifted_q *= inv_mag;
            }

            if (!has_previous) {
                previous_i = shifted_i;
                previous_q = shifted_q;
                has_previous = true;
                continue;
            }

            const double cross_real = (shifted_i * previous_i) + (shifted_q * previous_q);
            const double cross_imag = (shifted_q * previous_i) - (shifted_i * previous_q);
            previous_i = shifted_i;
            previous_q = shifted_q;
            const float mpx = mpx_lpf.process(static_cast<float>(std::atan2(cross_imag, cross_real) * inv_deviation));

            if (params.stereo_enabled) {
                float lpr = lpr_lpf.process(mpx);
                lpr = mono_pilot_notch.process(lpr);
                const float subcarrier = static_cast<float>(stereo_cos);
                const double next_stereo_cos = (stereo_cos * stereo_inc_cos) - (stereo_sin * stereo_inc_sin);
                stereo_sin = (stereo_cos * stereo_inc_sin) + (stereo_sin * stereo_inc_cos);
                stereo_cos = next_stereo_cos;
                stereo_renorm_counter++;
                if (stereo_renorm_counter >= 256) {
                    const double inv_st_mag = 1.0 / std::sqrt((stereo_cos * stereo_cos) + (stereo_sin * stereo_sin));
                    stereo_cos *= inv_st_mag;
                    stereo_sin *= inv_st_mag;
                    stereo_renorm_counter = 0;
                }

                float lmr = lmr_lpf.process(2.0f * mpx * subcarrier);
                float left = 0.5f * (lpr + lmr);
                float right = 0.5f * (lpr - lmr);

                if (params.deemphasis_enabled) {
                    left = left_deemphasis.process(left);
                    right = right_deemphasis.process(right);
                }

                left = post_left_lpf.process(left_pilot_notch.process(left));
                right = post_right_lpf.process(right_pilot_notch.process(right));

                const float mono_sample = 0.5f * (left + right);
                left_if.push_back(std::clamp(left * 0.92f, -1.0f, 1.0f));
                right_if.push_back(std::clamp(right * 0.92f, -1.0f, 1.0f));
                mono_if.push_back(std::clamp(mono_sample * 0.92f, -1.0f, 1.0f));
                continue;
            }

            float mono_sample = mono_lpf.process(mpx);
            if (params.deemphasis_enabled) {
                mono_sample = mono_deemphasis.process(mono_sample);
            }
            mono_sample = post_mono_lpf.process(mono_pilot_notch.process(mono_sample));
            mono_if.push_back(std::clamp(mono_sample * 0.92f, -1.0f, 1.0f));
        }

        if (params.stereo_enabled) {
            left_resampler.process_block(left_if, left_audio);
            right_resampler.process_block(right_if, right_audio);
            const int32_t count = std::min(left_audio.size(), right_audio.size());

            r_stereo_out.resize(count);
            r_mono_out.resize(count);
            godot::Vector2 *st_out = r_stereo_out.ptrw();
            float *mono_out = r_mono_out.ptrw();
            for (int32_t i = 0; i < count; i++) {
                const float left = left_audio[i];
                const float right = right_audio[i];
                st_out[i] = godot::Vector2(left, right);
                mono_out[i] = 0.5f * (left + right);
            }
            return;
        }

        mono_resampler.process_block(mono_if, r_mono_out);
    }

    void reset() {
        i_lpf.reset();
        q_lpf.reset();
        mpx_lpf.reset();
        mono_lpf.reset();
        lpr_lpf.reset();
        lmr_lpf.reset();
        mono_pilot_notch.reset();
        left_pilot_notch.reset();
        right_pilot_notch.reset();
        post_mono_lpf.reset();
        post_left_lpf.reset();
        post_right_lpf.reset();
        mono_deemphasis.reset();
        left_deemphasis.reset();
        right_deemphasis.reset();
        mono_resampler.reset();
        left_resampler.reset();
        right_resampler.reset();
        previous_i = 0.0;
        previous_q = 0.0;
        has_previous = false;
        nco_cos = 1.0;
        nco_sin = 0.0;
        stereo_cos = 1.0;
        stereo_sin = 0.0;
        decimation_phase = 0;
    }

private:
    WFMParams params;
    MultiPoleLowPass i_lpf;
    MultiPoleLowPass q_lpf;
    MultiPoleLowPass mpx_lpf;
    MultiPoleLowPass mono_lpf;
    MultiPoleLowPass lpr_lpf;
    MultiPoleLowPass lmr_lpf;
    BiquadNotch mono_pilot_notch;
    BiquadNotch left_pilot_notch;
    BiquadNotch right_pilot_notch;
    MultiPoleLowPass post_mono_lpf;
    MultiPoleLowPass post_left_lpf;
    MultiPoleLowPass post_right_lpf;
    DeemphasisFilter mono_deemphasis;
    DeemphasisFilter left_deemphasis;
    DeemphasisFilter right_deemphasis;
    LinearResampler mono_resampler;
    LinearResampler left_resampler;
    LinearResampler right_resampler;

    double previous_i = 0.0;
    double previous_q = 0.0;
    bool has_previous = false;
    double nco_phase_inc = 0.0;
    double nco_cos = 1.0;
    double nco_sin = 0.0;
    double stereo_phase_inc = 0.0;
    double stereo_cos = 1.0;
    double stereo_sin = 0.0;
    double nco_inc_cos = 1.0;
    double nco_inc_sin = 0.0;
    double stereo_inc_cos = 1.0;
    double stereo_inc_sin = 0.0;
    double if_sample_rate = 48000.0;
    double inv_deviation = 1.0;
    int32_t decimation = 1;
    int32_t decimation_phase = 0;
    bool configured = false;

    std::vector<float> mono_if;
    std::vector<float> left_if;
    std::vector<float> right_if;
    godot::PackedFloat32Array left_audio;
    godot::PackedFloat32Array right_audio;
};

struct SSBParams {
    enum Mode {
        MODE_USB = 0,
        MODE_LSB = 1,
        MODE_DSB = 2,
    };

    double input_sample_rate_hz = 2400000.0;
    double audio_sample_rate_hz = 48000.0;
    double offset_hz = 0.0;
    double rf_lowpass_hz = 3000.0;
    double audio_lowpass_hz = 3000.0;
    Mode mode = MODE_USB;
};

class SSBDemodCore {
public:
    void configure(const SSBParams &p_params) {
        params = p_params;
        const double input_sr = std::max(1000.0, params.input_sample_rate_hz);
        const double audio_sr = std::max(8000.0, params.audio_sample_rate_hz);
        const double min_if_sr = std::max({audio_sr * 4.0, params.rf_lowpass_hz * 8.0, 48000.0});
        decimation = std::max<int32_t>(1, static_cast<int32_t>(std::floor(input_sr / min_if_sr)));
        if_sample_rate = input_sr / static_cast<double>(decimation);
        nco_phase_inc = -2.0 * PI * (params.offset_hz / input_sr);
        nco_inc_cos = std::cos(nco_phase_inc);
        nco_inc_sin = std::sin(nco_phase_inc);

        const double rf_cutoff = std::clamp(params.rf_lowpass_hz, 300.0, if_sample_rate * 0.45);
        const double audio_cutoff = std::clamp(params.audio_lowpass_hz, 300.0, if_sample_rate * 0.45);

        i_lpf.configure(rf_cutoff, input_sr, 3);
        q_lpf.configure(rf_cutoff, input_sr, 3);
        audio_lpf.configure(audio_cutoff, if_sample_rate, 2);
        dc_block.configure(0.995);
        resampler.configure(if_sample_rate, audio_sr);
        decimation_phase = 0;
        configured = true;
    }

    void process_frame(const godot::PackedVector2Array &p_frame, godot::PackedFloat32Array &r_audio_out) {
        r_audio_out.resize(0);
        if (!configured || p_frame.is_empty()) {
            return;
        }

        scratch_mono.clear();
        scratch_mono.reserve(static_cast<size_t>(p_frame.size() / std::max<int32_t>(1, decimation) + 8));

        int32_t nco_renorm_counter = 0;
        for (int32_t i = 0; i < p_frame.size(); i++) {
            const godot::Vector2 sample = p_frame[i];
            const double iq_i = static_cast<double>(sample.x);
            const double iq_q = static_cast<double>(sample.y);
            double shifted_i = (iq_i * nco_cos) - (iq_q * nco_sin);
            double shifted_q = (iq_i * nco_sin) + (iq_q * nco_cos);

            const double next_nco_cos = (nco_cos * nco_inc_cos) - (nco_sin * nco_inc_sin);
            nco_sin = (nco_cos * nco_inc_sin) + (nco_sin * nco_inc_cos);
            nco_cos = next_nco_cos;
            nco_renorm_counter++;
            if (nco_renorm_counter >= 256) {
                const double inv_nco_mag = 1.0 / std::sqrt((nco_cos * nco_cos) + (nco_sin * nco_sin));
                nco_cos *= inv_nco_mag;
                nco_sin *= inv_nco_mag;
                nco_renorm_counter = 0;
            }

            shifted_i = static_cast<double>(i_lpf.process(static_cast<float>(shifted_i)));
            shifted_q = static_cast<double>(q_lpf.process(static_cast<float>(shifted_q)));

            decimation_phase++;
            if (decimation_phase < decimation) {
                continue;
            }
            decimation_phase = 0;

            float audio = 0.0f;
            switch (params.mode) {
                case SSBParams::MODE_USB:
                    audio = static_cast<float>(shifted_i + shifted_q);
                    break;
                case SSBParams::MODE_LSB:
                    audio = static_cast<float>(shifted_i - shifted_q);
                    break;
                case SSBParams::MODE_DSB:
                default:
                    audio = static_cast<float>(shifted_i);
                    break;
            }

            audio = dc_block.process(audio);
            audio = audio_lpf.process(audio * 0.7f);
            scratch_mono.push_back(std::clamp(audio, -1.0f, 1.0f));
        }

        resampler.process_block(scratch_mono, r_audio_out);
    }

    void reset() {
        i_lpf.reset();
        q_lpf.reset();
        audio_lpf.reset();
        dc_block.reset();
        resampler.reset();
        nco_cos = 1.0;
        nco_sin = 0.0;
        decimation_phase = 0;
    }

private:
    SSBParams params;
    MultiPoleLowPass i_lpf;
    MultiPoleLowPass q_lpf;
    MultiPoleLowPass audio_lpf;
    DCBlocker dc_block;
    LinearResampler resampler;
    double nco_phase_inc = 0.0;
    double nco_cos = 1.0;
    double nco_sin = 0.0;
    double nco_inc_cos = 1.0;
    double nco_inc_sin = 0.0;
    double if_sample_rate = 48000.0;
    int32_t decimation = 1;
    int32_t decimation_phase = 0;
    std::vector<float> scratch_mono;
    bool configured = false;
};

struct CWParams {
    double input_sample_rate_hz = 2400000.0;
    double audio_sample_rate_hz = 48000.0;
    double offset_hz = 0.0;
    double bandwidth_hz = 1000.0;
    double tone_hz = 700.0;
};

class CWDemodCore {
public:
    void configure(const CWParams &p_params) {
        params = p_params;
        const double input_sr = std::max(1000.0, params.input_sample_rate_hz);
        const double audio_sr = std::max(8000.0, params.audio_sample_rate_hz);
        const double min_if_sr = std::max({audio_sr * 4.0, params.bandwidth_hz * 12.0, 24000.0});
        decimation = std::max<int32_t>(1, static_cast<int32_t>(std::floor(input_sr / min_if_sr)));
        if_sample_rate = input_sr / static_cast<double>(decimation);

        nco_phase_inc = -2.0 * PI * (params.offset_hz / input_sr);
        nco_inc_cos = std::cos(nco_phase_inc);
        nco_inc_sin = std::sin(nco_phase_inc);

        bfo_phase_inc = 2.0 * PI * (params.tone_hz / if_sample_rate);
        bfo_inc_cos = std::cos(bfo_phase_inc);
        bfo_inc_sin = std::sin(bfo_phase_inc);

        const double rf_cutoff = std::clamp(params.bandwidth_hz * 0.8, 100.0, if_sample_rate * 0.45);
        const double audio_cutoff = std::clamp(params.bandwidth_hz * 0.8, 100.0, if_sample_rate * 0.45);

        i_lpf.configure(rf_cutoff, input_sr, 3);
        q_lpf.configure(rf_cutoff, input_sr, 3);
        audio_lpf.configure(audio_cutoff, if_sample_rate, 2);
        dc_block.configure(0.995);
        resampler.configure(if_sample_rate, audio_sr);
        decimation_phase = 0;
        configured = true;
    }

    void process_frame(const godot::PackedVector2Array &p_frame, godot::PackedFloat32Array &r_audio_out) {
        r_audio_out.resize(0);
        if (!configured || p_frame.is_empty()) {
            return;
        }

        scratch_mono.clear();
        scratch_mono.reserve(static_cast<size_t>(p_frame.size() / std::max<int32_t>(1, decimation) + 8));

        int32_t nco_renorm_counter = 0;
        int32_t bfo_renorm_counter = 0;
        for (int32_t i = 0; i < p_frame.size(); i++) {
            const godot::Vector2 sample = p_frame[i];
            const double iq_i = static_cast<double>(sample.x);
            const double iq_q = static_cast<double>(sample.y);
            double shifted_i = (iq_i * nco_cos) - (iq_q * nco_sin);
            double shifted_q = (iq_i * nco_sin) + (iq_q * nco_cos);

            const double next_nco_cos = (nco_cos * nco_inc_cos) - (nco_sin * nco_inc_sin);
            nco_sin = (nco_cos * nco_inc_sin) + (nco_sin * nco_inc_cos);
            nco_cos = next_nco_cos;
            nco_renorm_counter++;
            if (nco_renorm_counter >= 256) {
                const double inv_nco_mag = 1.0 / std::sqrt((nco_cos * nco_cos) + (nco_sin * nco_sin));
                nco_cos *= inv_nco_mag;
                nco_sin *= inv_nco_mag;
                nco_renorm_counter = 0;
            }

            shifted_i = static_cast<double>(i_lpf.process(static_cast<float>(shifted_i)));
            shifted_q = static_cast<double>(q_lpf.process(static_cast<float>(shifted_q)));

            decimation_phase++;
            if (decimation_phase < decimation) {
                continue;
            }
            decimation_phase = 0;

            const float audio = static_cast<float>((shifted_i * bfo_cos) - (shifted_q * bfo_sin));
            const double next_bfo_cos = (bfo_cos * bfo_inc_cos) - (bfo_sin * bfo_inc_sin);
            bfo_sin = (bfo_cos * bfo_inc_sin) + (bfo_sin * bfo_inc_cos);
            bfo_cos = next_bfo_cos;
            bfo_renorm_counter++;
            if (bfo_renorm_counter >= 256) {
                const double inv_bfo_mag = 1.0 / std::sqrt((bfo_cos * bfo_cos) + (bfo_sin * bfo_sin));
                bfo_cos *= inv_bfo_mag;
                bfo_sin *= inv_bfo_mag;
                bfo_renorm_counter = 0;
            }

            float filtered = dc_block.process(audio);
            filtered = audio_lpf.process(filtered * 0.8f);
            scratch_mono.push_back(std::clamp(filtered, -1.0f, 1.0f));
        }

        resampler.process_block(scratch_mono, r_audio_out);
    }

    void reset() {
        i_lpf.reset();
        q_lpf.reset();
        audio_lpf.reset();
        dc_block.reset();
        resampler.reset();
        nco_cos = 1.0;
        nco_sin = 0.0;
        bfo_cos = 1.0;
        bfo_sin = 0.0;
        decimation_phase = 0;
    }

private:
    CWParams params;
    MultiPoleLowPass i_lpf;
    MultiPoleLowPass q_lpf;
    MultiPoleLowPass audio_lpf;
    DCBlocker dc_block;
    LinearResampler resampler;
    double nco_phase_inc = 0.0;
    double nco_cos = 1.0;
    double nco_sin = 0.0;
    double nco_inc_cos = 1.0;
    double nco_inc_sin = 0.0;
    double bfo_phase_inc = 0.0;
    double bfo_cos = 1.0;
    double bfo_sin = 0.0;
    double bfo_inc_cos = 1.0;
    double bfo_inc_sin = 0.0;
    double if_sample_rate = 48000.0;
    int32_t decimation = 1;
    int32_t decimation_phase = 0;
    std::vector<float> scratch_mono;
    bool configured = false;
};

struct AMParams {
    double input_sample_rate_hz = 2400000.0;
    double audio_sample_rate_hz = 48000.0;
    double offset_hz = 0.0;
    double rf_lowpass_hz = 10000.0;
    double audio_lowpass_hz = 6000.0;
    bool agc_enabled = true;
    double agc_target_level = 0.35;
    double agc_attack_seconds = 0.008;
    double agc_release_seconds = 0.250;
};

class AMDemodCore {
public:
    void configure(const AMParams &p_params) {
        params = p_params;
        const double input_sr = std::max(1000.0, params.input_sample_rate_hz);
        const double audio_sr = std::max(8000.0, params.audio_sample_rate_hz);
        const double min_if_sr = std::max({audio_sr * 4.0, params.rf_lowpass_hz * 8.0, 48000.0});
        decimation = std::max<int32_t>(1, static_cast<int32_t>(std::floor(input_sr / min_if_sr)));
        if_sample_rate = input_sr / static_cast<double>(decimation);
        nco_phase_inc = -2.0 * PI * (params.offset_hz / input_sr);
        nco_inc_cos = std::cos(nco_phase_inc);
        nco_inc_sin = std::sin(nco_phase_inc);

        const double rf_cutoff = std::clamp(params.rf_lowpass_hz, 500.0, if_sample_rate * 0.45);
        const double audio_cutoff = std::clamp(params.audio_lowpass_hz, 300.0, if_sample_rate * 0.45);

        i_lpf.configure(rf_cutoff, input_sr, 3);
        q_lpf.configure(rf_cutoff, input_sr, 3);
        dc_block.configure(0.995);
        audio_lpf.configure(audio_cutoff, if_sample_rate, 2);
        resampler.configure(if_sample_rate, audio_sr);
        const double agc_rate = std::max(1000.0, if_sample_rate);
        const double attack_tau = std::max(1e-4, params.agc_attack_seconds);
        const double release_tau = std::max(1e-4, params.agc_release_seconds);
        agc_attack_coeff = std::clamp(1.0 - std::exp(-1.0 / (agc_rate * attack_tau)), 1e-5, 1.0);
        agc_release_coeff = std::clamp(1.0 - std::exp(-1.0 / (agc_rate * release_tau)), 1e-5, 1.0);
        agc_envelope = 0.0;
        agc_gain = 1.0;
        decimation_phase = 0;
        configured = true;
    }

    void process_frame(const godot::PackedVector2Array &p_frame, godot::PackedFloat32Array &r_audio_out) {
        r_audio_out.resize(0);
        if (!configured || p_frame.is_empty()) {
            return;
        }

        scratch_mono.clear();
        scratch_mono.reserve(static_cast<size_t>(p_frame.size() / std::max<int32_t>(1, decimation) + 8));

        int32_t nco_renorm_counter = 0;
        for (int32_t i = 0; i < p_frame.size(); i++) {
            const godot::Vector2 sample = p_frame[i];
            const double iq_i = static_cast<double>(sample.x);
            const double iq_q = static_cast<double>(sample.y);
            double shifted_i = (iq_i * nco_cos) - (iq_q * nco_sin);
            double shifted_q = (iq_i * nco_sin) + (iq_q * nco_cos);

            const double next_nco_cos = (nco_cos * nco_inc_cos) - (nco_sin * nco_inc_sin);
            nco_sin = (nco_cos * nco_inc_sin) + (nco_sin * nco_inc_cos);
            nco_cos = next_nco_cos;
            nco_renorm_counter++;
            if (nco_renorm_counter >= 256) {
                const double inv_nco_mag = 1.0 / std::sqrt((nco_cos * nco_cos) + (nco_sin * nco_sin));
                nco_cos *= inv_nco_mag;
                nco_sin *= inv_nco_mag;
                nco_renorm_counter = 0;
            }

            shifted_i = static_cast<double>(i_lpf.process(static_cast<float>(shifted_i)));
            shifted_q = static_cast<double>(q_lpf.process(static_cast<float>(shifted_q)));

            decimation_phase++;
            if (decimation_phase < decimation) {
                continue;
            }
            decimation_phase = 0;

            const float magnitude = static_cast<float>(std::sqrt((shifted_i * shifted_i) + (shifted_q * shifted_q)));
            float value = dc_block.process(magnitude);
            value = audio_lpf.process(value * 2.2f);
            if (params.agc_enabled) {
                const double abs_value = std::abs(static_cast<double>(value));
                if (abs_value > agc_envelope) {
                    agc_envelope += agc_attack_coeff * (abs_value - agc_envelope);
                } else {
                    agc_envelope += agc_release_coeff * (abs_value - agc_envelope);
                }
                const double safe_envelope = std::max(agc_envelope, 1e-5);
                const double target = std::max(0.02, params.agc_target_level);
                const double desired_gain = std::clamp(target / safe_envelope, 0.2, 10.0);
                const double gain_coeff = desired_gain < agc_gain ? agc_attack_coeff : agc_release_coeff;
                agc_gain += gain_coeff * (desired_gain - agc_gain);
                value = static_cast<float>(value * agc_gain);
            }
            scratch_mono.push_back(std::clamp(value, -1.0f, 1.0f));
        }

        resampler.process_block(scratch_mono, r_audio_out);
    }

    void reset() {
        i_lpf.reset();
        q_lpf.reset();
        dc_block.reset();
        audio_lpf.reset();
        resampler.reset();
        nco_cos = 1.0;
        nco_sin = 0.0;
        decimation_phase = 0;
        agc_envelope = 0.0;
        agc_gain = 1.0;
    }

private:
    AMParams params;
    MultiPoleLowPass i_lpf;
    MultiPoleLowPass q_lpf;
    DCBlocker dc_block;
    MultiPoleLowPass audio_lpf;
    LinearResampler resampler;
    double nco_phase_inc = 0.0;
    double nco_cos = 1.0;
    double nco_sin = 0.0;
    double nco_inc_cos = 1.0;
    double nco_inc_sin = 0.0;
    double if_sample_rate = 48000.0;
    int32_t decimation = 1;
    int32_t decimation_phase = 0;
    double agc_attack_coeff = 0.01;
    double agc_release_coeff = 0.001;
    double agc_envelope = 0.0;
    double agc_gain = 1.0;
    std::vector<float> scratch_mono;
    bool configured = false;
};

} // namespace esdr_demod

#endif // ESDR_DEMOD_DSP_UTILS_H
