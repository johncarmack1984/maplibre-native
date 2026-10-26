#include <mln/map/world_coordinate_helper.hpp>
#include <mln/util/constants.hpp>
#include <mln/math/clamp.hpp>
#include <mln/util/projection.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace mln {

namespace {

class MercatorWorldCoordinateHelper final : public WorldCoordinateHelper {
public:
    const std::string& name() const override { return name_; }

    Point<double> worldFromLatLng(const LatLng& latLng) const override {
        return Projection::project(latLng, 1.0 / util::tileSize_D);
    }

    LatLng latLngFromWorld(const Point<double>& point) const override {
        return Projection::unproject(point, 1.0 / util::tileSize_D);
    }

    bool hasLatLng(const Point<double>&) const override { return true; }

    double metersPerWorldUnit(const Point<double>& point) const override {
        return Projection::getMetersPerPixelAtLatitude(latLngFromWorld(point).latitude(), 0) * util::tileSize_D;
    }

    double worldZFromAltitude(double altitude, const LatLng& latLng) const override {
        return altitude / (Projection::getMetersPerPixelAtLatitude(latLng.latitude(), 0) * util::tileSize_D);
    }

    bool wraps() const override { return true; }

    const TileMatrixSet& tileMatrixSet() const override { return tileMatrixSet_; }

private:
    static constexpr double halfExtent = std::numbers::pi * util::EARTH_RADIUS_M;
    const std::string name_ = "mercator";
    const TileMatrixSet tileMatrixSet_{.origin = {-halfExtent, halfExtent}, .extentAtZoom0 = 2 * halfExtent};
};

// How far `forward` may take an inverse from the position it came from, as a fraction of the zoom 0 extent, for the
// inverse to count as that position's answer.
constexpr double roundTripTolerance = 1e-3;

// How many times the line to the center of the world square is halved in the search for the last position with an
// answer.
constexpr int edgeSearchSteps = 30;

// How many times as much as at the center of its tile matrix set a projection may stretch distances along the meridian.
constexpr double maxMeridianStretch = 10;

std::shared_ptr<const CRS> identityCRS() {
    static const auto crs = std::make_shared<const CRS>(
        CRS{.name = "identity",
            .forward = [](const std::array<double, 2>& lngLat) { return lngLat; },
            .inverse = [](const std::array<double, 2>& position) -> std::optional<std::array<double, 2>> {
                return position;
            },
            .tileMatrixSet = {.origin = {-90, 90}, .extentAtZoom0 = 180}});
    return crs;
}

std::mutex registryMutex;

std::unordered_map<std::string, std::shared_ptr<const CRS>>& registry() {
    static std::unordered_map<std::string, std::shared_ptr<const CRS>> projections{{"identity", identityCRS()}};
    return projections;
}

} // namespace

const std::shared_ptr<const WorldCoordinateHelper>& mercatorWorldCoordinateHelper() {
    static const std::shared_ptr<const WorldCoordinateHelper> helper =
        std::make_shared<const MercatorWorldCoordinateHelper>();
    return helper;
}

CRSWorldCoordinateHelper::CRSWorldCoordinateHelper(std::shared_ptr<const CRS> crs_)
    : crs(std::move(crs_)),
      originX(crs->tileMatrixSet.origin[0]),
      originY(crs->tileMatrixSet.origin[1]),
      extent(crs->tileMatrixSet.extentAtZoom0) {}

Point<double> CRSWorldCoordinateHelper::worldFromLatLng(const LatLng& latLng) const {
    const auto position = crs->forward({latLng.longitude(), latLng.latitude()});
    return {(position[0] - originX) / extent, (originY - position[1]) / extent};
}

LatLng CRSWorldCoordinateHelper::latLngFromWorld(const Point<double>& point) const {
    const Point<double> inside{util::clamp(point.x, 0.0, 1.0), util::clamp(point.y, 0.0, 1.0)};
    auto position = inverse(point);
    if (!position) position = inverse(inside);
    if (!position) position = inverseTowardCenter(inside);
    if (!position) position = finiteInverse(inside);
    if (!position) {
        throw std::runtime_error("The inverse of the projection \"" + crs->name +
                                 "\" has no answer inside its tile matrix set.");
    }
    return {util::clamp((*position)[1], -90.0, 90.0), (*position)[0], LatLng::Unwrapped};
}

bool CRSWorldCoordinateHelper::hasLatLng(const Point<double>& point) const {
    return inverse(point).has_value();
}

std::optional<std::array<double, 2>> CRSWorldCoordinateHelper::inverse(const Point<double>& point) const {
    const auto position = finiteInverse(point);
    if (!position) {
        return std::nullopt;
    }
    const auto forward = crs->forward(*position);
    const double distance = std::hypot(forward[0] - (originX + point.x * extent),
                                       forward[1] - (originY - point.y * extent));
    if (!(distance <= roundTripTolerance * extent)) {
        return std::nullopt;
    }
    return position;
}

std::optional<std::array<double, 2>> CRSWorldCoordinateHelper::finiteInverse(const Point<double>& point) const {
    std::optional<std::array<double, 2>> position;
    try {
        position = crs->inverse({originX + point.x * extent, originY - point.y * extent});
    } catch (...) {
        return std::nullopt;
    }
    if (!position || !std::isfinite((*position)[0]) || !std::isfinite((*position)[1])) {
        return std::nullopt;
    }
    return position;
}

std::optional<std::array<double, 2>> CRSWorldCoordinateHelper::inverseTowardCenter(const Point<double>& point) const {
    double answered = 0;
    double unanswered = 1;
    const auto along = [&](double fraction) {
        return Point<double>{0.5 + (point.x - 0.5) * fraction, 0.5 + (point.y - 0.5) * fraction};
    };
    for (int step = 0; step < edgeSearchSteps; ++step) {
        const double fraction = (answered + unanswered) / 2;
        if (inverse(along(fraction))) {
            answered = fraction;
        } else {
            unanswered = fraction;
        }
    }
    return inverse(along(answered));
}

const std::array<double, 2>& CRSWorldCoordinateHelper::latitudeRange() const {
    std::call_once(latitudeRangeFlag, [&] { latitudeRange_ = {findLatitudeLimit(-90), findLatitudeLimit(90)}; });
    return latitudeRange_;
}

double CRSWorldCoordinateHelper::findLatitudeLimit(double pole) const {
    const LatLng center = latLngFromWorld({0.5, 0.5});
    const double maxStretch = maxMeridianStretch * meridianStretch(center.longitude(), center.latitude());
    if (!(maxStretch > 0)) {
        return pole;
    }
    const double step = pole > center.latitude() ? 1 : -1;
    for (double latitude = center.latitude(); std::abs(pole - latitude) > 1; latitude += step) {
        if (!(meridianStretch(center.longitude(), latitude + step) <= maxStretch)) {
            return latitude;
        }
    }
    return pole;
}

double CRSWorldCoordinateHelper::meridianStretch(double longitude, double latitude) const {
    const auto position = crs->forward({longitude, latitude});
    const auto towardEquator = crs->forward({longitude, latitude >= 0 ? latitude - 0.01 : latitude + 0.01});
    return std::hypot(towardEquator[0] - position[0], towardEquator[1] - position[1]);
}

std::array<double, 4> latLngBoxToWorldBox(const WorldCoordinateHelper& helper, const LatLngBounds& box) {
    // The corners alone miss most of a box whose edges curve, as a parallel does around a pole.
    constexpr int steps = 16;
    const auto longitudeAt = [&](int step) {
        return step == steps ? box.east() : box.west() + (box.east() - box.west()) * step / steps;
    };
    const auto latitudeAt = [&](int step) {
        return step == steps ? box.south() : box.north() + (box.south() - box.north()) * step / steps;
    };
    const auto toPoint = [&](double longitude, double latitude) {
        return helper.worldFromLatLng({latitude, longitude});
    };
    std::vector<Point<double>> points;
    points.reserve((steps + 1) * (steps + 1));
    for (int row = 0; row <= steps; ++row) {
        for (int column = 0; column <= steps; ++column) {
            points.push_back(toPoint(longitudeAt(column), latitudeAt(row)));
        }
    }
    const auto pointAt = [&](int row, int column) {
        return points[row * (steps + 1) + column];
    };
    double bulge = 0;
    for (int step = 0; step < steps; ++step) {
        const double longitudeHalfway = (longitudeAt(step) + longitudeAt(step + 1)) / 2;
        const double latitudeHalfway = (latitudeAt(step) + latitudeAt(step + 1)) / 2;
        const std::array<std::array<Point<double>, 3>, 4> edges{{
            {pointAt(0, step), pointAt(0, step + 1), toPoint(longitudeHalfway, box.north())},
            {pointAt(steps, step), pointAt(steps, step + 1), toPoint(longitudeHalfway, box.south())},
            {pointAt(step, 0), pointAt(step + 1, 0), toPoint(box.west(), latitudeHalfway)},
            {pointAt(step, steps), pointAt(step + 1, steps), toPoint(box.east(), latitudeHalfway)},
        }};
        for (const auto& [start, end, halfway] : edges) {
            const Point<double> chord = end - start;
            const Point<double> toHalfway = halfway - start;
            const double chordLength = std::hypot(chord.x, chord.y);
            const double distance = chordLength > 0
                                        ? std::abs(chord.x * toHalfway.y - chord.y * toHalfway.x) / chordLength
                                        : std::hypot(toHalfway.x, toHalfway.y);
            bulge = std::max(bulge, distance);
        }
    }
    std::array<double, 4> rectangle{std::numeric_limits<double>::infinity(),
                                    std::numeric_limits<double>::infinity(),
                                    -std::numeric_limits<double>::infinity(),
                                    -std::numeric_limits<double>::infinity()};
    for (const auto& point : points) {
        rectangle[0] = std::min(rectangle[0], point.x);
        rectangle[1] = std::min(rectangle[1], point.y);
        rectangle[2] = std::max(rectangle[2], point.x);
        rectangle[3] = std::max(rectangle[3], point.y);
    }
    return {rectangle[0] - bulge, rectangle[1] - bulge, rectangle[2] + bulge, rectangle[3] + bulge};
}

std::shared_ptr<const CRS> getRegisteredProjection(const std::string& name) {
    std::scoped_lock lock(registryMutex);
    const auto it = registry().find(name);
    return it == registry().end() ? nullptr : it->second;
}

void addProjection(CRS crs) {
    if (crs.name.find(':') == std::string::npos) {
        throw std::invalid_argument("The projection name \"" + crs.name +
                                    "\" needs a colon, like \"EPSG:2193\" or \"custom:equal-earth\": names without one "
                                    "are reserved for built-in projections.");
    }
    std::scoped_lock lock(registryMutex);
    if (registry().contains(crs.name)) {
        throw std::invalid_argument("A projection called \"" + crs.name + "\" already exists.");
    }
    if (!(crs.tileMatrixSet.extentAtZoom0 > 0)) {
        throw std::invalid_argument("The projection \"" + crs.name +
                                    "\" needs a tileMatrixSet.extentAtZoom0 greater than zero.");
    }
    if (!crs.forward || !crs.inverse) {
        throw std::invalid_argument("The projection \"" + crs.name + "\" needs both forward and inverse.");
    }
    auto name = crs.name;
    registry().emplace(std::move(name), std::make_shared<const CRS>(std::move(crs)));
}

void removeProjection(const std::string& name) {
    if (name.find(':') == std::string::npos) {
        throw std::invalid_argument("The projection name \"" + name +
                                    "\" has no colon, so it is reserved for built-in projections, which cannot be "
                                    "removed.");
    }
    std::scoped_lock lock(registryMutex);
    registry().erase(name);
}

} // namespace mln
