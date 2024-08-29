load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", "use_cpp_toolchain")
load("@rules_cc//cc:action_names.bzl", "C_COMPILE_ACTION_NAME", "CPP_LINK_STATIC_LIBRARY_ACTION_NAME")

def _cflags_map_each(f):
    return "--cflags=" + f

def _get_flags_from_cc_info(cc_info):
    hdrs = []
    compilation_context = cc_info.compilation_context
    defines = ["-D" + define for define in compilation_context.defines.to_list()]
    includes = ["-I" + include for include in compilation_context.includes.to_list()]
    includes += ["-isystem" + include for include in compilation_context.external_includes.to_list()]
    includes += ["-iquote" + include for include in compilation_context.quote_includes.to_list()]
    includes += ["-isystem" + include for include in compilation_context.system_includes.to_list()]
    includes += ["-F" + include for include in compilation_context.framework_includes.to_list()]
    hdrs.extend(compilation_context.headers.to_list())
    hdrs.extend(compilation_context.direct_headers)
    hdrs.extend(compilation_context.direct_private_headers)
    hdrs.extend(compilation_context.direct_public_headers)
    hdrs.extend(compilation_context.direct_textual_headers)

    return defines, includes, hdrs


def _get_compiler_info(ctx,
                       required_libs,
                       result_sdk_library,
                       dependencies_directory,
                       sdk_library_params_file,):

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

    compile_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
    )

    compiler_options = cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
        variables = compile_variables,
    )

    libraries_to_link = [
        cc_common.create_library_to_link(
            actions = ctx.actions,
            feature_configuration = feature_configuration,
            cc_toolchain = cc_toolchain,
            static_library = library,
        ) for library in required_libs
    ]

    linker_input = cc_common.create_linker_input(
        owner = ctx.label,
        libraries = depset(direct = [
            cc_common.create_library_to_link(
                actions = ctx.actions,
                feature_configuration = feature_configuration,
                cc_toolchain = cc_toolchain,
                static_library = result_sdk_library,
                alwayslink = True,
            ),
        ] + libraries_to_link,),
    )

    compilation_context = cc_common.create_compilation_context(
        headers = depset([dependencies_directory, sdk_library_params_file]),
        system_includes = depset(["@" + sdk_library_params_file.path]),
    )
        
    linking_context = cc_common.create_linking_context(linker_inputs = depset(direct = [linker_input],))

    return compilation_context, linking_context, cc_path, ar_path, compiler_options


def _mcuxpresso_sdk_impl(ctx):
    mcuxpresso_sdk_builder = ctx.executable._mcuxpresso_sdk_builder
    mcuxpresso_sdk_compiler = ctx.executable._mcuxpresso_sdk_compiler
    mcuxpresso_sdk_files = ctx.attr.mcuxpresso_sdk[DefaultInfo].files
    result_sdk_library = ctx.actions.declare_file(ctx.label.name + ".a")
    # this file contains the parameters used to build SDK
    sdk_library_params_file = ctx.actions.declare_file(ctx.label.name + ".inc")
    manifest_path = ctx.file.manifest.path
    sdk_build_script = ctx.actions.declare_file(ctx.label.name + ".py")
    dependencies_directory = ctx.actions.declare_directory("external")

    required_libs = [ctx.actions.declare_file(library) for library in ctx.attr._libraries]

    builder_args = ctx.actions.args()
    builder_args.add_all(["bazel", manifest_path])
    builder_args.add_all(ctx.attr.include, before_each = "--include")
    builder_args.add_all(ctx.attr.exclude, before_each = "--exclude")
    builder_args.add_all(["--device-core", ctx.attr.device_core])
    builder_args.add_all(["--output_file", sdk_build_script.path])
    builder_args.add_all(["--output_path", dependencies_directory.path])
    builder_args.add_all(["--params_file", sdk_library_params_file.path])
    builder_args.add_all(["--mcuxpresso_repo", ctx.attr.mcuxpresso_sdk.label.workspace_root])
    builder_args.add_all(required_libs, before_each = "--library")

    ctx.actions.run(executable = mcuxpresso_sdk_builder,
                    arguments = [builder_args],
                    inputs = mcuxpresso_sdk_files,
                    outputs = [
                        sdk_build_script,
                        sdk_library_params_file,
                        dependencies_directory
                    ] + required_libs,
    )

    (compilation_context,
     linking_context,
     cc_path,
     ar_path,
     compiler_options) = _get_compiler_info(ctx,
                                            required_libs,
                                            result_sdk_library,
                                            dependencies_directory,
                                            sdk_library_params_file,)

    cc_deps = [dep for dep in ctx.attr.deps if CcInfo in dep]
    file_deps = [dep for dep in ctx.attr.deps if DefaultInfo in dep]

    file_deps_list = []
    for dep in file_deps:
        file_deps_list.extend(dep[DefaultInfo].files.to_list())

    cc_info = cc_common.merge_cc_infos(cc_infos = [
            CcInfo(compilation_context = compilation_context,
                   linking_context = linking_context),
    ] + [dep[CcInfo] for dep in cc_deps])

    (defines, includes, hdrs) = _get_flags_from_cc_info(cc_info)

    copts = [ctx.expand_location(copt) for copt in ctx.attr.copts]

    build_dir = ctx.actions.declare_directory("build")

    compiler_args = ctx.actions.args()
    compiler_args.add_all(["--cc", cc_path])
    compiler_args.add_all(["--ar", ar_path])
    compiler_args.add_all(compiler_options, map_each = _cflags_map_each)
    compiler_args.add_all(includes, map_each = _cflags_map_each)
    compiler_args.add_all(defines, map_each = _cflags_map_each)
    compiler_args.add_all(copts, map_each = _cflags_map_each)
    compiler_args.add_all(["--build_dir", build_dir.path])
    compiler_args.add_all(["--archive_name", result_sdk_library.path])
    compiler_args.add_all(["--sources_file", sdk_build_script.path])
    compiler_args.add_all(["--param_file", sdk_library_params_file.path])

    ctx.actions.run(
        executable = mcuxpresso_sdk_compiler,
        arguments = [compiler_args],
        inputs = [
            sdk_build_script,
            dependencies_directory,
            sdk_library_params_file,
        ] + mcuxpresso_sdk_files.to_list()
        + file_deps_list
        + hdrs,
        tools = [
            ctx.attr._cc_toolchain.files,
        ],
        outputs = [
            result_sdk_library,
            build_dir,
        ],
    )

    return [
        DefaultInfo(
            files = depset([
                result_sdk_library,
                sdk_library_params_file
            ],),
        ),
        cc_info
    ]


mcuxpresso_sdk = rule(
    implementation = _mcuxpresso_sdk_impl,
    attrs = {
        "mcuxpresso_sdk": attr.label(),
        "manifest": attr.label(allow_single_file = [ ".xml" ]),
        "west": attr.label(allow_single_file = [ ".yml" ]),
        "include": attr.string_list(),
        "exclude": attr.string_list(),
        "device_core": attr.string(),
        "deps": attr.label_list(
            # to allow files for copts to `-include`
            allow_files = True,
        ),
        "copts": attr.string_list(
            mandatory = False,
            default = [],
        ),
        "_libraries": attr.string_list(
            default = [
                "libethermind_ble_core.a",
                "libethermind_ble_gatt.a",
                "libethermind_ble_protocol.a",
                "libethermind_ble_util.a",
                "libethermind_ble_ga.a",
            ],
        ),
        "_mcuxpresso_sdk_builder": attr.label(
            default=Label("@pigweed//pw_build_mcuxpresso/py:mcuxpresso_builder"), executable=True, allow_files = True, cfg="exec"),
        "_mcuxpresso_sdk_compiler": attr.label(
            default=Label("@pigweed//pw_build_mcuxpresso/py:mcuxpresso_compiler"), executable=True, allow_files = True, cfg="exec"),
        "_cc_toolchain": attr.label(default=Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
    },
    toolchains = use_cpp_toolchain(),
    fragments = ["cpp"],
    provides = [CcInfo],
)
