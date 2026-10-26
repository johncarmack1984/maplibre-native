#pragma once

#include <mln/map/projection_base.hpp>

namespace mln {

class MercatorProjection final : public ProjectionBase {
public:
    explicit MercatorProjection(std::shared_ptr<const WorldCoordinateHelper> = mercatorWorldCoordinateHelper());

    const WorldCoordinateHelper& worldCoordinateHelper() const override { return *helper; }

    Point<double> project(const LatLng&, double scale) const override;
    LatLng unproject(const Point<double>&, double scale, LatLng::WrapMode) const override;

    void tileMatrix(mat4&, const UnwrappedTileID&, double scale) const override;

    ProjectionData getProjectionData(const TransformState&,
                                     const UnwrappedTileID&,
                                     const mat4& mercatorMatrix) const override;

    ProjectedTilePoint projectTilePoint(const ProjectionData&,
                                        const UnwrappedTileID&,
                                        const Point<double>&,
                                        double elevation) const override;

private:
    std::shared_ptr<const WorldCoordinateHelper> helper;
};

} // namespace mln
