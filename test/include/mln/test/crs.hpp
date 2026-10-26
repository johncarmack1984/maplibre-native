#pragma once

#include <mln/util/crs.hpp>

#include <cmath>
#include <numbers>
#include <string>

namespace mln {
namespace test {

/// A north polar stereographic projection of a sphere, with a tile matrix set the size of EPSG:3413's.
inline CRS northPolarStereographic(std::string name) {
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

/// Identity coordinates over a square twice as wide as the world, so a center can go past 180 degrees.
inline CRS wideIdentity(std::string name) {
    return {.name = std::move(name),
            .forward = [](const std::array<double, 2>& lngLat) { return lngLat; },
            .inverse = [](const std::array<double, 2>& position) -> std::optional<std::array<double, 2>> {
                return position;
            },
            .tileMatrixSet = {.origin = {-360, 360}, .extentAtZoom0 = 720}};
}

/// The identity, with an inverse that has no answer outside the disc inscribed in its tile matrix set.
inline CRS disc(std::string name) {
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

} // namespace test
} // namespace mln
