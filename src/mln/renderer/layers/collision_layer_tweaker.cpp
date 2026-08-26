#include <mln/renderer/layers/collision_layer_tweaker.hpp>

#include <mln/gfx/context.hpp>
#include <mln/gfx/drawable.hpp>
#include <mln/gfx/renderable.hpp>
#include <mln/gfx/renderer_backend.hpp>
#include <mln/gfx/collision_drawable_data.hpp>
#include <mln/renderer/layer_group.hpp>
#include <mln/renderer/render_tree.hpp>
#include <mln/renderer/paint_parameters.hpp>
#include <mln/renderer/paint_property_binder.hpp>
#include <mln/shaders/collision_layer_ubo.hpp>
#include <mln/shaders/shader_program_base.hpp>
#include <mln/style/layers/symbol_layer_properties.hpp>
#include <mln/util/convert.hpp>
#include <mln/util/std.hpp>
#include <mln/util/logging.hpp>

namespace mln {

using namespace style;
using namespace shaders;

void CollisionLayerTweaker::execute(LayerGroupBase& layerGroup, const PaintParameters& parameters) {
    if (layerGroup.empty()) {
        return;
    }

    auto& context = parameters.context;

#if !defined(NDEBUG)
    const auto label = layerGroup.getName() + "-update-uniforms";
    const auto debugGroup = parameters.encoder->createDebugGroup(label.c_str());
#endif

    propertiesUpdated = false;

    visitLayerGroupDrawables(layerGroup, [&](gfx::Drawable& drawable) {
        if (!drawable.getTileID() || !drawable.getData() || !checkTweakDrawable(drawable)) {
            return;
        }

        const auto tileID = drawable.getTileID()->toUnwrapped();
        const auto& data = static_cast<gfx::CollisionDrawableData&>(*drawable.getData());

        // matrix
        const auto translate = data.translate;
        const auto anchor = data.translateAnchor;
        constexpr bool nearClipped = false;
        constexpr bool inViewportPixelUnits = false;
        const auto matrix = getTileMatrix(
            tileID, parameters, translate, anchor, nearClipped, inViewportPixelUnits, drawable);

        // The collision index measures boxes and circles in viewport pixels, so the shaders
        // only need the pixel size of the viewport to place them.
        const auto size = parameters.state.getSize();
        const std::array<float, 2> pixelExtrudeScale = {
            {1.0f / static_cast<float>(size.width), 1.0f / static_cast<float>(size.height)}};

        const CollisionDrawableUBO drawableUBO = {/* .matrix = */ util::cast<float>(matrix)};

        const CollisionTilePropsUBO tilePropsUBO = {.pixel_extrude_scale = pixelExtrudeScale, .pad1 = 0, .pad2 = 0};

        auto& drawableUniforms = drawable.mutableUniformBuffers();
        drawableUniforms.createOrUpdate(idCollisionDrawableUBO, &drawableUBO, context);
        drawableUniforms.createOrUpdate(idCollisionTilePropsUBO, &tilePropsUBO, context);
    });
}

} // namespace mln
