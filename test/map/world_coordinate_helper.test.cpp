#include <mln/test/util.hpp>

#include <mln/map/world_coordinate_helper.hpp>
#include <mln/util/crs.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>

using namespace mln;

namespace {

// A north polar stereographic projection of a sphere, with a tile matrix set the size of EPSG:3413's.
CRS northPolarStereographic(std::string name) {
    constexpr double radius = 6378137;
    return {.name = std::move(name),
            .forward =
                [](const std::array<double, 2>& lngLat) {
                    const double lambda = lngLat[0] * std::numbers::pi / 180;
                    const double rho = 2 * radius * std::tan(std::numbers::pi / 4 - lngLat[1] * std::numbers::pi / 360);
                    return std::array<double, 2>{rho * std::sin(lambda), -rho * std::cos(lambda)};
                },
            .inverse = [](const std::array<double, 2>& position) -> std::optional<std::array<double, 2>> {
                const double rho = std::hypot(position[0], position[1]);
                const double latitude = 90 - std::atan(rho / (2 * radius)) * 360 / std::numbers::pi;
                return std::array<double, 2>{std::atan2(position[0], -position[1]) * 180 / std::numbers::pi, latitude};
            },
            .tileMatrixSet = {.origin = {-4194304, 4194304}, .extentAtZoom0 = 8388608}};
}

// The identity, with an inverse that has no answer outside the disc inscribed in its tile matrix set.
CRS disc(std::string name) {
    return {.name = std::move(name),
            .forward = [](const std::array<double, 2>& lngLat) { return lngLat; },
            .inverse = [](const std::array<double, 2>& position) -> std::optional<std::array<double, 2>> {
                if (std::hypot(position[0], position[1]) > 90) {
                    return std::nullopt;
                }
                return position;
            },
            .tileMatrixSet = {.origin = {-90, 90}, .extentAtZoom0 = 180}};
}

} // namespace

TEST(CRSWorldCoordinateHelper, Identity) {
    const CRSWorldCoordinateHelper helper(getRegisteredProjection("identity"));
    EXPECT_EQ("identity", helper.name());
    EXPECT_FALSE(helper.wraps());
    EXPECT_EQ(Point<double>(0.75, 0.25), helper.worldFromLatLng({45, 45}));
    EXPECT_EQ(LatLng(45, 45), helper.latLngFromWorld({0.75, 0.25}));
    EXPECT_EQ(180, helper.metersPerWorldUnit({0.5, 0.5}));
    EXPECT_EQ(0.5, helper.worldZFromAltitude(90, {0, 0}));
}

TEST(CRSWorldCoordinateHelper, LatLngPastTheEdgeOfTheSquareIsUnclamped) {
    const CRSWorldCoordinateHelper helper(getRegisteredProjection("identity"));
    // The identity has an answer everywhere: a position east of the square keeps its own longitude.
    EXPECT_EQ(LatLng(0, 135), helper.latLngFromWorld({1.25, 0.5}));
    EXPECT_TRUE(helper.hasLatLng({1.25, 0.5}));
    EXPECT_EQ(LatLng(90, 0), helper.latLngFromWorld({0.5, -0.5}));
}

TEST(CRSWorldCoordinateHelper, PositionWithoutAnAnswerUsesTheEdgeTowardTheCenter) {
    const CRSWorldCoordinateHelper helper(std::make_shared<const CRS>(disc("test:disc")));
    EXPECT_FALSE(helper.hasLatLng({1, 1}));
    EXPECT_TRUE(helper.hasLatLng({0.5, 0.5}));
    // The corner of the square has no answer, nor any position past it; the last one with an answer on the line to the
    // center is on the disc.
    const LatLng edge = helper.latLngFromWorld({2, 2});
    EXPECT_NEAR(90, std::hypot(edge.longitude(), edge.latitude()), 1e-6);
    EXPECT_NEAR(-edge.latitude(), edge.longitude(), 1e-9);
    // A position past the square whose clamped one has an answer uses that one.
    EXPECT_EQ(LatLng(0, 90), helper.latLngFromWorld({1.5, 0.5}));
}

TEST(CRSWorldCoordinateHelper, ThrowsWhereTheInverseNeverAnswers) {
    CRS crs = disc("test:never");
    crs.inverse = [](const std::array<double, 2>&) -> std::optional<std::array<double, 2>> {
        return std::nullopt;
    };
    const CRSWorldCoordinateHelper helper(std::make_shared<const CRS>(std::move(crs)));
    EXPECT_THROW(helper.latLngFromWorld({0.5, 0.5}), std::runtime_error);
}

TEST(CRSWorldCoordinateHelper, LatitudeRangeOfAPolarProjection) {
    const CRSWorldCoordinateHelper helper(std::make_shared<const CRS>(northPolarStereographic("test:polar")));
    // The opposite pole projects to infinity: the range ends where the meridian stretches ten times as much as at the
    // north pole.
    EXPECT_EQ((std::array<double, 2>{-53, 90}), helper.latitudeRange());
}

TEST(CRSWorldCoordinateHelper, LatitudeRangeOfTheIdentity) {
    const CRSWorldCoordinateHelper helper(getRegisteredProjection("identity"));
    EXPECT_EQ((std::array<double, 2>{-90, 90}), helper.latitudeRange());
}

TEST(AddProjection, NeedsAColonInTheName) {
    EXPECT_THROW(addProjection(disc("disc")), std::invalid_argument);
    EXPECT_FALSE(getRegisteredProjection("disc"));
}

TEST(AddProjection, RejectsATakenName) {
    addProjection(disc("test:taken"));
    EXPECT_THROW(addProjection(disc("test:taken")), std::invalid_argument);
    removeProjection("test:taken");
}

TEST(AddProjection, NeedsAPositiveExtent) {
    CRS crs = disc("test:empty");
    crs.tileMatrixSet.extentAtZoom0 = 0;
    EXPECT_THROW(addProjection(std::move(crs)), std::invalid_argument);
    EXPECT_FALSE(getRegisteredProjection("test:empty"));
}

TEST(AddProjection, NeedsForwardAndInverse) {
    CRS crs = disc("test:forward-only");
    crs.inverse = nullptr;
    EXPECT_THROW(addProjection(std::move(crs)), std::invalid_argument);
}

TEST(RemoveProjection, RemovesARegisteredProjection) {
    addProjection(disc("test:removed"));
    ASSERT_TRUE(getRegisteredProjection("test:removed"));
    removeProjection("test:removed");
    EXPECT_FALSE(getRegisteredProjection("test:removed"));
}

TEST(RemoveProjection, CannotRemoveTheIdentity) {
    EXPECT_THROW(removeProjection("identity"), std::invalid_argument);
    EXPECT_TRUE(getRegisteredProjection("identity"));
}
