#include <mln/test/util.hpp>

#include <mln/map/transform.hpp>
#include <mln/map/world_coordinate_helper.hpp>
#include <mln/style/projection_definition.hpp>
#include <mln/tile/tile_id.hpp>
#include <mln/util/crs.hpp>
#include <mln/util/tile_cover.hpp>

using namespace mln;

namespace {

void useProjection(Transform& transform, const std::string& name) {
    transform.setProjectionDefinition(ProjectionDefinition::registeredProjection(name));
}

// Identity coordinates over a square twice as wide as the world, so a center can go past 180 degrees.
CRS wideIdentity(std::string name) {
    return {.name = std::move(name),
            .forward = [](const std::array<double, 2>& lngLat) { return lngLat; },
            .inverse = [](const std::array<double, 2>& position) -> std::optional<std::array<double, 2>> {
                return position;
            },
            .tileMatrixSet = {.origin = {-360, 360}, .extentAtZoom0 = 720}};
}

} // namespace

TEST(TransformInTheIdentityProjection, PlacesTheCenterInItsSquare) {
    Transform transform;
    transform.resize({512, 512});
    useProjection(transform, "identity");
    // Tile 1/1/0 of the identity spans lng 0..90 and lat 0..90, and fills the view.
    transform.jumpTo(CameraOptions().withCenter(LatLng{45, 45}).withZoom(1));

    EXPECT_EQ(LatLng(45, 45), transform.getLatLng());
    EXPECT_EQ(ScreenCoordinate(256, 256), transform.latLngToScreenCoordinate({45, 45}));
    const LatLng topLeft = transform.screenCoordinateToLatLng({0, 0});
    EXPECT_NEAR(90, topLeft.latitude(), 1e-6);
    EXPECT_NEAR(0, topLeft.longitude(), 1e-6);
    const LatLng bottomRight = transform.screenCoordinateToLatLng({512, 512});
    EXPECT_NEAR(0, bottomRight.latitude(), 1e-6);
    EXPECT_NEAR(90, bottomRight.longitude(), 1e-6);
}

TEST(TransformInTheIdentityProjection, StopsTheCenterAtTheEdgeOfItsSquare) {
    Transform transform;
    transform.resize({512, 512});
    useProjection(transform, "identity");
    transform.jumpTo(CameraOptions().withCenter(LatLng{0, 120}).withZoom(2));

    EXPECT_EQ(LatLng(0, 90), transform.getLatLng());
}

TEST(TransformInTheIdentityProjection, LeavesTheZoomToTheMinimumZoom) {
    Transform transform;
    transform.resize({1024, 1024});
    useProjection(transform, "identity");
    transform.jumpTo(CameraOptions().withCenter(LatLng{0, 0}).withZoom(0));

    // The square is half the height of the view; Mercator would zoom in until it filled it.
    EXPECT_EQ(0, transform.getZoom());
}

TEST(TransformInTheIdentityProjection, KeepsALatitudePastTheMercatorLimit) {
    Transform transform;
    transform.resize({512, 512});
    useProjection(transform, "identity");
    transform.jumpTo(CameraOptions().withCenter(LatLng{88, 10}).withZoom(4));

    EXPECT_DOUBLE_EQ(88, transform.getLatLng().latitude());
    EXPECT_DOUBLE_EQ(10, transform.getLatLng().longitude());
}

TEST(TransformInTheIdentityProjection, KeepsTheCenterAcrossAProjectionChange) {
    Transform transform;
    transform.resize({512, 512});
    transform.jumpTo(CameraOptions().withCenter(LatLng{30, 40}).withZoom(3));
    useProjection(transform, "identity");

    EXPECT_NEAR(30, transform.getLatLng().latitude(), 1e-9);
    EXPECT_NEAR(40, transform.getLatLng().longitude(), 1e-9);

    transform.setProjectionDefinition(ProjectionDefinition(ProjectionType::Mercator));
    EXPECT_NEAR(30, transform.getLatLng().latitude(), 1e-9);
    EXPECT_NEAR(40, transform.getLatLng().longitude(), 1e-9);
}

TEST(TransformInTheIdentityProjection, KeepsTheCenterInsideMaxBounds) {
    Transform transform;
    transform.resize({512, 512});
    useProjection(transform, "identity");
    transform.setLatLngBounds(LatLngBounds::hull({0, 0}, {45, 45}));
    transform.jumpTo(CameraOptions().withCenter(LatLng{80, -60}).withZoom(3));

    EXPECT_NEAR(45, transform.getLatLng().latitude(), 1e-9);
    EXPECT_NEAR(0, transform.getLatLng().longitude(), 1e-9);
}

TEST(TransformInTheIdentityProjection, RoundTripsTheFreeCamera) {
    Transform transform;
    transform.resize({512, 512});
    useProjection(transform, "identity");
    transform.jumpTo(CameraOptions().withCenter(LatLng{20, -30}).withZoom(5).withPitch(40).withBearing(25));

    const auto camera = transform.getFreeCameraOptions();
    Transform roundTrip;
    roundTrip.resize({512, 512});
    useProjection(roundTrip, "identity");
    roundTrip.setFreeCameraOptions(camera);

    EXPECT_NEAR(20, roundTrip.getLatLng().latitude(), 1e-9);
    EXPECT_NEAR(-30, roundTrip.getLatLng().longitude(), 1e-9);
    EXPECT_NEAR(5, roundTrip.getZoom(), 1e-9);
}

TEST(TransformInTheIdentityProjection, CoversItsSquareWithoutWorldCopies) {
    Transform transform;
    transform.resize({1024, 1024});
    useProjection(transform, "identity");
    transform.jumpTo(CameraOptions().withCenter(LatLng{0, 0}).withZoom(0));

    // The view is twice as wide as the square: Mercator would add the world copies on both sides.
    EXPECT_EQ((std::vector<OverscaledTileID>{{0, 0, 0}}),
              util::tileCover({.transformState = transform.getState()}, 0, {0, 22}));
}

TEST(TransformInTheIdentityProjection, CoversTheTileThatFillsTheView) {
    Transform transform;
    transform.resize({512, 512});
    useProjection(transform, "identity");
    transform.jumpTo(CameraOptions().withCenter(LatLng{45, 45}).withZoom(1));

    EXPECT_EQ((std::vector<OverscaledTileID>{{1, 1, 0}}),
              util::tileCover({.transformState = transform.getState()}, 1, {0, 22}));
}

TEST(TransformInARegisteredProjection, FliesPastTheAntimeridianWithoutWrapping) {
    addProjection(wideIdentity("test:wide"));
    Transform transform;
    transform.resize({512, 512});
    useProjection(transform, "test:wide");
    transform.jumpTo(CameraOptions().withCenter(LatLng{0, 170}).withZoom(4));

    transform.flyTo(CameraOptions().withCenter(LatLng{0, 200}), AnimationOptions(Seconds(1)));
    transform.updateTransitions(transform.getTransitionStart() + Milliseconds(500));
    // Halfway, eastward: a wrapping world would have taken the target as -160 and gone the other way around.
    EXPECT_GT(transform.getLatLng().longitude(), 170);
    transform.updateTransitions(transform.getTransitionStart() + transform.getTransitionDuration());
    EXPECT_NEAR(200, transform.getLatLng().longitude(), 1e-9);
    removeProjection("test:wide");
}

TEST(TransformInARegisteredProjection, FallsBackToMercatorForAnUnknownName) {
    Transform transform;
    transform.resize({512, 512});
    useProjection(transform, "test:unknown");
    transform.jumpTo(CameraOptions().withCenter(LatLng{60, 10}).withZoom(3));

    EXPECT_TRUE(transform.getState().getProjection().worldCoordinateHelper().wraps());
    EXPECT_NEAR(60, transform.getLatLng().latitude(), 1e-9);
}
