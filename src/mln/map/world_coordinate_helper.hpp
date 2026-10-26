#pragma once

#include <mln/util/geo.hpp>
#include <mln/util/geometry.hpp>
#include <mln/util/tile_matrix_set.hpp>

#include <memory>
#include <string>

namespace mln {

/// Maps lat/lng to and from the world square, where tile 0/0/0 spans 0..1 on both axes and y grows down.
class WorldCoordinateHelper {
public:
    virtual ~WorldCoordinateHelper() = default;

    /// `mercator`, or the name of a registered projection.
    virtual const std::string& name() const = 0;
    virtual Point<double> worldFromLatLng(const LatLng&) const = 0;
    virtual LatLng latLngFromWorld(const Point<double>&) const = 0;
    /// Whether the position has an answer of its own, rather than the nearest one `latLngFromWorld` falls back on.
    virtual bool hasLatLng(const Point<double>&) const = 0;
    virtual double metersPerWorldUnit(const Point<double>&) const = 0;
    virtual double worldZFromAltitude(double altitude, const LatLng&) const = 0;
    /// Whether the world repeats east and west: world copies, longitude wrapping and the Mercator latitude limit.
    virtual bool wraps() const = 0;
    virtual const TileMatrixSet& tileMatrixSet() const = 0;
};

const std::shared_ptr<const WorldCoordinateHelper>& mercatorWorldCoordinateHelper();

/// Mercator keeps the formulas each call site had, so its answers stay bit for bit what they were.
inline bool isMercator(const WorldCoordinateHelper& helper) {
    return &helper == mercatorWorldCoordinateHelper().get();
}

} // namespace mln
