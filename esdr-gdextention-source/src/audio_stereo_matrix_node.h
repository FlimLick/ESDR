#ifndef ESDR_AUDIO_STEREO_MATRIX_NODE_H
#define ESDR_AUDIO_STEREO_MATRIX_NODE_H

#include "sdr_node.h"

#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cstdint>
#include <mutex>

class AudioStereoMatrix : public SDR {
    GDCLASS(AudioStereoMatrix, SDR)

private:
    godot::SpinBox *input_gain_spin = nullptr;
    godot::SpinBox *output_gain_spin = nullptr;
    godot::SpinBox *ll_spin = nullptr;
    godot::SpinBox *lr_spin = nullptr;
    godot::SpinBox *rl_spin = nullptr;
    godot::SpinBox *rr_spin = nullptr;
    godot::Label *status_label = nullptr;

    float input_gain = 1.0f;
    float output_gain = 1.0f;
    float matrix_ll = 1.0f;
    float matrix_lr = 0.0f;
    float matrix_rl = 0.0f;
    float matrix_rr = 1.0f;

    std::mutex input_mutex;
    godot::PackedFloat32Array pending_mono_frame;
    godot::PackedVector2Array pending_stereo_frame;
    bool has_pending_mono = false;
    bool has_pending_stereo = false;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();
    void update_status();

    void on_ready();
    void on_exit_tree();
    void process_tick();

    void set_input_gain(float p_value);
    void set_output_gain(float p_value);
    void set_matrix_ll(float p_value);
    void set_matrix_lr(float p_value);
    void set_matrix_rl(float p_value);
    void set_matrix_rr(float p_value);

    void _on_input_gain_changed(double p_value);
    void _on_output_gain_changed(double p_value);
    void _on_matrix_ll_changed(double p_value);
    void _on_matrix_lr_changed(double p_value);
    void _on_matrix_rl_changed(double p_value);
    void _on_matrix_rr_changed(double p_value);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    void push_audio_frame(const godot::PackedFloat32Array &p_frame);
    void push_audio_stereo_frame(const godot::PackedVector2Array &p_frame);
    void push_audio_frame_from_source(const godot::PackedFloat32Array &p_frame, const godot::StringName &p_source_id);
    void push_audio_stereo_frame_from_source(const godot::PackedVector2Array &p_frame, const godot::StringName &p_source_id);
    bool get_stereo_enabled() const;

    godot::Variant get_port_value(int64_t p_port) const;
    void set_port_value(int64_t p_port, const godot::Variant &p_value);
};

#endif // ESDR_AUDIO_STEREO_MATRIX_NODE_H
