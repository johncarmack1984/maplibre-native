#pragma once

#include <mln/util/crs.hpp>
#include <mln/util/geo.hpp>
#include <mln/util/geometry.hpp>
#include <mln/util/tile_matrix_set.hpp>

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

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

/// The mapping for a CRS: world x/y are its coordinates relative to the tile matrix set origin, scaled so tile 0/0/0 is
/// the 0..1 square, with y growing down. A CRS is a bounded plane: no world copies, no wrapping, no latitude limit.
class CRSWorldCoordinateHelper final : public WorldCoordinateHelper {
public:
    explicit CRSWorldCoordinateHelper(std::shared_ptr<const CRS>);

    const std::string& name() const override { return crs->name; }
    Point<double> worldFromLatLng(const LatLng&) const override;
    /// A position without an answer uses the nearest one inside the world square, or else the last one with an answer
    /// on the line to the square's center, or else the converter's own finite answer; throws when there is none.
    LatLng latLngFromWorld(const Point<double>&) const override;
    bool hasLatLng(const Point<double>&) const override;
    double metersPerWorldUnit(const Point<double>&) const override { return extent; }
    double worldZFromAltitude(double altitude, const LatLng&) const override { return altitude / extent; }
    bool wraps() const override { return false; }
    const TileMatrixSet& tileMatrixSet() const override { return crs->tileMatrixSet; }

    /// The latitudes, south and north, from the center of the tile matrix set toward each pole, up to which the
    /// projection stretches distances along the meridian less than ten times as much as at the center.
    const std::array<double, 2>& latitudeRange() const;

private:
    std::optional<std::array<double, 2>> inverse(const Point<double>&) const;
    std::optional<std::array<double, 2>> finiteInverse(const Point<double>&) const;
    std::optional<std::array<double, 2>> inverseTowardCenter(const Point<double>&) const;
    double findLatitudeLimit(double pole) const;
    double meridianStretch(double longitude, double latitude) const;

    std::shared_ptr<const CRS> crs;
    double originX;
    double originY;
    double extent;
    mutable std::once_flag latitudeRangeFlag;
    mutable std::array<double, 2> latitudeRange_{};
};

/// A lat/lng box sampled on a grid, 17 points on a side with its edges and corners, row by row from the north-west.
std::vector<LatLng> latLngBoxSamples(const LatLngBounds&);

/// The axis-aligned rectangle in the world square around a lat/lng box, as min x, min y, max x, max y: the box sampled
/// on a grid, grown by how far its edges stray from the lines between neighboring samples. In Mercator that is the
/// rectangle of its corners.
std::array<double, 4> latLngBoxToWorldBox(const WorldCoordinateHelper&, const LatLngBounds&);

/// The CRS registered under `name`, `identity` included, or null.
std::shared_ptr<const CRS> getRegisteredProjection(const std::string& name);

/// Mercator keeps the formulas each call site had, so its answers stay bit for bit what they were.
inline bool isMercator(const WorldCoordinateHelper& helper) {
    return &helper == mercatorWorldCoordinateHelper().get();
}

} // namespace mln
