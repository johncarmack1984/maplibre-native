#pragma once

#include <mln/shaders/shader_source.hpp>
#include <mln/shaders/vulkan/shader_program.hpp>

namespace mln {
namespace shaders {

constexpr auto collisionShaderPrelude = R"(

#define idCollisionDrawableUBO      drawableUBOStartId
#define idCollisionTilePropsUBO     drawableUBOStartId + 1

)";

template <>
struct ShaderSource<BuiltIn::CollisionBoxShader, gfx::Backend::Type::Vulkan> {
    static constexpr const char* name = "CollisionBoxShader";

    static const std::array<AttributeInfo, 3> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 0> textures;

    static constexpr auto prelude = collisionShaderPrelude;
    static constexpr auto vertex = R"(

layout(location = 0) in ivec2 in_position;
layout(location = 1) in uvec2 in_placed;
layout(location = 2) in vec3 in_measured_box;

layout(set = DRAWABLE_UBO_SET_INDEX, binding = idCollisionDrawableUBO) uniform CollisionDrawableUBO {
    mat4 matrix;
} drawable;

layout(set = DRAWABLE_UBO_SET_INDEX, binding = idCollisionTilePropsUBO) uniform CollisionTilePropsUBO {
    vec2 pixel_extrude_scale;
    float pad1;
    float pad2;
} tileProps;

layout(location = 0) out float frag_placed;
layout(location = 1) out float frag_notUsed;

void main() {

    // The collision index already measured this corner in viewport pixels; draw it verbatim.
    gl_Position = drawable.matrix * vec4(in_position, 0.0, 1.0);
    gl_Position.xy += in_measured_box.xy * tileProps.pixel_extrude_scale * 2.0 * vec2(1.0, -1.0) * gl_Position.w;
    applySurfaceTransform();

    frag_placed = in_placed.x;
    frag_notUsed = in_placed.y;
}
)";

    static constexpr auto fragment = R"(

layout(location = 0) in float frag_placed;
layout(location = 1) in float frag_notUsed;

layout(location = 0) out vec4 out_color;

void main() {

    float alpha = 0.5;

    // Red = collision, hide label
    vec4 color = vec4(1.0, 0.0, 0.0, 1.0) * alpha;

    // Blue = no collision, label is showing
    if (frag_placed > 0.5) {
        color = vec4(0.0, 0.0, 1.0, 0.5) * alpha;
    }

    if (frag_notUsed > 0.5) {
        // This box not used, fade it out
        color *= 0.1;
    }

    out_color = color;
}
)";
};

template <>
struct ShaderSource<BuiltIn::CollisionCircleShader, gfx::Backend::Type::Vulkan> {
    static constexpr const char* name = "CollisionCircleShader";

    static const std::array<AttributeInfo, 4> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 0> textures;

    static constexpr auto prelude = collisionShaderPrelude;
    static constexpr auto vertex = R"(

layout(location = 0) in ivec2 in_position;
layout(location = 1) in ivec2 in_extrude;
layout(location = 2) in uvec2 in_placed;
layout(location = 3) in vec3 in_measured_box;

layout(set = DRAWABLE_UBO_SET_INDEX, binding = idCollisionDrawableUBO) uniform CollisionDrawableUBO {
    mat4 matrix;
} drawable;

layout(set = DRAWABLE_UBO_SET_INDEX, binding = idCollisionTilePropsUBO) uniform CollisionTilePropsUBO {
    vec2 pixel_extrude_scale;
    float pad1;
    float pad2;
} tileProps;

layout(location = 0) out float frag_placed;
layout(location = 1) out float frag_radius;
layout(location = 2) out vec2 frag_extrude;

void main() {

    // The sign of the extrusion picks the quad corner; the radius the collision index measured
    // for this circle sizes it, in viewport pixels.
    vec2 quadVertexOffset = vec2(in_extrude.x < 0 ? -1.0 : 1.0, in_extrude.y < 0 ? -1.0 : 1.0);

    float padding_factor = 1.2; // Pad the vertices slightly to make room for anti-alias blur
    vec2 quadVertexExtent = quadVertexOffset * in_measured_box.z * padding_factor;

    gl_Position = drawable.matrix * vec4(in_position, 0.0, 1.0);
    gl_Position.xy += quadVertexExtent * tileProps.pixel_extrude_scale * 2.0 * gl_Position.w;
    applySurfaceTransform();

    frag_placed = in_placed.x;
    frag_radius = in_measured_box.z;
    frag_extrude = quadVertexExtent;
}
)";

    static constexpr auto fragment = R"(

layout(location = 0) in float frag_placed;
layout(location = 1) in float frag_radius;
layout(location = 2) in vec2 frag_extrude;

layout(location = 0) out vec4 out_color;

void main() {

    float alpha = 0.5;
    float stroke_radius = 0.9;

    float distance_to_center = length(frag_extrude);
    float distance_to_edge = abs(distance_to_center - frag_radius);
    float opacity_t = smoothstep(-stroke_radius, 0.0, -distance_to_edge);

    // Red = collision, hide label. Blue = no collision, label is showing.
    vec4 color = frag_placed > 0.5 ? vec4(0.0, 0.0, 1.0, 0.5) : vec4(1.0, 0.0, 0.0, 1.0);

    out_color = color * alpha * opacity_t;
}
)";
};

} // namespace shaders
} // namespace mln
