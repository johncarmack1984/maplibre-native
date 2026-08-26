// Generated code, do not modify this file!
#pragma once
#include <mln/shaders/shader_source.hpp>

namespace mln {
namespace shaders {

template <>
struct ShaderSource<BuiltIn::CollisionBoxShader, gfx::Backend::Type::OpenGL> {
    static constexpr const char* name = "CollisionBoxShader";
    static constexpr const char* vertex = R"(layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_placed;
layout (location = 2) in vec3 a_measured_box;

layout (std140) uniform CollisionDrawableUBO {
    highp mat4 u_matrix;
};

layout (std140) uniform CollisionTilePropsUBO {
    highp vec2 u_pixel_extrude_scale;
    highp float drawable_pad1;
    highp float drawable_pad2;
};

out float v_placed;
out float v_notUsed;

void main() {
    // The collision index already measured this corner in viewport pixels; draw it verbatim.
    gl_Position = u_matrix * vec4(a_pos, 0.0, 1.0);
    gl_Position.xy += a_measured_box.xy * u_pixel_extrude_scale * 2.0 * vec2(1.0, -1.0) * gl_Position.w;

    v_placed = a_placed.x;
    v_notUsed = a_placed.y;
}
)";
    static constexpr const char* fragment = R"(in float v_placed;
in float v_notUsed;

void main() {

    float alpha = 0.5;

    // Red = collision, hide label
    fragColor = vec4(1.0, 0.0, 0.0, 1.0) * alpha;

    // Blue = no collision, label is showing
    if (v_placed > 0.5) {
        fragColor = vec4(0.0, 0.0, 1.0, 0.5) * alpha;
    }

    if (v_notUsed > 0.5) {
        // This box not used, fade it out
        fragColor *= .1;
    }
}
)";
};

} // namespace shaders
} // namespace mln
