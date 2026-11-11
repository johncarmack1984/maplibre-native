#include <mln/gfx/mip_levels.hpp>

#include <algorithm>
#include <cmath>

namespace mln {
namespace gfx {

uint32_t mipLevelCount(const Size size) {
    if (size.width == 0 || size.height == 0) {
        return 1;
    }
    return static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
}

std::vector<uint8_t> downsampleRGBA8(const uint8_t* pixels, const Size size, Size& levelSize) {
    levelSize = {std::max(1u, size.width / 2), std::max(1u, size.height / 2)};
    std::vector<uint8_t> level(static_cast<size_t>(levelSize.width) * levelSize.height * 4);
    for (uint32_t y = 0; y < levelSize.height; ++y) {
        const uint32_t y0 = std::min(y * 2, size.height - 1);
        const uint32_t y1 = std::min(y * 2 + 1, size.height - 1);
        for (uint32_t x = 0; x < levelSize.width; ++x) {
            const uint32_t x0 = std::min(x * 2, size.width - 1);
            const uint32_t x1 = std::min(x * 2 + 1, size.width - 1);
            const uint8_t* a = pixels + ((static_cast<size_t>(y0) * size.width) + x0) * 4;
            const uint8_t* b = pixels + ((static_cast<size_t>(y0) * size.width) + x1) * 4;
            const uint8_t* c = pixels + ((static_cast<size_t>(y1) * size.width) + x0) * 4;
            const uint8_t* d = pixels + ((static_cast<size_t>(y1) * size.width) + x1) * 4;
            uint8_t* out = level.data() + ((static_cast<size_t>(y) * levelSize.width) + x) * 4;
            for (int channel = 0; channel < 4; ++channel) {
                out[channel] = static_cast<uint8_t>((a[channel] + b[channel] + c[channel] + d[channel] + 2) / 4);
            }
        }
    }
    return level;
}

} // namespace gfx
} // namespace mln
