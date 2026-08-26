#pragma once

#include <mln/shaders/collision_layer_ubo.hpp>
#include <mln/shaders/shader_source.hpp>
#include <mln/shaders/mtl/shader_program.hpp>

namespace mln {
namespace shaders {

constexpr auto collisionShaderPrelude = R"(

enum {
    idCollisionDrawableUBO = idDrawableReservedVertexOnlyUBO,
    idCollisionTilePropsUBO = drawableReservedUBOCount,
    collisionUBOCount
};

struct alignas(16) CollisionDrawableUBO {
    /*  0 */ float4x4 matrix;
    /* 64 */
};
static_assert(sizeof(CollisionDrawableUBO) == 4 * 16, "wrong size");

struct alignas(16) CollisionTilePropsUBO {
    /*  0 */ float2 pixel_extrude_scale;
    /*  8 */ float pad1;
    /* 12 */ float pad2;
    /* 16 */
};
static_assert(sizeof(CollisionTilePropsUBO) == 16, "wrong size");

)";

template <>
struct ShaderSource<BuiltIn::CollisionBoxShader, gfx::Backend::Type::Metal> {
    static constexpr auto name = "CollisionBoxShader";
    static constexpr auto vertexMainFunction = "vertexMain";
    static constexpr auto fragmentMainFunction = "fragmentMain";

    static const std::array<AttributeInfo, 3> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 0> textures;

    static constexpr auto prelude = collisionShaderPrelude;
    static constexpr auto source = R"(

struct VertexStage {
    short2 pos [[attribute(0)]];
    ushort2 placed [[attribute(1)]];
    float3 measured_box [[attribute(2)]];
};

struct FragmentStage {
    float4 position [[position, invariant]];
    float placed;
    float notUsed;
};

FragmentStage vertex vertexMain(thread const VertexStage vertx [[stage_in]],
                                device const CollisionDrawableUBO& drawable [[buffer(idCollisionDrawableUBO)]],
                                device const CollisionTilePropsUBO& tileProps [[buffer(idCollisionTilePropsUBO)]]) {

    // The collision index already measured this corner in viewport pixels; draw it verbatim.
    float4 position = drawable.matrix * float4(float2(vertx.pos), 0.0, 1.0);
    position.xy += vertx.measured_box.xy * tileProps.pixel_extrude_scale * 2.0 * float2(1.0, -1.0) * position.w;

    float placed = float(vertx.placed.x);
    float notUsed = float(vertx.placed.y);

    return {
        .position       = position,
        .placed         = placed,
        .notUsed        = notUsed,
    };
}

half4 fragment fragmentMain(FragmentStage in [[stage_in]]) {

    float alpha = 0.5;

    // Red = collision, hide label
    float4 color = float4(1.0, 0.0, 0.0, 1.0) * alpha;

    // Blue = no collision, label is showing
    if (in.placed > 0.5) {
        color = float4(0.0, 0.0, 1.0, 0.5) * alpha;
    }

    if (in.notUsed > 0.5) {
        // This box not used, fade it out
        color *= 0.1;
    }

    return half4(color);
}
)";
};

template <>
struct ShaderSource<BuiltIn::CollisionCircleShader, gfx::Backend::Type::Metal> {
    static constexpr auto name = "CollisionCircleShader";
    static constexpr auto vertexMainFunction = "vertexMain";
    static constexpr auto fragmentMainFunction = "fragmentMain";

    static const std::array<AttributeInfo, 4> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 0> textures;

    static constexpr auto prelude = collisionShaderPrelude;
    static constexpr auto source = R"(

struct VertexStage {
    short2 pos [[attribute(0)]];
    short2 extrude [[attribute(1)]];
    ushort2 placed [[attribute(2)]];
    float3 measured_box [[attribute(3)]];
};

struct FragmentStage {
    float4 position [[position, invariant]];
    float placed;
    float radius;
    float2 extrude;
};

FragmentStage vertex vertexMain(thread const VertexStage vertx [[stage_in]],
                                device const CollisionDrawableUBO& drawable [[buffer(idCollisionDrawableUBO)]],
                                device const CollisionTilePropsUBO& tileProps [[buffer(idCollisionTilePropsUBO)]]) {

    // The sign of the extrusion picks the quad corner; the radius the collision index measured
    // for this circle sizes it, in viewport pixels.
    float2 quadVertexOffset = float2(vertx.extrude.x < 0 ? -1.0 : 1.0, vertx.extrude.y < 0 ? -1.0 : 1.0);

    float padding_factor = 1.2; // Pad the vertices slightly to make room for anti-alias blur
    float2 quadVertexExtent = quadVertexOffset * vertx.measured_box.z * padding_factor;

    float4 position = drawable.matrix * float4(float2(vertx.pos), 0.0, 1.0);
    position.xy += quadVertexExtent * tileProps.pixel_extrude_scale * 2.0 * position.w;

    return {
        .position       = position,
        .placed         = float(vertx.placed.x),
        .radius         = vertx.measured_box.z,
        .extrude        = quadVertexExtent,
    };
}

half4 fragment fragmentMain(FragmentStage in [[stage_in]]) {

    float alpha = 0.5;
    float stroke_radius = 0.9;

    float distance_to_center = length(in.extrude);
    float distance_to_edge = abs(distance_to_center - in.radius);
    float opacity_t = smoothstep(-stroke_radius, 0.0, -distance_to_edge);

    // Red = collision, hide label. Blue = no collision, label is showing.
    float4 color = in.placed > 0.5 ? float4(0.0, 0.0, 1.0, 0.5) : float4(1.0, 0.0, 0.0, 1.0);

    return half4(color * alpha * opacity_t);
}
)";
};

} // namespace shaders
} // namespace mln
