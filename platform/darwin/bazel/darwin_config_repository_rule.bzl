def _impl(ctx):
    # Resolve the files in this module, not in the root module, so that the
    # rule also works when maplibre is used as a dependency.
    example_config = ctx.path(Label("//platform/darwin:bazel/example_config.bzl"))
    config = example_config.dirname.get_child("config.bzl")

    ctx.watch(config)
    ctx.watch(example_config)

    ctx.file("BUILD.bazel", "")
    if config.exists:
        ctx.file("config.bzl", ctx.read(config))
    else:
        ctx.file("config.bzl", ctx.read(example_config))

darwin_config = repository_rule(
    implementation = _impl,
    local = True,
)
