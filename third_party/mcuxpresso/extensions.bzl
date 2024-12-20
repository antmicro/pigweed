load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def _mcuxpresso_impl(module_ctx):
    requested_shas = []

    for module in module_ctx.modules:
        for commit in module.tags.source_commit:
            if commit.sha not in requested_shas:
                requested_shas.append(commit.sha)

    if len(requested_shas) > 1:
        fail("mcuxpresso extension does not support specifying multiple different shas: " + str(requested_shas))

    if len(requested_shas) == 0:
        fail("mcuxpresso extension requires specifying a sha")
       
    git_repository(
        name = "mcuxpresso",
        commit = requested_shas[0],
        remote = "https://github.com/antmicro/mcuxpresso-sdk.git",
    )

mcuxpresso = module_extension(
    doc = "Bzlmod extension for pulling in mcuxpresso sources.",
    implementation = _mcuxpresso_impl,
    tag_classes = {
        "source_commit": tag_class(
            doc = "Controls the source code version to use.",
            attrs = {
                "sha": attr.string(
                    doc = "Commit SHA value to use",
                ),
            },
        ),
    },
)
