#include <mln/util/geojson_reproject.hpp>

#include <mln/map/world_coordinate_helper.hpp>

#include <mapbox/geometry/for_each_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <span>

namespace mln {

namespace {

using Position = mapbox::geometry::point<double>;
using GeoGeometry = mapbox::geometry::geometry<double>;
using GeoFeature = mapbox::feature::feature<double>;
using GeoFeatures = mapbox::feature::feature_collection<double>;

/// The longitude step, in degrees, between the positions an edge along a clamped latitude gets, so that it follows the
/// parallel in the projection instead of cutting across it.
constexpr double parallelStep = 10;

/// Maps the positions of a geometry: points one by one, lines and rings as a whole, which may gain or lose positions.
template <class Mapping>
struct GeometryMapper {
    const Mapping& mapping;

    GeoGeometry operator()(const mapbox::geometry::empty& empty) const { return empty; }

    GeoGeometry operator()(const mapbox::geometry::point<double>& point) const {
        if (const auto mapped = mapping.position(point)) {
            return *mapped;
        }
        return mapbox::geometry::empty{};
    }

    GeoGeometry operator()(const mapbox::geometry::multi_point<double>& points) const {
        mapbox::geometry::multi_point<double> result;
        result.reserve(points.size());
        for (const auto& point : points) {
            if (const auto mapped = mapping.position(point)) {
                result.push_back(*mapped);
            }
        }
        return result;
    }

    GeoGeometry operator()(const mapbox::geometry::line_string<double>& line) const { return mapping.line(line); }

    GeoGeometry operator()(const mapbox::geometry::multi_line_string<double>& lines) const {
        mapbox::geometry::multi_line_string<double> result;
        result.reserve(lines.size());
        for (const auto& line : lines) {
            result.push_back(mapping.line(line));
        }
        return result;
    }

    GeoGeometry operator()(const mapbox::geometry::polygon<double>& polygon) const { return mapPolygon(polygon); }

    GeoGeometry operator()(const mapbox::geometry::multi_polygon<double>& polygons) const {
        mapbox::geometry::multi_polygon<double> result;
        result.reserve(polygons.size());
        for (const auto& polygon : polygons) {
            result.push_back(mapPolygon(polygon));
        }
        return result;
    }

    GeoGeometry operator()(const mapbox::geometry::geometry_collection<double>& geometries) const {
        mapbox::geometry::geometry_collection<double> result;
        result.reserve(geometries.size());
        for (const auto& geometry : geometries) {
            result.push_back(GeoGeometry::visit(geometry, *this));
        }
        return result;
    }

    mapbox::geometry::polygon<double> mapPolygon(const mapbox::geometry::polygon<double>& polygon) const {
        mapbox::geometry::polygon<double> result;
        result.reserve(polygon.size());
        for (const auto& ring : polygon) {
            result.push_back(mapping.line(ring));
        }
        return result;
    }

    GeoFeature feature(const GeoFeature& feature) const {
        return {GeoGeometry::visit(feature.geometry, *this), feature.properties, feature.id};
    }

    GeoFeatures features(const GeoFeatures& features) const {
        GeoFeatures result;
        result.reserve(features.size());
        for (const auto& feature : features) {
            result.push_back(this->feature(feature));
        }
        return result;
    }
};

/// Lines and rings moved onto the CRS's latitude range, still in lng/lat, and positions that are not lng/lat left out.
class OntoLatitudeRange {
public:
    explicit OntoLatitudeRange(const std::array<double, 2>& latitudeRange)
        : south(latitudeRange[0]),
          north(latitudeRange[1]) {}

    std::optional<Position> position(const Position& lngLat) const {
        if (!std::isfinite(lngLat.x) || !(std::abs(lngLat.y) <= 90)) {
            return std::nullopt;
        }
        return lngLat;
    }

    template <class Line>
    Line line(const Line& positions) const {
        Line result;
        result.reserve(positions.size());
        if (std::ranges::all_of(positions,
                                [&](const Position& lngLat) { return lngLat.y >= south && lngLat.y <= north; })) {
            for (const auto& lngLat : positions) {
                add(result, lngLat);
            }
            return result;
        }
        double previousLongitude = 0;
        double previousLatitude = std::numeric_limits<double>::quiet_NaN();
        for (const auto& lngLat : positions) {
            if (!std::isfinite(lngLat.x) || std::isnan(lngLat.y)) {
                continue;
            }
            const double latitude = std::clamp(lngLat.y, south, north);
            if (latitude == previousLatitude && isLimit(latitude)) {
                const auto steps = static_cast<int>(std::ceil(std::abs(lngLat.x - previousLongitude) / parallelStep));
                for (int step = 1; step < steps; ++step) {
                    add(result, {previousLongitude + (lngLat.x - previousLongitude) * step / steps, latitude});
                }
            }
            add(result, {lngLat.x, latitude});
            previousLongitude = lngLat.x;
            previousLatitude = latitude;
        }
        return result;
    }

private:
    bool isLimit(const double latitude) const {
        return (latitude == south && south > -90) || (latitude == north && north < 90);
    }

    template <class Line>
    void add(Line& line, const Position& lngLat) const {
        if (const auto inRange = position(lngLat)) {
            line.push_back(*inRange);
        }
    }

    const double south;
    const double north;
};

/// Positions without a finite place left out.
struct Finite {
    static bool isFinite(const Position& point) { return std::isfinite(point.x) && std::isfinite(point.y); }

    std::optional<Position> position(const Position& point) const {
        return isFinite(point) ? std::optional(point) : std::nullopt;
    }

    template <class Line>
    Line line(const Line& positions) const {
        Line result;
        result.reserve(positions.size());
        std::ranges::copy_if(positions, std::back_inserter(result), isFinite);
        return result;
    }
};

template <class Fn>
void forEachPoint(GeoFeatures& features, Fn&& fn) {
    for (auto& feature : features) {
        mapbox::geometry::for_each_point(feature.geometry, fn);
    }
}

template <class Fn>
void forEachPoint(GeoJSON& geoJSON, Fn&& fn) {
    geoJSON.match([&](GeoGeometry& geometry) { mapbox::geometry::for_each_point(geometry, fn); },
                  [&](GeoFeature& feature) { mapbox::geometry::for_each_point(feature.geometry, fn); },
                  [&](GeoFeatures& features) { forEachPoint(features, fn); });
}

template <class Mapping>
GeoJSON mapGeoJSON(const GeoJSON& geoJSON, const Mapping& mapping) {
    const GeometryMapper<Mapping> mapper{mapping};
    return geoJSON.match([&](const GeoGeometry& geometry) -> GeoJSON { return GeoGeometry::visit(geometry, mapper); },
                         [&](const GeoFeature& feature) -> GeoJSON { return mapper.feature(feature); },
                         [&](const GeoFeatures& features) -> GeoJSON { return mapper.features(features); });
}

} // namespace

GeoJSON reprojectGeoJSONToWorld(const GeoJSON& geoJSON, const CRSWorldCoordinateHelper& helper) {
    GeoJSON world = mapGeoJSON(geoJSON, OntoLatitudeRange(helper.latitudeRange()));
    bool allFinite = true;
    convertPoints([&](auto&& visit) { forEachPoint(world, visit); },
                  [&](std::span<const std::array<double, 2>> lngLats, std::span<std::array<double, 2>> positions) {
                      helper.worldFromLngLats(lngLats, positions);
                      allFinite = allFinite &&
                                  std::ranges::all_of(positions, [](const std::array<double, 2>& position) {
                                      return std::isfinite(position[0]) && std::isfinite(position[1]);
                                  });
                  });
    if (!allFinite) {
        return mapGeoJSON(world, Finite());
    }
    return world;
}

GeoFeatures reprojectFeaturesFromWorld(GeoFeatures features, const WorldCoordinateHelper& helper) {
    convertPoints([&](auto&& visit) { forEachPoint(features, visit); },
                  [&](std::span<const std::array<double, 2>> world, std::span<std::array<double, 2>> lngLats) {
                      helper.lngLatsFromWorld(world, lngLats);
                  });
    return features;
}

} // namespace mln
