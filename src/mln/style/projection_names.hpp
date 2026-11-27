#pragma once

#include <mln/style/projection_definition.hpp>

#include <optional>
#include <string_view>

namespace mln {

std::string_view projectionTypeName(ProjectionType);
std::optional<ProjectionType> projectionTypeFromName(std::string_view);

/// `identity`, or a name with a colon, which is how registered projections are named.
bool isRegisteredProjectionName(std::string_view);

/// What a name in a projection expression draws, or a transition between two names. A registered name only works as
/// a plain `projection.type`, so the globe draws it, as GL JS's globe draws any name it does not know.
std::optional<ProjectionDefinition> projectionDefinitionInExpression(std::string_view name);
std::optional<ProjectionDefinition> projectionDefinitionInExpression(std::string_view from,
                                                                     std::string_view to,
                                                                     double transition);

} // namespace mln
