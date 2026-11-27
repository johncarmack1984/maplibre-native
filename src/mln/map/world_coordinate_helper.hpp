#pragma once

#include <mln/util/crs.hpp>
#include <mln/util/geo.hpp>
#include <mln/util/geometry.hpp>
#include <mln/util/tile_matrix_set.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace mln {

/// Maps lat/lng to and from the world square, where tile 0/0/0 spans 0..1 on both axes and y grows down.
class WorldCoordinateHelper {
public:
    virtual ~WorldCoordinateHelper() = default;

    /// `mercator`, or the name of a registered projection.
    virtual const std::string& name() const = 0;
    /// Each lng/lat in degrees to its place in the world square, NaN where the projection has none.
    virtual void worldFromLngLats(std::span<const std::array<double, 2>> lngLats,
                                  std::span<std::array<double, 2>> world) const = 0;
    /// Each place in the world square to lng/lat in degrees, with the latitude within 90 degrees of the equator.
    virtual void lngLatsFromWorld(std::span<const std::array<double, 2>> world,
                                  std::span<std::array<double, 2>> lngLats) const = 0;
    Point<double> worldFromLatLng(const LatLng&) const;
    LatLng latLngFromWorld(const Point<double>&) const;
    virtual double metersPerWorldUnit(const Point<double>&) const = 0;
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
    void worldFromLngLats(std::span<const std::array<double, 2>> lngLats,
                          std::span<std::array<double, 2>> world) const override;
    /// A position without an answer uses the nearest one inside the world square, or else the last one with an answer
    /// on the line to the square's center, or else the converter's own finite answer; throws when there is none. Each
    /// fallback converts every position that needs it in one batch.
    void lngLatsFromWorld(std::span<const std::array<double, 2>> world,
                          std::span<std::array<double, 2>> lngLats) const override;
    double metersPerWorldUnit(const Point<double>&) const override { return extent; }
    bool wraps() const override { return false; }
    const TileMatrixSet& tileMatrixSet() const override { return crs->tileMatrixSet; }

    /// The latitudes, south and north, from the center of the tile matrix set toward each pole, up to which the
    /// projection stretches distances along the meridian less than ten times as much as at the center.
    const std::array<double, 2>& latitudeRange() const;

private:
    std::array<double, 2> crsPosition(const std::array<double, 2>& world) const;
    void finiteInverses(std::span<const std::array<double, 2>> positions,
                        std::span<std::array<double, 2>> lngLats) const;
    void ownAnswers(std::span<const std::array<double, 2>> world, std::span<std::array<double, 2>> lngLats) const;
    void answersTowardCenter(std::span<const std::array<double, 2>> world,
                             std::span<std::array<double, 2>> lngLats) const;
    double findLatitudeLimit(const LatLng& center, double pole) const;

    std::shared_ptr<const CRS> crs;
    double originX;
    double originY;
    double extent;
    mutable std::once_flag latitudeRangeFlag;
    mutable std::array<double, 2> latitudeRange_{};
};

/// The most positions `convertPoints` hands a converter in one call. A batch of a whole big geometry would need fresh
/// memory for every position twice over, and the page faults on it cost more than the calls the batch saves.
inline constexpr std::size_t maxConverterBatch = 65536;

/// Moves every point `forEachPoint` visits through `convert`, a helper's `worldFromLngLats` or `lngLatsFromWorld`, in
/// place, in batches of up to `maxConverterBatch` points. `forEachPoint(fn)` calls `fn` on each `Point<double>&`.
template <class ForEachPoint, class Convert>
void convertPoints(ForEachPoint&& forEachPoint, Convert&& convert) {
    std::vector<std::array<double, 2>> in;
    std::vector<std::array<double, 2>> out;
    std::vector<Point<double>*> points;
    const auto flush = [&] {
        out.resize(in.size());
        convert(std::span<const std::array<double, 2>>(in), std::span<std::array<double, 2>>(out));
        for (std::size_t i = 0; i < points.size(); ++i) {
            *points[i] = {out[i][0], out[i][1]};
        }
        in.clear();
        points.clear();
    };
    forEachPoint([&](Point<double>& point) {
        in.push_back({point.x, point.y});
        points.push_back(&point);
        if (in.size() == maxConverterBatch) {
            flush();
        }
    });
    if (!in.empty()) {
        flush();
    }
}

/// A lat/lng box sampled on a grid, 17 points on a side with its edges and corners, row by row from the north-west.
std::vector<LatLng> latLngBoxSamples(const LatLngBounds&);

/// The axis-aligned rectangle in the world square around a lat/lng box, as min x, min y, max x, max y: the box sampled
/// on a grid, grown by how far its edges stray from the lines between neighboring samples. In Mercator that is the
/// rectangle of its corners.
std::array<double, 4> latLngBoxToWorldBox(const WorldCoordinateHelper&, const LatLngBounds&);

/// The helper for the projection registered under `name`, `identity` included, or null. A projection has one helper
/// from `addProjection` on, so its latitude range is found once, and whatever is drawn in it holds that helper: a map
/// keeps drawing in a removed projection until its projection changes.
std::shared_ptr<const CRSWorldCoordinateHelper> getRegisteredProjection(const std::string& name);

/// Mercator keeps the formulas each call site had, so its answers stay bit for bit what they were.
inline bool isMercator(const WorldCoordinateHelper& helper) {
    return &helper == mercatorWorldCoordinateHelper().get();
}

} // namespace mln
