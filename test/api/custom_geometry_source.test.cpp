#include <mln/test/util.hpp>

#include <mln/map/map.hpp>
#include <mln/map/map_options.hpp>
#include <mln/gfx/headless_frontend.hpp>
#include <mln/renderer/renderer.hpp>
#include <mln/storage/resource_options.hpp>
#include <mln/style/style.hpp>
#include <mln/style/sources/custom_geometry_source.hpp>
#include <mln/style/layers/fill_layer.hpp>
#include <mln/style/layers/line_layer.hpp>
#include <mln/util/geojson.hpp>
#include <mln/util/io.hpp>
#include <mln/util/mat4.hpp>
#include <mln/util/run_loop.hpp>

using namespace mln;
using namespace mln::style;

TEST(CustomGeometrySource, Grid) {
    util::RunLoop loop;

    HeadlessFrontend frontend{1};
    Map map(frontend,
            MapObserver::nullObserver(),
            MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize()),
            ResourceOptions().withCachePath(":memory:").withAssetPath("test/fixtures/api/assets"));
    map.getStyle().loadJSON(util::read_file("test/fixtures/api/water.json"));
    map.jumpTo(CameraOptions().withCenter(LatLng{37.8, -122.5}).withZoom(10.0));

    CustomGeometrySource::Options options;
    options.fetchTileFunction = [&map](const mln::CanonicalTileID& tileID) {
        double gridSpacing = 0.1;
        FeatureCollection features;
        const LatLngBounds bounds(tileID);
        for (double y = ceil(bounds.north() / gridSpacing) * gridSpacing;
             y >= floor(bounds.south() / gridSpacing) * gridSpacing;
             y -= gridSpacing) {
            mapbox::geojson::line_string gridLine;
            gridLine.emplace_back(bounds.west(), y);
            gridLine.emplace_back(bounds.east(), y);

            features.emplace_back(gridLine);
        }

        for (double x = floor(bounds.west() / gridSpacing) * gridSpacing;
             x <= ceil(bounds.east() / gridSpacing) * gridSpacing;
             x += gridSpacing) {
            mapbox::geojson::line_string gridLine;
            gridLine.emplace_back(x, bounds.south());
            gridLine.emplace_back(x, bounds.north());

            features.emplace_back(gridLine);
        }
        auto source = static_cast<CustomGeometrySource*>(map.getStyle().getSource("custom"));
        if (source) {
            source->setTileData(tileID, features);
        }
    };

    map.getStyle().addSource(std::make_unique<CustomGeometrySource>("custom", options));

    auto fillLayer = std::make_unique<FillLayer>("landcover", "mapbox");
    fillLayer->setSourceLayer("landcover");
    fillLayer->setFillColor(Color{1.0, 1.0, 0.0, 1.0});
    map.getStyle().addLayer(std::move(fillLayer));

    auto layer = std::make_unique<LineLayer>("grid", "custom");
    layer->setLineColor(Color{1.0, 1.0, 1.0, 1.0});
    map.getStyle().addLayer(std::move(layer));

    test::checkImage("test/fixtures/custom_geometry_source/grid", frontend.render(map).image, 0.0006, 0.1);
}

TEST(CustomGeometrySource, InARegisteredProjection) {
    util::RunLoop loop;

    HeadlessFrontend frontend{1};
    Map map(frontend,
            MapObserver::nullObserver(),
            MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize()),
            ResourceOptions().withCachePath(":memory:").withAssetPath("test/fixtures/api/assets"));
    map.getStyle().loadJSON(R"({"version": 8, "projection": {"type": "identity"}, "sources": {}, "layers": []})");

    CustomGeometrySource::Options options;
    options.fetchTileFunction = [&map](const CanonicalTileID& tileID) {
        // Every tile gets the box from 0 to 20 E and 20 to 40 N, in lat/lng as the source's data always is.
        const mapbox::geojson::polygon box{{{0, 20}, {20, 20}, {20, 40}, {0, 40}, {0, 20}}};
        if (auto* source = static_cast<CustomGeometrySource*>(map.getStyle().getSource("custom"))) {
            source->setTileData(tileID, FeatureCollection{Feature{box}});
        }
    };
    options.tileOptions.clip = true;
    map.getStyle().addSource(std::make_unique<CustomGeometrySource>("custom", options));

    auto layer = std::make_unique<FillLayer>("box", "custom");
    layer->setFillColor(Color::red());
    map.getStyle().addLayer(std::move(layer));

    frontend.render(map);

    // 10 E, 30 N is inside the box in the identity projection; in Mercator the whole box is drawn below that point.
    EXPECT_EQ(frontend.getRenderer()->queryRenderedFeatures(map.pixelForLatLng({30, 10})).size(), 1u);
}
