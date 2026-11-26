#include <mln/map/mercator_projection.hpp>
#include <mln/util/projection.hpp>

namespace mln {

MercatorProjection::MercatorProjection(std::shared_ptr<const WorldCoordinateHelper> helper_)
    : helper(std::move(helper_)) {}

Point<double> MercatorProjection::project(const LatLng& latLng, double scale) const {
    if (isMercator(*helper)) {
        return Projection::project(latLng, scale);
    }
    return helper->worldFromLatLng(latLng) * Projection::worldSize(scale);
}

LatLng MercatorProjection::unproject(const Point<double>& point, double scale, LatLng::WrapMode wrapMode) const {
    if (isMercator(*helper)) {
        return Projection::unproject(point, scale, wrapMode);
    }
    return helper->latLngFromWorld(point / Projection::worldSize(scale));
}

} // namespace mln
