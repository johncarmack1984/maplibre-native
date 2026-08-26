#pragma once

#include <mln/shaders/collision_layer_ubo.hpp>
#include <mln/shaders/shader_source.hpp>
#include <mln/shaders/webgpu/shader_program.hpp>

namespace mln {
namespace shaders {

template <>
struct ShaderSource<BuiltIn::CollisionBoxShader, gfx::Backend::Type::WebGPU> {
    static constexpr const char* name = "CollisionBoxShader";
    static const std::array<AttributeInfo, 3> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 0> textures;

    static constexpr auto vertex = R"(
struct VertexInput {
    @location(3) position: vec2<i32>,
    @location(4) placed: vec2<u32>,
    @location(5) measured_box: vec3<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) placed: f32,
    @location(1) not_used: f32,
};

struct CollisionDrawableUBO {
    matrix: mat4x4<f32>,
};

struct CollisionTilePropsUBO {
    pixel_extrude_scale: vec2<f32>,
    pad1: f32,
    pad2: f32,
};

@group(0) @binding(2) var<uniform> drawable: CollisionDrawableUBO;
@group(0) @binding(4) var<uniform> tile_props: CollisionTilePropsUBO;

@vertex
fn main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;

    // The collision index already measured this corner in viewport pixels; draw it verbatim.
    out.position = drawable.matrix * vec4<f32>(f32(in.position.x), f32(in.position.y), 0.0, 1.0);
    out.position.x += in.measured_box.x * tile_props.pixel_extrude_scale.x * 2.0 * out.position.w;
    out.position.y -= in.measured_box.y * tile_props.pixel_extrude_scale.y * 2.0 * out.position.w;

    out.placed = f32(in.placed.x);
    out.not_used = f32(in.placed.y);

    return out;
}
)";

    static constexpr auto fragment = R"(
struct FragmentInput {
    @location(0) placed: f32,
    @location(1) not_used: f32,
};

@fragment
fn main(in: FragmentInput) -> @location(0) vec4<f32> {
    let alpha = 0.5;

    // Red = collision, hide label
    var color = vec4<f32>(1.0, 0.0, 0.0, 1.0) * alpha;

    // Blue = no collision, label is showing
    if (in.placed > 0.5) {
        color = vec4<f32>(0.0, 0.0, 1.0, 0.5) * alpha;
    }

    if (in.not_used > 0.5) {
        // This box not used, fade it out
        color = color * 0.1;
    }

    return color;
}
)";
};

template <>
struct ShaderSource<BuiltIn::CollisionCircleShader, gfx::Backend::Type::WebGPU> {
    static constexpr const char* name = "CollisionCircleShader";
    static const std::array<AttributeInfo, 4> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 0> textures;

    static constexpr auto vertex = R"(
struct VertexInput {
    @location(3) position: vec2<i32>,
    @location(4) extrude: vec2<i32>,
    @location(5) placed: vec2<u32>,
    @location(6) measured_box: vec3<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) placed: f32,
    @location(1) radius: f32,
    @location(2) extrude: vec2<f32>,
};

struct CollisionDrawableUBO {
    matrix: mat4x4<f32>,
};

struct CollisionTilePropsUBO {
    pixel_extrude_scale: vec2<f32>,
    pad1: f32,
    pad2: f32,
};

@group(0) @binding(2) var<uniform> drawable: CollisionDrawableUBO;
@group(0) @binding(4) var<uniform> tile_props: CollisionTilePropsUBO;

@vertex
fn main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;

    // The sign of the extrusion picks the quad corner; the radius the collision index measured
    // for this circle sizes it, in viewport pixels.
    let quad_vertex_offset = vec2<f32>(
        select(1.0, -1.0, in.extrude.x < 0),
        select(1.0, -1.0, in.extrude.y < 0));

    let padding_factor = 1.2; // Pad the vertices slightly to make room for anti-alias blur
    let quad_vertex_extent = quad_vertex_offset * in.measured_box.z * padding_factor;

    var position = drawable.matrix * vec4<f32>(f32(in.position.x), f32(in.position.y), 0.0, 1.0);
    position.x += quad_vertex_extent.x * tile_props.pixel_extrude_scale.x * 2.0 * position.w;
    position.y += quad_vertex_extent.y * tile_props.pixel_extrude_scale.y * 2.0 * position.w;

    out.position = position;
    out.placed = f32(in.placed.x);
    out.radius = in.measured_box.z;
    out.extrude = quad_vertex_extent;
    return out;
}
)";

    static constexpr auto fragment = R"(
struct FragmentInput {
    @location(0) placed: f32,
    @location(1) radius: f32,
    @location(2) extrude: vec2<f32>,
};

@fragment
fn main(in: FragmentInput) -> @location(0) vec4<f32> {
    let alpha = 0.5;
    let stroke_radius = 0.9;

    let distance_to_center = length(in.extrude);
    let distance_to_edge = abs(distance_to_center - in.radius);
    let opacity_t = smoothstep(-stroke_radius, 0.0, -distance_to_edge);

    // Red = collision, hide label. Blue = no collision, label is showing.
    var color = vec4<f32>(1.0, 0.0, 0.0, 1.0);
    if (in.placed > 0.5) {
        color = vec4<f32>(0.0, 0.0, 1.0, 0.5);
    }

    return color * alpha * opacity_t;
}
)";
};

} // namespace shaders
} // namespace mln
