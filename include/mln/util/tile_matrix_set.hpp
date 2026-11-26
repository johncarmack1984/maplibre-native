#pragma once

#include <array>

namespace mln {

/// The square tile grid over a projection's plane: the top-left corner of tile 0/0/0 (its smallest x and largest y)
/// and the side of that tile, in the projection's units.
struct TileMatrixSet {
    std::array<double, 2> origin{};
    double extentAtZoom0 = 0;
};

} // namespace mln
