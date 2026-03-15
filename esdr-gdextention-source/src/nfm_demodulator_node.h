#ifndef ESDR_NFM_DEMODULATOR_NODE_H
#define ESDR_NFM_DEMODULATOR_NODE_H

#include "sdr_node.h"

#include <godot_cpp/classes/spin_box.hpp>

class NFMDemodulator : public SDR {
    GDCLASS(NFMDemodulator, SDR)

private:
    double offset_hz = 0.0;
    double bandwidth_hz = 8000.0;

    godot::SpinBox *offset_spin = nullptr;
    godot::SpinBox *bandwidth_spin = nullptr;

    void ensure_ui();
    void cache_ui_refs();
    void bind_ui();
    void configure_slots();

    void _on_offset_changed(double p_value);
    void _on_bandwidth_changed(double p_value);

protected:
    static void _bind_methods();
    void _notification(int32_t p_what);

public:
    void set_offset_hz(double p_value);
    double get_offset_hz() const;

    void set_bandwidth_hz(double p_value);
    double get_bandwidth_hz() const;
};

#endif // ESDR_NFM_DEMODULATOR_NODE_H
