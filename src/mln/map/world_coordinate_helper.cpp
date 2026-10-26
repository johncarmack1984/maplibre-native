#include <mln/map/world_coordinate_helper.hpp>
#include <mln/util/constants.hpp>
#include <mln/util/projection.hpp>

#include <numbers>

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

} // namespace

const std::shared_ptr<const WorldCoordinateHelper>& mercatorWorldCoordinateHelper() {
    static const std::shared_ptr<const WorldCoordinateHelper> helper =
        std::make_shared<const MercatorWorldCoordinateHelper>();
    return helper;
}

} // namespace mln
