#include <mln/test/util.hpp>

#include <mln/style/conversion/json.hpp>
#include <mln/style/conversion/source_options.hpp>
#include <mln/util/tileset.hpp>

#include <mln/util/logging.hpp>

using namespace mln;
using namespace mln::style;
using namespace mln::style::conversion;

TEST(SourceOptions, Basic) {
    Error error;
    auto converted = convertJSON<SourceOptions>("{}", error);
    ASSERT_FALSE(converted);
}

TEST(SourceOptions, ErrorHandling) {
    Error error;
    auto converted = convertJSON<SourceOptions>(
        R"JSON({
        "encoding": "this isn't a valid encoding"
    })JSON",
        error);
    ASSERT_FALSE(converted);
    ASSERT_EQ(0, error.message.find("invalid encoding"));
}

TEST(SourceOptions, TerrariumEncodingParsed) {
    Error error;
    auto converted = convertJSON<SourceOptions>(
        R"JSON({
        "encoding": "terrarium"
    })JSON",
        error);
    ASSERT_EQ(converted.value().rasterEncoding, Tileset::RasterEncoding::Terrarium);
    ASSERT_FALSE(converted.value().vectorEncoding);
}

TEST(SourceOptions, MapboxEncodingParsed) {
    Error error;
    auto converted = convertJSON<SourceOptions>(
        R"JSON({
        "encoding": "mapbox"
    })JSON",
        error);
    ASSERT_EQ(converted.value().rasterEncoding, Tileset::RasterEncoding::Mapbox);
    ASSERT_FALSE(converted.value().vectorEncoding);
}

TEST(SourceOptions, CustomEncodingParsed) {
    Error error;
    auto converted = convertJSON<SourceOptions>(
        R"JSON({
        "encoding": "custom",
        "redFactor": 0,
        "greenFactor": 25.6,
        "blueFactor": 0.1,
        "baseShift": 3446.4
    })JSON",
        error);
    ASSERT_EQ(converted.value().rasterEncoding, Tileset::RasterEncoding::Custom);
    EXPECT_EQ(converted.value().rasterCustomFactors, (std::array<float, 4>{0, 25.6f, 0.1f, 3446.4f}));
}

TEST(SourceOptions, CustomEncodingDefaultFactors) {
    Error error;
    auto converted = convertJSON<SourceOptions>(
        R"JSON({
        "encoding": "custom"
    })JSON",
        error);
    ASSERT_EQ(converted.value().rasterEncoding, Tileset::RasterEncoding::Custom);
    EXPECT_EQ(converted.value().rasterCustomFactors, (std::array<float, 4>{1, 1, 1, 0}));
}

TEST(SourceOptions, CustomEncodingFactorNotANumber) {
    Error error;
    auto converted = convertJSON<SourceOptions>(
        R"JSON({
        "encoding": "custom",
        "greenFactor": "25.6"
    })JSON",
        error);
    ASSERT_FALSE(converted);
    EXPECT_EQ("greenFactor must be a number", error.message);
}

TEST(SourceOptions, MVTEncodingParsed) {
    Error error;
    auto converted = convertJSON<SourceOptions>(
        R"JSON({
        "encoding": "mvt"
    })JSON",
        error);
    ASSERT_EQ(converted.value().vectorEncoding, Tileset::VectorEncoding::Mapbox);
    ASSERT_FALSE(converted.value().rasterEncoding);
}

TEST(SourceOptions, MLTEncodingParsed) {
    Error error;
    auto converted = convertJSON<SourceOptions>(
        R"JSON({
        "encoding": "mlt"
    })JSON",
        error);
    ASSERT_EQ(converted.value().vectorEncoding, Tileset::VectorEncoding::MLT);
    ASSERT_FALSE(converted.value().rasterEncoding);
}
