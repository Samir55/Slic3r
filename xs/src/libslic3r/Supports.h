#ifndef SLIC3R_SUPPORTS_H
#define SLIC3R_SUPPORTS_H

#include "libslic3r.h"
#include "PrintConfig.hpp"
#include "Flow.hpp"

/*

 */

#define MARGIN_STEP MARGIN
#define PILLAR_SIZE 2.5
#define PILLAR_SPACING 10

class Supports
{
public:
    PrintConfig* print_config; ///<
    PrintObjectConfig* print_object_config; ///<
    Flow flow; ///<
    Flow first_layer_flow; ///<
    Flow interface_flow; ///<

    Supports(PrintConfig *print_config,
             PrintObjectConfig *print_object_config,
             const Flow &flow,
             const Flow &first_layer_flow,
             const Flow &interface_flow)
        : print_config(print_config),
          print_object_config(print_object_config),
          flow(flow),
          first_layer_flow(first_layer_flow),
          interface_flow(interface_flow)
    {}

    void generate() {}

    void contact_area() {}

    void object_top() {}

    void support_layers_z() {}

    void generate_interface_layers() {}

    void generate_bottom_interface_layers() {}

    void generate_base_layers() {}

    void clip_with_object() {}

    void generate_toolpaths() {}

    void generate_pillars_shape() {}

    void clip_with_shape() {}

    void overlapping_layers(int layer_idx) {}

    float contact_distance(float layer_height, float nozzle_diameter) {
        float extra = print_object_config->support_material_contact_distance.value;
        if (extra == 0) {
            return layer_height;
        } else {
            return nozzle_diameter + extra;
        }
    }

};


#endif //SLIC3R_SUPPORTS_H
