#include <mln/shaders/mtl/collision.hpp>
#include <mln/shaders/shader_defines.hpp>

namespace mln {
namespace shaders {

//
// Collision box

using CollisionBoxShaderSource = ShaderSource<BuiltIn::CollisionBoxShader, gfx::Backend::Type::Metal>;

const std::array<AttributeInfo, 3> CollisionBoxShaderSource::attributes = {
    AttributeInfo{0, gfx::AttributeDataType::Short2, collisionUBOCount + 0, idCollisionPosVertexAttribute},

    // Dynamic
    AttributeInfo{1, gfx::AttributeDataType::UShort2, collisionUBOCount + 1, idCollisionPlacedVertexAttribute},
    AttributeInfo{2, gfx::AttributeDataType::Float3, collisionUBOCount + 1, idCollisionMeasuredBoxVertexAttribute},
};
const std::array<TextureInfo, 0> CollisionBoxShaderSource::textures = {};

//
// Collision circle

using CollisionCircleShaderSource = ShaderSource<BuiltIn::CollisionCircleShader, gfx::Backend::Type::Metal>;

const std::array<AttributeInfo, 4> CollisionCircleShaderSource::attributes = {
    AttributeInfo{0, gfx::AttributeDataType::Short2, collisionUBOCount + 0, idCollisionPosVertexAttribute},
    AttributeInfo{1, gfx::AttributeDataType::Short2, collisionUBOCount + 0, idCollisionExtrudeVertexAttribute},

    // Dynamic
    AttributeInfo{2, gfx::AttributeDataType::UShort2, collisionUBOCount + 1, idCollisionPlacedVertexAttribute},
    AttributeInfo{3, gfx::AttributeDataType::Float3, collisionUBOCount + 1, idCollisionMeasuredBoxVertexAttribute},
};
const std::array<TextureInfo, 0> CollisionCircleShaderSource::textures = {};

} // namespace shaders
} // namespace mln
