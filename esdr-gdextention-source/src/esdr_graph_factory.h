#ifndef ESDR_GRAPH_FACTORY_H
#define ESDR_GRAPH_FACTORY_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

class ESDRGraphFactory : public godot::RefCounted {
    GDCLASS(ESDRGraphFactory, godot::RefCounted)

protected:
    static void _bind_methods();

public:
    static godot::Object *create_graph_node(const godot::String &p_node_type);
    static godot::Object *create_rtl_sdr_source();
    static godot::Object *create_nfm_demodulator();
    static godot::Object *create_wfm_demodulator();
    static godot::Object *create_am_demodulator();
    static godot::Object *create_usb_demodulator();
    static godot::Object *create_lsb_demodulator();
    static godot::Object *create_dsb_demodulator();
    static godot::Object *create_cw_demodulator();
    static godot::Object *create_audio_sink();
    static godot::Object *create_audio_generator_source();
    static godot::Object *create_audio_mixer();
    static godot::Object *create_audio_stereo_matrix();
    static godot::Object *create_baseband_replay();
    static godot::Object *create_audio_replay();
    static godot::Object *create_math_operator();
    static godot::Object *create_value_type_converter();
};

#endif // ESDR_GRAPH_FACTORY_H
