#include <mln/map/world_coordinate_helper.hpp>
#include <mln/style/sources/geojson_source_impl.hpp>
#include <mln/tile/tile_id.hpp>
#include <mln/util/constants.hpp>
#include <mln/util/feature.hpp>
#include <mln/util/geojson_reproject.hpp>
#include <mln/util/string.hpp>
#include <mln/util/thread_pool.hpp>
#include <mln/util/identity.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

#include <mapbox/geojsonvt.hpp>
#include <supercluster.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <cmath>

namespace mln {
namespace style {

class GeoJSONVTData final : public GeoJSONData {
    void getTile(const CanonicalTileID& id, const std::function<void(TileFeatures)>& fn, bool runSynchronously) final {
        assert(fn);
        if (runSynchronously) {
            fn(this->impl->getTile(id.z, id.x, id.y).features);
        } else {
            sequencedScheduler->scheduleAndReplyValue(
                util::SimpleIdentity::Empty,
                [id, geoJSONVT_impl = this->impl]() -> TileFeatures {
                    return geoJSONVT_impl->getTile(id.z, id.x, id.y).features;
                },
                fn);
        }
    }

    Features getChildren(const std::uint32_t) final { return {}; }

    Features getLeaves(const std::uint32_t, const std::uint32_t, const std::uint32_t) final { return {}; }

    std::uint8_t getClusterExpansionZoom(std::uint32_t) final { return 0; }

    friend GeoJSONData;
    GeoJSONVTData(const GeoJSON& geoJSON,
                  const mapbox::geojsonvt::Options& options,
                  std::shared_ptr<Scheduler> sequencedScheduler_)
        : impl(std::make_shared<mapbox::geojsonvt::GeoJSONVT>(geoJSON, options)),
          sequencedScheduler(std::move(sequencedScheduler_)) {
        assert(sequencedScheduler);
    }

    std::shared_ptr<mapbox::geojsonvt::GeoJSONVT> impl; // Accessed on worker thread.
    std::shared_ptr<Scheduler> sequencedScheduler;
};

class SuperclusterData final : public GeoJSONData {
    void getTile(const CanonicalTileID& id, const std::function<void(TileFeatures)>& fn, bool) final {
        assert(fn);
        fn(impl.getTile(id.z, id.x, id.y));
    }

    Features getChildren(const std::uint32_t cluster_id) final { return toLatLng(impl.getChildren(cluster_id)); }

    Features getLeaves(const std::uint32_t cluster_id, const std::uint32_t limit, const std::uint32_t offset) final {
        return toLatLng(impl.getLeaves(cluster_id, limit, offset));
    }

    std::uint8_t getClusterExpansionZoom(std::uint32_t cluster_id) final {
        return impl.getClusterExpansionZoom(cluster_id);
    }

    friend GeoJSONData;
    SuperclusterData(const Features& features,
                     const mapbox::supercluster::Options& options,
                     std::shared_ptr<const WorldCoordinateHelper> helper_)
        : impl(features, options),
          helper(std::move(helper_)) {}

    Features toLatLng(Features features) const {
        return helper ? reprojectFeaturesFromWorld(features, *helper) : features;
    }

    mapbox::supercluster::Supercluster impl;
    std::shared_ptr<const WorldCoordinateHelper> helper;
};

template <class T>
T evaluateFeature(const mapbox::feature::feature<double>& f,
                  const std::shared_ptr<expression::Expression>& expression,
                  std::optional<T> accumulated = std::nullopt) {
    const expression::EvaluationResult result = expression->evaluate(accumulated, f);
    if (result) {
        std::optional<T> typed = expression::fromExpressionValue<T>(*result);
        if (typed) {
            return std::move(*typed);
        }
    }
    return T();
}

// static
std::shared_ptr<GeoJSONData> GeoJSONData::create(const GeoJSON& geoJSON,
                                                 std::shared_ptr<Scheduler> sequencedScheduler,
                                                 const Immutable<GeoJSONOptions>& options,
                                                 const std::string& projection) {
    std::shared_ptr<const CRSWorldCoordinateHelper> helper;
    if (auto crs = projection.empty() ? nullptr : getRegisteredProjection(projection)) {
        helper = std::make_shared<const CRSWorldCoordinateHelper>(std::move(crs));
    }
    const GeoJSON reprojected = helper ? reprojectGeoJSONToWorld(geoJSON, *helper) : GeoJSON{};
    const GeoJSON& data = helper ? reprojected : geoJSON;
    const auto inProjection = [&](GeoJSONData* created) {
        if (helper) {
            created->projection = projection;
        }
        return std::shared_ptr<GeoJSONData>(created);
    };

    constexpr double scale = util::EXTENT / util::tileSize_D;
    if (options->cluster && data.is<Features>() && !data.get<Features>().empty()) {
        mapbox::supercluster::Options clusterOptions;
        clusterOptions.maxZoom = options->clusterMaxZoom;
        clusterOptions.extent = util::EXTENT;
        clusterOptions.radius = static_cast<uint16_t>(::round(scale * options->clusterRadius));
        clusterOptions.minPoints = options->clusterMinPoints;

        auto feature = std::make_shared<Feature>();
        clusterOptions.map = [feature, options](const PropertyMap& properties) -> PropertyMap {
            PropertyMap ret{};
            if (properties.empty()) return ret;
            for (const auto& p : options->clusterProperties) {
                feature->properties = properties;
                ret[p.first] = evaluateFeature<Value>(*feature, p.second.first);
            }
            return ret;
        };
        clusterOptions.reduce = [feature, options](PropertyMap& toReturn, const PropertyMap& toFill) {
            for (const auto& p : options->clusterProperties) {
                if (!toFill.contains(p.first)) {
                    continue;
                }
                feature->properties = toFill;
                std::optional<Value> accumulated(toReturn[p.first]);
                toReturn[p.first] = evaluateFeature<Value>(*feature, p.second.second, accumulated);
            }
        };
        if (helper) {
            clusterOptions.coordinates = mapbox::supercluster::Coordinates::UnitSquare;
            clusterOptions.wrap = false;
        }
        return inProjection(new SuperclusterData(data.get<Features>(), clusterOptions, helper));
    }

    mapbox::geojsonvt::Options vtOptions;
    vtOptions.maxZoom = options->maxzoom;
    vtOptions.extent = util::EXTENT;
    vtOptions.buffer = static_cast<uint16_t>(::round(scale * options->buffer));
    vtOptions.tolerance = scale * options->tolerance;
    vtOptions.lineMetrics = options->lineMetrics;
    if (helper) {
        vtOptions.coordinates = mapbox::geojsonvt::Coordinates::UnitSquare;
        vtOptions.wrap = false;
    }
    return inProjection(new GeoJSONVTData(data, vtOptions, std::move(sequencedScheduler)));
}

GeoJSONSource::Impl::Impl(std::string id_, Immutable<GeoJSONOptions> options_)
    : Source::Impl(SourceType::GeoJSON, std::move(id_)),
      options(std::move(options_)) {}

GeoJSONSource::Impl::Impl(const GeoJSONSource::Impl& other, std::shared_ptr<GeoJSONData> data_)
    : Source::Impl(other),
      options(other.options),
      data(std::move(data_)),
      overrideSynchronousUpdate(other.overrideSynchronousUpdate) {}

GeoJSONSource::Impl::~Impl() = default;

Range<uint8_t> GeoJSONSource::Impl::getZoomRange() const {
    return {options->minzoom, options->maxzoom};
}

std::weak_ptr<GeoJSONData> GeoJSONSource::Impl::getData() const {
    return data;
}

std::optional<std::string> GeoJSONSource::Impl::getAttribution() const {
    return {};
}

bool GeoJSONSource::Impl::isUpdateSynchronous() const {
    return options->synchronousUpdate || overrideSynchronousUpdate;
}

void GeoJSONSource::Impl::setOverrideSynchronousUpdate(bool newOverride) const {
    overrideSynchronousUpdate = newOverride;
}

} // namespace style
} // namespace mln
