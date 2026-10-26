#pragma once

#include <mln/util/tile_matrix_set.hpp>

#include <array>
#include <functional>
#include <optional>
#include <string>

namespace mln {

/// A planar coordinate reference system and the square, power-of-two quad tile grid laid over it, so a map can draw
/// tiles that were cut in that system. Register it with `addProjection` and select it by name with the style's
/// `projection.type`. Every tile source of the map is taken to be cut in `tileMatrixSet`, the tiles are drawn as they
/// are, and CRS units count as meters wherever the map converts meters.
struct CRS {
    /// The name `projection.type` takes, like `EPSG:2193`. It must contain a colon: names without one are reserved for
    /// built-in projections.
    std::string name;
    /// Lng/lat in degrees to CRS coordinates. Called from any thread.
    std::function<std::array<double, 2>(const std::array<double, 2>& lngLat)> forward;
    /// CRS coordinates to lng/lat in degrees, or nothing where there is no answer. Called from any thread.
    std::function<std::optional<std::array<double, 2>>(const std::array<double, 2>& position)> inverse;
    TileMatrixSet tileMatrixSet;
};

/// Registers a projection for every map. Throws `std::invalid_argument` if the name has no colon or is taken, or if
/// `tileMatrixSet.extentAtZoom0` is not positive.
void addProjection(CRS);

/// Maps already in the projection keep it until their projection changes. Throws `std::invalid_argument` for a name
/// without a colon, since built-in projections cannot be removed.
void removeProjection(const std::string& name);

} // namespace mln
