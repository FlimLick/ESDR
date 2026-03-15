#include "esdr_graph_factory.h"

#include "nfm_demodulator_node.h"
#include "rtl_sdr_source_node.h"

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
}

Object *ESDRGraphFactory::create_graph_node(const String &p_node_type) {
    const String normalized = p_node_type.strip_edges().to_lower();

    if (normalized == "rtlsdrsource" || normalized == "rtl_sdr_source" || normalized == "rtl-sdr source" || normalized == "rtl") {
        return create_rtl_sdr_source();
    }

    if (normalized == "nfmdemodulator" || normalized == "nfm_demodulator" || normalized == "nfm demodulator" || normalized == "nfm") {
        return create_nfm_demodulator();
    }

    return nullptr;
}

Object *ESDRGraphFactory::create_rtl_sdr_source() {
    return memnew(RTLSDRSource);
}

Object *ESDRGraphFactory::create_nfm_demodulator() {
    return memnew(NFMDemodulator);
}
