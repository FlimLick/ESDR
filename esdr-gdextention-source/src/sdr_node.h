#ifndef ESDR_SDR_NODE_H
#define ESDR_SDR_NODE_H

#include <godot_cpp/classes/graph_node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/variant.hpp>

class SDR : public godot::GraphNode {
    GDCLASS(SDR, godot::GraphNode)

public:
    enum SignalKind {
        SIGNAL_INT = 1,
        SIGNAL_BOOL = 2,
        SIGNAL_FLOAT = 3,
        SIGNAL_BASEBAND = 8,
        SIGNAL_AUDIO = 9,
    };

    static godot::Color signal_color(int32_t p_kind);

protected:
    static void _bind_methods();
};

VARIANT_ENUM_CAST(SDR::SignalKind)

#endif // ESDR_SDR_NODE_H
