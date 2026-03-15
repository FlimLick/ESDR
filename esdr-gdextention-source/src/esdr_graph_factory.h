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
};

#endif // ESDR_GRAPH_FACTORY_H
