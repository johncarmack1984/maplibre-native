#include <mln/style/projection.hpp>
#include <mln/style/projection_impl.hpp>
#include <mln/style/projection_observer.hpp>
#include <mln/style/conversion/property_value.hpp>
#include <mln/style/conversion_impl.hpp>
#include <mln/map/world_coordinate_helper.hpp>
#include <mln/renderer/property_evaluator.hpp>
#include <mln/util/logging.hpp>

namespace mln {
namespace style {

namespace {
ProjectionObserver nullObserver;

void setTypeAndName(Projection& projection, PropertyValue<ProjectionDefinition> type, std::string registeredName) {
    auto impl = projection.mutableImpl();
    impl->type = std::move(type);
    impl->registeredName = std::move(registeredName);
    projection.impl = std::move(impl);
    projection.observer->onProjectionChanged(projection);
}

/// The first registered projection a projection expression names anywhere in its arrays, as GL JS looks for one.
std::optional<std::string> registeredProjectionIn(const conversion::Convertible& value) {
    if (auto name = toString(value)) {
        if (getRegisteredProjection(*name)) {
            return name;
        }
        return std::nullopt;
    }
    if (isArray(value)) {
        for (std::size_t i = 0; i < arrayLength(value); ++i) {
            if (auto name = registeredProjectionIn(arrayMember(value, i))) {
                return name;
            }
        }
    }
    return std::nullopt;
}

} // namespace

Projection::Projection(Immutable<Projection::Impl> impl_)
    : impl(std::move(impl_)),
      observer(&nullObserver) {}

Projection::Projection()
    : Projection(makeMutable<Impl>()) {}

Projection::~Projection() = default;

void Projection::setObserver(ProjectionObserver* observer_) {
    observer = observer_ ? observer_ : &nullObserver;
}

Mutable<Projection::Impl> Projection::mutableImpl() const {
    return makeMutable<Impl>(*impl);
}

ProjectionDefinition Projection::getDefaultType() {
    return ProjectionDefinition();
}

PropertyValue<ProjectionDefinition> Projection::getType() const {
    return impl->type;
}

void Projection::setType(PropertyValue<ProjectionDefinition> type) {
    setTypeAndName(*this, std::move(type), {});
}

std::optional<conversion::Error> Projection::setProperty(const std::string& name,
                                                         const conversion::Convertible& value) {
    if (name != "type") {
        return conversion::Error{"projection doesn't support this property"};
    }
    if (const auto registeredName = toString(value); registeredName && isRegisteredProjectionName(*registeredName)) {
        setTypeAndName(*this, {}, *registeredName);
        return std::nullopt;
    }
    conversion::Error error;
    const auto type = conversion::convert<PropertyValue<ProjectionDefinition>>(value, error, false, false);
    if (!type) {
        return error;
    }
    if (const auto registeredName = registeredProjectionIn(value)) {
        Log::Warning(Event::ParseStyle,
                     "Projection expressions cannot use the registered projection \"" + *registeredName +
                         "\", which only works as a plain name. Falling back to the globe projection.");
    }
    setType(*type);
    return std::nullopt;
}

StyleProperty Projection::getProperty(const std::string& name) const {
    if (name == "type") {
        if (!impl->registeredName.empty()) {
            return {impl->registeredName, StyleProperty::Kind::Constant};
        }
        return conversion::makeStyleProperty(getType());
    }
    return {};
}

ProjectionDefinition Projection::Impl::evaluate(float zoom) const {
    const PropertyEvaluationParameters parameters(zoom);
    return type.evaluate(PropertyEvaluator<ProjectionDefinition>(parameters, Projection::getDefaultType()));
}

} // namespace style
} // namespace mln
