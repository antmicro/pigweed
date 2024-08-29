load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", "use_cpp_toolchain")
load("@rules_cc//cc:action_names.bzl", "C_COMPILE_ACTION_NAME", "CPP_LINK_STATIC_LIBRARY_ACTION_NAME")

def _mcuxpresso_sdk_impl(ctx):
    mcuxpresso_sdk_builder = ctx.executable._mcuxpresso_sdk_builder
    out = ctx.actions.declare_file(ctx.label.name + ".a")
    sdk_params = ctx.actions.declare_file(ctx.label.name + "_sdk.inc")
    manifest_path = ctx.attr.manifest
    bazel_out = ctx.actions.declare_file(ctx.label.name + ".bzl")
    out_dir = ctx.actions.declare_directory("external")
    required_libs = []
    for library in ctx.attr.libraries:
        required_libs.append(ctx.actions.declare_file(library))

    arguments = ["bazel", manifest_path, "--name", "mcuxpresso_sdk"]
    for include in ctx.attr.include:
        arguments.append("--include")
        arguments.append(include)
    for exclude in ctx.attr.exclude:
        arguments.append("--exclude")
        arguments.append(exclude)

    arguments.append("--device-core")
    arguments.append(ctx.attr.device_core)
    arguments.append("--output_file")
    arguments.append(bazel_out.path)
    arguments.append("--output_path")
    arguments.append(out_dir.path)
    arguments.append("--params_file")
    arguments.append(sdk_params.path)
    arguments.append("--libraries")
    for library in required_libs:
        arguments.append(library.path)

    ctx.actions.run(executable = mcuxpresso_sdk_builder,
                    arguments = arguments,
                    inputs = [],
                    outputs = [bazel_out, out_dir, sdk_params] + required_libs,
    )

    cc_toolchain = find_cpp_toolchain(ctx)

    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )

    cc_path = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
    )

    ar_path = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = CPP_LINK_STATIC_LIBRARY_ACTION_NAME,
    )

    cpp_compile_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        user_compile_flags = ctx.fragments.cpp.copts + ctx.fragments.cpp.conlyopts,
        source_file = bazel_out.path,
        output_file = out.path,
    )

    env = cc_common.get_environment_variables(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
        variables = cpp_compile_variables,
    )

    libraries_to_link = []
    for library in required_libs:
        libraries_to_link.append(
            cc_common.create_library_to_link(
                actions = ctx.actions,
                feature_configuration = feature_configuration,
                cc_toolchain = cc_toolchain,
                static_library = library,
            ),
        )


    linker_input = cc_common.create_linker_input(
        owner = ctx.label,
        libraries = depset(direct = [
            cc_common.create_library_to_link(
                actions = ctx.actions,
                feature_configuration = feature_configuration,
                cc_toolchain = cc_toolchain,
                static_library = out,
                alwayslink = True,
            ),
        ] + libraries_to_link,),
    )

    out_include_dir = out_dir.path + "/include"

    compilation_context_g = cc_common.create_compilation_context(
        headers = depset([out_dir]),
        system_includes = depset([out_include_dir] + [
            out_dir.path + "/" + include
            for include in ctx.attr.includes
        ]),
        quote_includes = depset([". \'" + "@" + sdk_params.path]),
    )
        
    linking_context = cc_common.create_linking_context(linker_inputs = depset(direct = [linker_input],))

    defines = []
    includes = []
    hdrs = []
    include_headers = []
    include_headers_d = []
    for dep in ctx.attr.deps:
        cc_info = dep[CcInfo]
        compilation_context = cc_info.compilation_context
        for define in compilation_context.defines.to_list():
            defines.append("-D" + define)

        for include in compilation_context.includes.to_list():
            includes.append("-I" + include)

        for external_includes in compilation_context.external_includes.to_list():
            includes.append("-isystem" + external_includes)

        for quote_includes in compilation_context.quote_includes.to_list():
            includes.append("-iquote" + quote_includes)

        for system_includes in compilation_context.system_includes.to_list():
            includes.append("-isystem" + system_includes)

        for framework_includes in compilation_context.framework_includes.to_list():
            includes.append("-F" + framework_includes)

        for hdr in compilation_context.headers.to_list():
            hdrs.append(hdr)

        for direct_header in compilation_context.direct_headers:
            hdrs.append(direct_header)

        for direct_private_header in compilation_context.direct_private_headers:
            hdrs.append(direct_private_header)
            
        for direct_public_header in compilation_context.direct_public_headers:
            hdrs.append(direct_public_header)

        for direct_textual_header in compilation_context.direct_textual_headers:
            hdrs.append(direct_textual_header)

    for include_header in ctx.attr.include_headers:
        cc_info = include_header[CcInfo]
        compilation_context = cc_info.compilation_context
        for hdr in compilation_context.headers.to_list():
            include_headers.append("-include" + hdr.path)
            include_headers_d.append(hdr)

    build_dir = ctx.actions.declare_directory("build")
    ctx.actions.run_shell(
        inputs = [bazel_out, out_dir, sdk_params] + include_headers_d,
        tools = [ctx.attr._cc_toolchain.files, out_dir,] + hdrs,
        outputs = [out, build_dir],
        arguments = [cc_path, ar_path, "-mcpu=cortex-m33 -mfloat-abi=hard -Og -g " + " ".join(include_headers) + " " + " ".join(includes) + " " + " ".join(defines), build_dir.path, out.path],
        command = bazel_out.path + " \"$1\" \"$2\" \"$3\" \"$4\" \"$5\"",
    )

    cc_info = cc_common.merge_cc_infos(cc_infos = [
            CcInfo(compilation_context = compilation_context_g, linking_context = linking_context),
    ] + [dep[CcInfo] for dep in ctx.attr.deps])
    return [
        DefaultInfo(
            files = depset([out, sdk_params])),
        cc_info]

mcuxpresso_sdk = rule(
    implementation = _mcuxpresso_sdk_impl,
    attrs = {
        "manifest": attr.string(),
        "west": attr.label(allow_single_file = [ ".yml" ]),
        "include": attr.string_list(),
        "exclude": attr.string_list(),
        "device_core": attr.string(),
        "deps": attr.label_list(),
        "includes": attr.string_list(
            doc = (
                "Optional list of include dirs to be passed to the dependencies of this library. " +
                "They are NOT passed to the compiler, you should duplicate them in the configuration options."
            ),
            mandatory = False,
            default = [],
        ),
        "libraries": attr.string_list(
            mandatory = False,
            default = [],
        ),
        "include_headers": attr.label_list(),
        "_mcuxpresso_sdk_builder": attr.label(
            default=Label("@pigweed//pw_build_mcuxpresso/py:mcuxpresso_builder"), executable=True, allow_files = True, cfg="exec"),
        "_cc_toolchain": attr.label(default=Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
    },
    toolchains = use_cpp_toolchain(),
    fragments = ["cpp"],
    provides = [CcInfo],
)
