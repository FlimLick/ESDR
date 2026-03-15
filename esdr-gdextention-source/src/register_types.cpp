#include "register_types.h"

#include "esdr_graph_factory.h"
#include "nfm_demodulator_node.h"
#include "rtl_sdr_backend.h"
#include "rtl_sdr_source_node.h"
#include "sdr_node.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_esdr_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    ClassDB::register_abstract_class<SDR>();
    ClassDB::register_internal_class<RTLSDRSource>();
    ClassDB::register_internal_class<NFMDemodulator>();
    ClassDB::register_class<RTLSDRBackend>();
    ClassDB::register_class<ESDRGraphFactory>();
}

void uninitialize_esdr_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {
GDExtensionBool GDE_EXPORT esdr_library_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        const GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization) {
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_esdr_module);
    init_obj.register_terminator(uninitialize_esdr_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}
