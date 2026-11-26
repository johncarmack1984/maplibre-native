#pragma once

#include <mln/style/projection.hpp>

namespace mln {
namespace style {

class Projection::Impl {
public:
    PropertyValue<ProjectionDefinition> type;

    /// The definition at a zoom level; `mercator` when the style does not set one.
    ProjectionDefinition evaluate(float zoom) const;

    /// The registered projection the style names as its plain type, or empty for the built-in ones.
    std::string registeredName() const;
};

} // namespace style
} // namespace mln
