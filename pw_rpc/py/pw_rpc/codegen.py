# Copyright 2020 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Common RPC codegen utilities."""

from datetime import datetime
import os
from typing import cast, Any, Callable, Iterable

from pw_protobuf.output_file import OutputFile
from pw_protobuf.proto_tree import ProtoNode, ProtoService, ProtoServiceMethod

import pw_rpc.ids

PLUGIN_NAME = 'pw_rpc_codegen'
PLUGIN_VERSION = '0.2.0'

RPC_NAMESPACE = '::pw::rpc'

STUB_REQUEST_TODO = (
    '// TODO: Read the request as appropriate for your application')
STUB_RESPONSE_TODO = (
    '// TODO: Fill in the response as appropriate for your application')
STUB_WRITER_TODO = (
    '// TODO: Send responses with the writer as appropriate for your '
    'application')

ServerWriterGenerator = Callable[[OutputFile], None]
MethodGenerator = Callable[[ProtoServiceMethod, int, OutputFile], None]
ServiceGenerator = Callable[[ProtoService, ProtoNode, OutputFile], None]
IncludesGenerator = Callable[[Any, ProtoNode], Iterable[str]]


def package(file_descriptor_proto, proto_package: ProtoNode,
            output: OutputFile, includes: IncludesGenerator,
            service: ServiceGenerator, client: ServiceGenerator) -> None:
    """Generates service and client code for a package."""
    assert proto_package.type() == ProtoNode.Type.PACKAGE

    output.write_line(f'// {os.path.basename(output.name())} automatically '
                      f'generated by {PLUGIN_NAME} {PLUGIN_VERSION}')
    output.write_line(f'// on {datetime.now().isoformat()}')
    output.write_line('// clang-format off')
    output.write_line('#pragma once\n')

    output.write_line('#include <array>')
    output.write_line('#include <cstdint>')
    output.write_line('#include <type_traits>\n')

    include_lines = [
        '#include "pw_rpc/internal/method_lookup.h"',
        '#include "pw_rpc/server_context.h"',
        '#include "pw_rpc/service.h"',
    ]
    include_lines += includes(file_descriptor_proto, proto_package)

    for include_line in sorted(include_lines):
        output.write_line(include_line)

    output.write_line()

    if proto_package.cpp_namespace():
        file_namespace = proto_package.cpp_namespace()
        if file_namespace.startswith('::'):
            file_namespace = file_namespace[2:]

        output.write_line(f'namespace {file_namespace} {{')

    for node in proto_package:
        if node.type() == ProtoNode.Type.SERVICE:
            service(cast(ProtoService, node), proto_package, output)
            client(cast(ProtoService, node), proto_package, output)

    if proto_package.cpp_namespace():
        output.write_line(f'}}  // namespace {file_namespace}')


def service_class(service: ProtoService, root: ProtoNode, output: OutputFile,
                  server_writer_alias: ServerWriterGenerator,
                  method_union: str,
                  method_descriptor: MethodGenerator) -> None:
    """Generates a C++ derived class for a nanopb RPC service."""

    output.write_line('namespace generated {')

    base_class = f'{RPC_NAMESPACE}::Service'
    output.write_line('\ntemplate <typename Implementation>')
    output.write_line(
        f'class {service.cpp_namespace(root)} : public {base_class} {{')
    output.write_line(' public:')

    with output.indent():
        output.write_line(
            f'using ServerContext = {RPC_NAMESPACE}::ServerContext;')
        server_writer_alias(output)
        output.write_line()

        output.write_line(f'constexpr {service.name()}()')
        output.write_line(f'    : {base_class}(kServiceId, kMethods) {{}}')

        output.write_line()
        output.write_line(
            f'{service.name()}(const {service.name()}&) = delete;')
        output.write_line(f'{service.name()}& operator='
                          f'(const {service.name()}&) = delete;')

        output.write_line()
        output.write_line(f'static constexpr const char* name() '
                          f'{{ return "{service.name()}"; }}')

        output.write_line()
        output.write_line(
            '// Used by MethodLookup to identify the generated service base.')
        output.write_line(
            'constexpr void _PwRpcInternalGeneratedBase() const {}')

    service_name_hash = pw_rpc.ids.calculate(service.proto_path())
    output.write_line('\n private:')

    with output.indent():
        output.write_line('friend class ::pw::rpc::internal::MethodLookup;\n')
        output.write_line(f'// Hash of "{service.proto_path()}".')
        output.write_line(
            f'static constexpr uint32_t kServiceId = 0x{service_name_hash:08x};'
        )

        output.write_line()

        # Generate the method table
        output.write_line('static constexpr std::array<'
                          f'{RPC_NAMESPACE}::internal::{method_union},'
                          f' {len(service.methods())}> kMethods = {{')

        with output.indent(4):
            for method in service.methods():
                method_descriptor(method, pw_rpc.ids.calculate(method.name()),
                                  output)

        output.write_line('};\n')

        # Generate the method lookup table
        _method_lookup_table(service, output)

    output.write_line('};')

    output.write_line('\n}  // namespace generated\n')


def _method_lookup_table(service: ProtoService, output: OutputFile) -> None:
    """Generates array of method IDs for looking up methods at compile time."""
    output.write_line('static constexpr std::array<uint32_t, '
                      f'{len(service.methods())}> kMethodIds = {{')

    with output.indent(4):
        for method in service.methods():
            method_id = pw_rpc.ids.calculate(method.name())
            output.write_line(
                f'0x{method_id:08x},  // Hash of "{method.name()}"')

    output.write_line('};\n')


StubFunction = Callable[[ProtoServiceMethod, OutputFile], None]

_STUBS_COMMENT = r'''
/*
    ____                __                          __        __  _
   /  _/___ ___  ____  / /__  ____ ___  ___  ____  / /_____ _/ /_(_)___  ____
   / // __ `__ \/ __ \/ / _ \/ __ `__ \/ _ \/ __ \/ __/ __ `/ __/ / __ \/ __ \
 _/ // / / / / / /_/ / /  __/ / / / / /  __/ / / / /_/ /_/ / /_/ / /_/ / / / /
/___/_/ /_/ /_/ .___/_/\___/_/ /_/ /_/\___/_/ /_/\__/\__,_/\__/_/\____/_/ /_/
             /_/
   _____ __        __         __
  / ___// /___  __/ /_  _____/ /
  \__ \/ __/ / / / __ \/ ___/ /
 ___/ / /_/ /_/ / /_/ (__  )_/
/____/\__/\__,_/_.___/____(_)

*/
// This section provides stub implementations of the RPC services in this file.
// The code below may be referenced or copied to serve as a starting point for
// your RPC service implementations.
'''


def package_stubs(proto_package: ProtoNode, output: OutputFile,
                  unary_stub: StubFunction,
                  server_streaming_stub: StubFunction) -> None:

    output.write_line('#ifdef _PW_RPC_COMPILE_GENERATED_SERVICE_STUBS')
    output.write_line(_STUBS_COMMENT)

    output.write_line(f'#include "{output.name()}"\n')

    if proto_package.cpp_namespace():
        file_namespace = proto_package.cpp_namespace()
        if file_namespace.startswith('::'):
            file_namespace = file_namespace[2:]

        output.write_line(f'namespace {file_namespace} {{')

    for node in proto_package:
        if node.type() == ProtoNode.Type.SERVICE:
            _generate_service_stub(cast(ProtoService, node), output,
                                   unary_stub, server_streaming_stub)

    if proto_package.cpp_namespace():
        output.write_line(f'}}  // namespace {file_namespace}')

    output.write_line('\n#endif  // _PW_RPC_COMPILE_GENERATED_SERVICE_STUBS')


def _generate_service_stub(service: ProtoService, output: OutputFile,
                           unary_stub: StubFunction,
                           server_streaming_stub: StubFunction) -> None:
    output.write_line()
    output.write_line(
        f'class {service.name()} '
        f': public generated::{service.name()}<{service.name()}> {{')

    output.write_line(' public:')

    with output.indent():
        blank_line = False

        for method in service.methods():
            if blank_line:
                output.write_line()
            else:
                blank_line = True

            if method.type() is ProtoServiceMethod.Type.UNARY:
                unary_stub(method, output)
            elif method.type() is ProtoServiceMethod.Type.SERVER_STREAMING:
                server_streaming_stub(method, output)
            else:
                raise NotImplementedError(
                    'Client and bidirectional streaming not yet implemented')

    output.write_line('};\n')
