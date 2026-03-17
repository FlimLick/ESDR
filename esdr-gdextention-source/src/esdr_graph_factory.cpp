#include "esdr_graph_factory.h"

#include "audio_sink_node.h"
#include "audio_generator_source_node.h"
#include "audio_mixer_node.h"
#include "audio_stereo_matrix_node.h"
#include "audio_replay_node.h"
#include "baseband_replay_node.h"
#include "cw_demodulator_node.h"
#include "math_operator_node.h"
#include "am_demodulator_node.h"
#include "nfm_demodulator_node.h"
#include "rtl_sdr_source_node.h"
#include "ssb_demodulator_node.h"
#include "value_type_converter_node.h"
#include "wfm_demodulator_node.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

using namespace godot;

void ESDRGraphFactory::_bind_methods() {
    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_graph_node", "node_type"),
            &ESDRGraphFactory::create_graph_node);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_rtl_sdr_source"),
            &ESDRGraphFactory::create_rtl_sdr_source);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_nfm_demodulator"),
            &ESDRGraphFactory::create_nfm_demodulator);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_wfm_demodulator"),
            &ESDRGraphFactory::create_wfm_demodulator);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_am_demodulator"),
            &ESDRGraphFactory::create_am_demodulator);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_usb_demodulator"),
            &ESDRGraphFactory::create_usb_demodulator);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_lsb_demodulator"),
            &ESDRGraphFactory::create_lsb_demodulator);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_dsb_demodulator"),
            &ESDRGraphFactory::create_dsb_demodulator);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_cw_demodulator"),
            &ESDRGraphFactory::create_cw_demodulator);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_audio_sink"),
            &ESDRGraphFactory::create_audio_sink);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_audio_generator_source"),
            &ESDRGraphFactory::create_audio_generator_source);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_audio_mixer"),
            &ESDRGraphFactory::create_audio_mixer);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_audio_stereo_matrix"),
            &ESDRGraphFactory::create_audio_stereo_matrix);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_baseband_replay"),
            &ESDRGraphFactory::create_baseband_replay);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_audio_replay"),
            &ESDRGraphFactory::create_audio_replay);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_math_operator"),
            &ESDRGraphFactory::create_math_operator);

    ClassDB::bind_static_method("ESDRGraphFactory",
            D_METHOD("create_value_type_converter"),
            &ESDRGraphFactory::create_value_type_converter);
}

Object *ESDRGraphFactory::create_graph_node(const String &p_node_type) {
    const String normalized = p_node_type.strip_edges().to_lower();

    if (normalized == "rtlsdrsource" || normalized == "rtl_sdr_source" || normalized == "rtl-sdr source" || normalized == "rtl") {
        return create_rtl_sdr_source();
    }

    if (normalized == "nfmdemodulator" || normalized == "nfm_demodulator" || normalized == "nfm demodulator" || normalized == "nfm") {
        return create_nfm_demodulator();
    }

    if (normalized == "wfmdemodulator" || normalized == "wfm_demodulator" || normalized == "wfm demodulator" || normalized == "wfm") {
        return create_wfm_demodulator();
    }

    if (normalized == "amdemodulator" || normalized == "am_demodulator" || normalized == "am demodulator" || normalized == "am") {
        return create_am_demodulator();
    }

    if (normalized == "usbdemodulator" || normalized == "usb_demodulator" || normalized == "usb demodulator" || normalized == "usb") {
        return create_usb_demodulator();
    }

    if (normalized == "lsbdemodulator" || normalized == "lsb_demodulator" || normalized == "lsb demodulator" || normalized == "lsb" || normalized == "lsv") {
        return create_lsb_demodulator();
    }

    if (normalized == "dsbdemodulator" || normalized == "dsb_demodulator" || normalized == "dsb demodulator" || normalized == "dsb" || normalized == "dsv") {
        return create_dsb_demodulator();
    }

    if (normalized == "cwdemodulator" || normalized == "cw_demodulator" || normalized == "cw demodulator" || normalized == "cw") {
        return create_cw_demodulator();
    }

    if (normalized == "audiosink" || normalized == "audio_sink" || normalized == "audio sink" || normalized == "sink") {
        return create_audio_sink();
    }

    if (normalized == "audiogenerator" || normalized == "audio_generator" || normalized == "audio generator" || normalized == "tone") {
        return create_audio_generator_source();
    }

    if (normalized == "audiomixer" || normalized == "audio_mixer" || normalized == "audio mixer" || normalized == "mix") {
        return create_audio_mixer();
    }

    if (normalized == "audiostereomatrix" || normalized == "audio_stereo_matrix" || normalized == "audio stereo matrix" || normalized == "stereo matrix") {
        return create_audio_stereo_matrix();
    }

    if (normalized == "basebandreplay" || normalized == "baseband_replay" || normalized == "baseband replay" || normalized == "iq replay") {
        return create_baseband_replay();
    }

    if (normalized == "audioreplay" || normalized == "audio_replay" || normalized == "audio replay") {
        return create_audio_replay();
    }

    if (normalized == "math" || normalized == "mathoperator" || normalized == "math_operator" || normalized == "math operator") {
        return create_math_operator();
    }

    if (normalized == "valueconverter" || normalized == "value_type_converter" || normalized == "value converter" || normalized == "convert") {
        return create_value_type_converter();
    }

    return nullptr;
}

Object *ESDRGraphFactory::create_rtl_sdr_source() {
    return memnew(RTLSDRSource);
}

Object *ESDRGraphFactory::create_nfm_demodulator() {
    return memnew(NFMDemodulator);
}

Object *ESDRGraphFactory::create_wfm_demodulator() {
    return memnew(WFMDemodulator);
}

Object *ESDRGraphFactory::create_am_demodulator() {
    return memnew(AMDemodulator);
}

Object *ESDRGraphFactory::create_usb_demodulator() {
    SSBDemodulator *node = memnew(SSBDemodulator);
    node->set_demod_mode(SSBDemodulator::DEMOD_USB);
    return node;
}

Object *ESDRGraphFactory::create_lsb_demodulator() {
    SSBDemodulator *node = memnew(SSBDemodulator);
    node->set_demod_mode(SSBDemodulator::DEMOD_LSB);
    return node;
}

Object *ESDRGraphFactory::create_dsb_demodulator() {
    SSBDemodulator *node = memnew(SSBDemodulator);
    node->set_demod_mode(SSBDemodulator::DEMOD_DSB);
    return node;
}

Object *ESDRGraphFactory::create_cw_demodulator() {
    return memnew(CWDemodulator);
}

Object *ESDRGraphFactory::create_audio_sink() {
    return memnew(AudioSink);
}

Object *ESDRGraphFactory::create_audio_generator_source() {
    return memnew(AudioGeneratorSource);
}

Object *ESDRGraphFactory::create_audio_mixer() {
    return memnew(AudioMixer);
}

Object *ESDRGraphFactory::create_audio_stereo_matrix() {
    return memnew(AudioStereoMatrix);
}

Object *ESDRGraphFactory::create_baseband_replay() {
    return memnew(BasebandReplay);
}

Object *ESDRGraphFactory::create_audio_replay() {
    return memnew(AudioReplay);
}

Object *ESDRGraphFactory::create_math_operator() {
    return memnew(MathOperator);
}

Object *ESDRGraphFactory::create_value_type_converter() {
    return memnew(ValueTypeConverter);
}
