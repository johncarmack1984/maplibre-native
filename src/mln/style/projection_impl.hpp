#pragma once

#include <mln/style/projection.hpp>

#include <memory>

namespace mln {

class CRSWorldCoordinateHelper;

namespace style {

class Projection::Impl {
public:
    PropertyValue<ProjectionDefinition> type;
    /// The projection registered with `addProjection`, or the built-in `identity`, that the style names as its plain
    /// type, or empty. It draws flat, as Mercator does, and `type` is then undefined.
    std::string registeredName;
    /// The projection registered under that name when the type was set, or null when none was. The map, its sources
    /// and annotations draw in this one, so removing the name from the registry leaves them as they are.
    std::shared_ptr<const CRSWorldCoordinateHelper> registeredProjection;
};

} // namespace style
} // namespace mln
