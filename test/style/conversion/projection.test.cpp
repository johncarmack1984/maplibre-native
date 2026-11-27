#include <mln/test/crs.hpp>
#include <mln/test/fixture_log_observer.hpp>
#include <mln/test/util.hpp>

#include <mln/style/conversion/json.hpp>
#include <mln/style/conversion/projection.hpp>
#include <mln/style/conversion_impl.hpp>
#include <mln/style/projection_impl.hpp>
#include <mln/style/rapidjson_conversion.hpp>
#include <mln/util/rapidjson.hpp>

using namespace mln;
using namespace mln::style;
using namespace mln::style::conversion;

TEST(StyleConversion, RegisteredProjection) {
    FixtureLog log;
    Error error;
    auto parseProjection = [&](const std::string& src) {
        return convertJSON<Projection>(src, error);
    };

    for (const std::string name : {"identity", "EPSG:3413"}) {
        auto projection = parseProjection(R"({"type":")" + name + R"("})");
        ASSERT_TRUE(projection) << error.message;
        EXPECT_EQ(name, projection->impl->registeredName);
        EXPECT_TRUE(projection->getType().isUndefined());
        EXPECT_EQ(StyleProperty::Kind::Constant, projection->getProperty("type").getKind());
        EXPECT_EQ(mln::Value(name), projection->getProperty("type").getValue());

        projection->setType(ProjectionDefinition(ProjectionType::Mercator));
        EXPECT_TRUE(projection->impl->registeredName.empty());
    }
    EXPECT_EQ(1u,
              log.count({EventSeverity::Warning,
                         Event::General,
                         -1,
                         "Unknown projection name \"EPSG:3413\". Falling back to mercator projection."}));

    EXPECT_FALSE(parseProjection(R"({"type":"equal-earth"})"));
}

TEST(StyleConversion, RegisteredProjectionInAnExpressionFallsBackToTheGlobe) {
    FixtureLog log;
    Error error;
    auto parseProjection = [&](const std::string& src) {
        return convertJSON<Projection>(src, error);
    };
    const auto warning = [](const std::string& name) {
        return FixtureLog::Message{EventSeverity::Warning,
                                   Event::ParseStyle,
                                   -1,
                                   "Projection expressions cannot use the registered projection \"" + name +
                                       "\", which only works as a plain name. Falling back to the globe projection."};
    };

    // As in GL JS, the globe draws the expression, and reads a registered name in it as it reads any name but mercator.
    {
        auto projection = parseProjection(R"({"type":["step",["zoom"],"identity",10,"mercator"]})");
        ASSERT_TRUE(projection) << error.message;
        EXPECT_EQ(1u, log.count(warning("identity")));
        EXPECT_EQ(ProjectionDefinition(ProjectionType::VerticalPerspective),
                  projection->getType().asExpression().evaluate(5.f));
        EXPECT_EQ(ProjectionDefinition(ProjectionType::Mercator), projection->getType().asExpression().evaluate(10.f));
    }

    const test::ScopedProjection registration(test::polarStereographic("test:in-a-transition"));
    {
        auto projection = parseProjection(R"({"type":["mercator","test:in-a-transition",0.25]})");
        ASSERT_TRUE(projection) << error.message;
        EXPECT_EQ(1u, log.count(warning("test:in-a-transition")));
        // GL JS's globe takes a transition with a name it does not know as the whole globe.
        EXPECT_EQ(ProjectionDefinition(ProjectionType::VerticalPerspective), projection->getType().asConstant());
    }

    // GL JS warns only for a name that is registered.
    {
        auto projection = parseProjection(R"({"type":["mercator","test:not-registered",0.25]})");
        ASSERT_TRUE(projection) << error.message;
        EXPECT_EQ(ProjectionDefinition(ProjectionType::VerticalPerspective), projection->getType().asConstant());
    }
    EXPECT_EQ(0u, log.uncheckedCount()) << log.unchecked();
}

TEST(StyleConversion, Projection) {
    Error error;

    auto parseProjection = [&](const std::string& src) {
        return convertJSON<Projection>(src, error);
    };

    {
        auto projection = parseProjection("{}");
        ASSERT_TRUE(projection);
        ASSERT_TRUE(projection->getType().isUndefined());
    }

    {
        auto projection = parseProjection(R"({"type":"globe"})");
        ASSERT_TRUE(projection);
        ASSERT_TRUE(projection->getType().isConstant());
        ASSERT_EQ(ProjectionDefinition("globe"), projection->getType().asConstant());
        // The preset is the vertical perspective until zoom 11, Mercator from zoom 12, and blends in between.
        ASSERT_EQ(ProjectionDefinition("vertical-perspective"), projection->impl->evaluate(3.f));
        ASSERT_EQ(ProjectionDefinition("vertical-perspective"), projection->impl->evaluate(11.f));
        ASSERT_EQ(ProjectionDefinition("vertical-perspective", "mercator", 0.5), projection->impl->evaluate(11.5f));
        ASSERT_EQ(ProjectionDefinition("mercator"), projection->impl->evaluate(12.f));
        ASSERT_EQ(ProjectionDefinition("mercator"), projection->impl->evaluate(16.f));
    }

    {
        auto projection = parseProjection(R"({"type":["vertical-perspective","mercator",0.25]})");
        ASSERT_TRUE(projection);
        ASSERT_TRUE(projection->getType().isConstant());
        ASSERT_EQ(ProjectionDefinition(ProjectionType::VerticalPerspective, ProjectionType::Mercator, 0.25),
                  projection->getType().asConstant());
    }

    {
        auto projection = parseProjection(
            R"({"type":["interpolate",["linear"],["zoom"],10,"vertical-perspective",12,"mercator"]})");
        ASSERT_TRUE(projection);
        ASSERT_TRUE(projection->getType().isExpression());
        const auto type = projection->getType();
        ASSERT_EQ(ProjectionDefinition(ProjectionType::VerticalPerspective), type.asExpression().evaluate(9.f));
        ASSERT_EQ(ProjectionDefinition(ProjectionType::VerticalPerspective), type.asExpression().evaluate(10.f));
        ASSERT_EQ(ProjectionDefinition(ProjectionType::VerticalPerspective, ProjectionType::Mercator, 0.5),
                  type.asExpression().evaluate(11.f));
        ASSERT_EQ(ProjectionDefinition(ProjectionType::Mercator), type.asExpression().evaluate(12.f));
        ASSERT_EQ(ProjectionDefinition(ProjectionType::Mercator), type.asExpression().evaluate(13.f));
    }

    {
        // Stops that are already part way to the globe keep their share of it between them.
        auto projection = parseProjection(
            R"({"type":["interpolate",["linear"],["zoom"],0,["literal",["mercator","vertical-perspective",0.25]],)"
            R"(10,["literal",["mercator","vertical-perspective",0.25]]]})");
        ASSERT_TRUE(projection);
        ASSERT_TRUE(projection->getType().isExpression());
        const auto type = projection->getType();
        EXPECT_DOUBLE_EQ(0.25, type.asExpression().evaluate(0.f).transitionState());
        EXPECT_DOUBLE_EQ(0.25, type.asExpression().evaluate(2.5f).transitionState());
        EXPECT_DOUBLE_EQ(0.25, type.asExpression().evaluate(5.f).transitionState());
        EXPECT_DOUBLE_EQ(0.25, type.asExpression().evaluate(10.f).transitionState());
    }

    {
        // A reversed stop three quarters of the way to the globe blends into one a quarter of the way.
        auto projection = parseProjection(
            R"({"type":["interpolate",["linear"],["zoom"],0,["literal",["vertical-perspective","mercator",0.25]],)"
            R"(10,["literal",["mercator","vertical-perspective",0.25]]]})");
        ASSERT_TRUE(projection);
        const auto type = projection->getType();
        EXPECT_DOUBLE_EQ(0.75, type.asExpression().evaluate(0.f).transitionState());
        EXPECT_DOUBLE_EQ(0.625, type.asExpression().evaluate(2.5f).transitionState());
        EXPECT_DOUBLE_EQ(0.5, type.asExpression().evaluate(5.f).transitionState());
        EXPECT_DOUBLE_EQ(0.25, type.asExpression().evaluate(10.f).transitionState());
    }

    {
        // An end that is the globe preset counts with its share at the zoom, so above 12 it is Mercator.
        auto projection = parseProjection(R"({"type":["interpolate",["linear"],["zoom"],0,"mercator",20,"globe"]})");
        ASSERT_TRUE(projection);
        EXPECT_NEAR(0.25, projection->impl->evaluate(5.f).transitionState(), 1e-6);
        EXPECT_NEAR(0.575 * 0.5, projection->impl->evaluate(11.5f).transitionState(), 1e-6);
        EXPECT_EQ(ProjectionDefinition("mercator"), projection->impl->evaluate(15.f));
    }

    {
        auto projection = parseProjection("{}");
        ASSERT_TRUE(projection);
        const mln::JSValue typeValue("globe");
        ASSERT_FALSE(projection->setProperty("type", &typeValue));
        ASSERT_EQ(ProjectionDefinition(ProjectionType::Globe), projection->getType().asConstant());
        ASSERT_TRUE(projection->setProperty("bogus", &typeValue));
        ASSERT_EQ(StyleProperty::Kind::Constant, projection->getProperty("type").getKind());
        ASSERT_EQ(mln::Value("globe"), projection->getProperty("type").getValue());
    }

    {
        ASSERT_FALSE(parseProjection("3"));
        ASSERT_EQ("projection must be an object", error.message);
    }

    {
        ASSERT_FALSE(parseProjection(R"({"type":3})"));
        ASSERT_EQ("value must be a string or an array of [from, to, transition]", error.message);
    }
}

TEST(StyleConversion, ProjectionSubdivisionGranularity) {
    Error error;
    auto granularity = [&](const std::string& src) {
        auto projection = convertJSON<Projection>(src, error);
        EXPECT_TRUE(projection) << error.message;
        return projection->impl->getSubdivisionGranularity();
    };
    // Only a style that never renders a globe keeps the flat, unsubdivided geometry.
    EXPECT_EQ(SubdivisionGranularitySetting::none(), granularity("{}"));
    EXPECT_EQ(SubdivisionGranularitySetting::none(), granularity(R"({"type":"mercator"})"));
    EXPECT_EQ(SubdivisionGranularitySetting::none(), granularity(R"({"type":"identity"})"));
    EXPECT_EQ(SubdivisionGranularitySetting::globe(), granularity(R"({"type":"globe"})"));
    EXPECT_EQ(SubdivisionGranularitySetting::globe(), granularity(R"({"type":"vertical-perspective"})"));
    EXPECT_EQ(SubdivisionGranularitySetting::globe(), granularity(R"({"type":["mercator","vertical-perspective",0]})"));
    EXPECT_EQ(SubdivisionGranularitySetting::globe(),
              granularity(R"({"type":["interpolate",["linear"],["zoom"],10,"vertical-perspective",12,"mercator"]})"));
}
