load("//pw_build:merge_flags.bzl", "merge_flags_for_transition_impl", "merge_flags_for_transition_outputs")
load("//third_party/freertos:flags.bzl", "FREERTOS_FLAGS")

MIMXRT595_SYSTEM_FLAGS = FREERTOS_FLAGS | {
    "@freertos//:freertos_config": str(Label("//targets/mimxrt595_evk_freertos:freertos_config")),
    "@pigweed//pw_assert:assert_backend": str(Label("//pw_assert_trap")),
    "@pigweed//pw_assert:assert_backend_impl": str(Label("//pw_assert_trap:impl")),
    "@pigweed//pw_assert:check_backend": str(Label("//pw_assert_trap")),
    "@pigweed//pw_assert:check_backend_impl": str(Label("//pw_assert_trap:impl")),
    "@pigweed//pw_build:default_module_config": str(Label("//targets/mimxrt595_evk_freertos:pigweed_module_config")),
    "@pigweed//pw_cpu_exception:entry_backend": str(Label("//pw_cpu_exception_cortex_m:cpu_exception")),
    "@pigweed//pw_cpu_exception:entry_backend_impl": str(Label("//pw_cpu_exception_cortex_m:cpu_exception_impl")),
    "@pigweed//pw_cpu_exception:handler_backend": str(Label("//pw_cpu_exception:basic_handler")),
    "@pigweed//pw_cpu_exception:support_backend": str(Label("//pw_cpu_exception_cortex_m:support")),
    "@pigweed//pw_interrupt:backend": str(Label("//pw_interrupt_cortex_m:context")),
    "@pigweed//pw_log:backend": str(Label("//pw_log_tokenized")),
    "@pigweed//pw_log:backend_impl": str(Label("//pw_log_tokenized:impl")),
    "@pigweed//pw_log_tokenized:handler_backend": str(Label("//pw_system:log_backend")),
    "@pigweed//pw_sys_io:backend": str(Label("//pw_sys_io_mimxrt595_evk_freertos")),
    "@pigweed//pw_system:device_handler_backend": str(Label("//targets/mimxrt595_evk_freertos:device_handler")),
    "@pigweed//pw_system:extra_platform_libs": str(Label("//targets/mimxrt595_evk_freertos:extra_platform_libs")),
    "@pigweed//pw_toolchain:cortex-m_toolchain_kind": "clang",
    "@pigweed//pw_trace:backend": str(Label("//pw_trace_tokenized:pw_trace_tokenized")),
    "@pigweed//pw_unit_test:backend": str(Label("//pw_unit_test:light")),
    "@pigweed//pw_unit_test:main": str(Label("//targets/mimxrt595_evk_freertos:unit_test_app")),
}

_COMMON_FLAGS = merge_flags_for_transition_impl(
    base = MIMXRT595_SYSTEM_FLAGS,
    override = {
        "@pigweed//pw_build:default_module_config": "//system:module_config",
        "@pigweed//pw_system:io_backend": "@pigweed//pw_system:sys_io_target_io",
    },
)

_MIMXRT595_FLAGS = {
    "//command_line_option:platforms": "//targets/mimxrt595:mimxrt595",
}

def _mimxrt595_transition(device_specific_flags):
    def _mimxrt595_transition_impl(settings, attr):
        # buildifier: disable=unused-variable
        _ignore = settings, attr
        return merge_flags_for_transition_impl(
            base = _COMMON_FLAGS,
            override = device_specific_flags,
        )

    return transition(
        implementation = _mimxrt595_transition_impl,
        inputs = [],
        outputs = merge_flags_for_transition_outputs(
            base = _COMMON_FLAGS,
            override = device_specific_flags,
        ),
    )

def _mimxrt595_binary_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.symlink(output = out, is_executable = True, target_file = ctx.executable.binary)
    return [DefaultInfo(files = depset([out]), executable = out)]

mimxrt595_binary = rule(
    _mimxrt595_binary_impl,
    attrs = {
        "binary": attr.label(
            doc = "cc_binary to build for the mimxrt595040",
            cfg = _mimxrt595_transition(_MIMXRT595_FLAGS),
            executable = True,
            mandatory = True,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    doc = "Builds the specified binary for the mimxrt595 platform",
    # This target is for mimxrt595 and can't be run on host.
    executable = False,
)
