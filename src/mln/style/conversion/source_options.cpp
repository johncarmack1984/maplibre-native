#include <mln/style/conversion/source_options.hpp>
#include <mln/style/conversion_impl.hpp>
#include <mln/style/expression/dsl.hpp>

namespace mln {
namespace style {
namespace conversion {

std::optional<SourceOptions> Converter<SourceOptions>::operator()(const Convertible& value, Error& error) const {
    const auto encodingValue = objectMember(value, "encoding");
    if (encodingValue) {
        const auto encoding = toString(*encodingValue);
        if (encoding && *encoding == "terrarium") {
            return {{.rasterEncoding = Tileset::RasterEncoding::Terrarium}};
        } else if (encoding && *encoding == "mapbox") {
            return {{.rasterEncoding = Tileset::RasterEncoding::Mapbox}};
        } else if (encoding && *encoding == "custom") {
            std::array<float, 4> factors{1, 1, 1, 0};
            const std::array<const char*, 4> names{"redFactor", "greenFactor", "blueFactor", "baseShift"};
            for (size_t i = 0; i < names.size(); ++i) {
                if (const auto factorValue = objectMember(value, names[i])) {
                    const auto factor = toNumber(*factorValue);
                    if (!factor) {
                        error.message = std::string(names[i]) + " must be a number";
                        return std::nullopt;
                    }
                    factors[i] = *factor;
                }
            }
            return {{.rasterEncoding = Tileset::RasterEncoding::Custom, .rasterCustomFactors = factors}};
        } else if (encoding && *encoding == "mvt") {
            return {{.vectorEncoding = Tileset::VectorEncoding::Mapbox}};
        } else if (encoding && *encoding == "mlt") {
            return {{.vectorEncoding = Tileset::VectorEncoding::MLT}};
        } else {
            error.message =
                "invalid encoding - valid types are 'mapbox', 'terrarium' and 'custom' for raster sources, "
                "'mvt' and 'mlt' for vector sources";
            return std::nullopt;
        }
    }
    return {};
}

} // namespace conversion
} // namespace style
} // namespace mln
