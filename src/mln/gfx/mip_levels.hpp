#pragma once

#include <mln/util/size.hpp>

#include <cstdint>
#include <vector>

namespace mln {
namespace gfx {

/// The number of levels in a full mip chain for a texture of this size, down to 1x1.
uint32_t mipLevelCount(Size size);

/// The next mip level of an RGBA8 image: half the size rounded down and at least 1, each texel the average of the 2x2
/// block above it.
std::vector<uint8_t> downsampleRGBA8(const uint8_t* pixels, Size size, Size& levelSize);

} // namespace gfx
} // namespace mln
