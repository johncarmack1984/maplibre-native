#pragma once

#include <mln/util/feature.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mln {

/// The names the style's `projection.type` accepts.
enum class ProjectionType : uint8_t {
    Mercator,
    VerticalPerspective,
    /// The vertical perspective that hands over to Mercator between zoom 11 and 12.
    Globe,
};

std::string_view projectionTypeName(ProjectionType);
std::optional<ProjectionType> projectionTypeFromName(std::string_view);

/// `identity`, or a name with a colon, which is how registered projections are named.
bool isRegisteredProjectionName(std::string_view);

/// The value of the style's `projection.type`: a single projection, or a transition between two.
struct ProjectionDefinition {
    ProjectionDefinition() = default;
    explicit ProjectionDefinition(ProjectionType type);
    ProjectionDefinition(ProjectionType from_, ProjectionType to_, double transition_);
    /// From a projection name; asserts the name is one of `projectionTypeName`'s.
    explicit ProjectionDefinition(std::string_view name);
    ProjectionDefinition(std::string_view from_, std::string_view to_, double transition_);

    ProjectionType from = ProjectionType::Mercator;
    ProjectionType to = ProjectionType::Mercator;
    double transition = 1;

    bool operator==(const ProjectionDefinition&) const = default;

    /// How far the definition is toward the globe: 0 is Mercator, 1 the globe (GL JS
    /// `GlobeProjection.transitionState`).
    double transitionState() const;

    mln::Value serialize() const;
};

/// What a name in a projection expression draws, or a transition between two names. A registered name only works as
/// a plain `projection.type`, so the globe draws it, as GL JS's globe draws any name it does not know.
std::optional<ProjectionDefinition> projectionDefinitionInExpression(std::string_view name);
std::optional<ProjectionDefinition> projectionDefinitionInExpression(std::string_view from,
                                                                     std::string_view to,
                                                                     double transition);

} // namespace mln
