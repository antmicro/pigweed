# Copyright 2023 The Pigweed Authors
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
"""Bazel output support."""

from typing import Any
from pathlib import Path

import os
import pathlib
from contextlib import redirect_stdout

try:
    from pw_build_mcuxpresso.components import Project
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    from components import Project  # type: ignore


def _bazel_bool_out(name: str, val: bool, indent: int = 0) -> None:
    """Outputs boolean in Bazel format."""
    print('    ' * indent + f'{name} = "{val}",')


def _bazel_int_out(name: str, val: int, indent: int = 0) -> None:
    """Outputs integer in Bazel format."""
    print('    ' * indent + f'{name} = "{val}",')


def _bazel_str(val: Any) -> str:
    """Returns string in Bazel format with correct escaping."""
    return str(val).replace('"', r'\"').replace('$', r'\$')


def _bazel_str_out(name: str, val: Any, indent: int = 0) -> None:
    """Outputs string in Bazel format with correct escaping."""
    print('    ' * indent + f'{name} = "{_bazel_str(val)}",')


def _bazel_str_list_includes_out(vals: list[Any],
                                 path_prefix: str | None = None, ) -> str:
    """Outputs list of strings in Bazel format with correct escaping."""
    result = ""
    if path_prefix is not None:
        vals = [f'{path_prefix}{str(val)}' for val in vals]
    else:
        vals = [str(f) for f in vals]


    prefix = "-I"
    for val in vals:
        result += f'{prefix}{_bazel_str(val)} '

    return result


def _bazel_str_list_defines_out(vals: list[Any],) -> str:
    """Outputs list of strings in Bazel format with correct escaping."""
    result = ""

    prefix = "-D"
    for val in vals:
        result += f'{prefix}{_bazel_str(val)} '

    return result


def _bazel_str_list_src_out(vals: list[Any],
                            path_prefix: str | None = None) -> None:
    """Outputs list of strings in Bazel format with correct escaping."""
    if path_prefix is not None:
        vals = [f'{path_prefix}{str(val)}' for val in vals]
    else:
        vals = [str(f) for f in vals]

    for val in vals:
        if not val.endswith('.inc'):
            print(f'mkdir -p $build_dir/{pathlib.Path(_bazel_str(val)).parent};')
            print(f'$CC $options @$params_file {_bazel_str(val)} -c -o $build_dir/{_bazel_str(val)}.o &')


def _bazel_str_list_lib_out(vals: list[Any],
                            path_prefix: str | None = None,) -> str:
    """Outputs list of strings in Bazel format with correct escaping."""
    result = ""
    if path_prefix is not None:
        vals = [f'{path_prefix}{str(val)}' for val in vals]
    else:
        vals = [str(f) for f in vals]

    for val in vals:
        if val.endswith('.inc'):
            pass
        elif val.endswith('.a'):
            result += f'{_bazel_str(val)} '
        else:
            result += f'$build_dir/{_bazel_str(val)}.o '
    return result


def _bazel_str_list_out(name: str, vals: list[Any], indent: int = 0) -> str:
    """Outputs list of strings in Bazel format with correct escaping."""
    if not vals:
        return

    result = ""

    prefix = ""
    if name == "defines":
        prefix = "-D"
    if name == "includes":
        prefix = "-I"
    for val in vals:
        if val.endswith('.a'):
            path_val = pathlib.Path(val)
            result += f'-L{path_val.parent}'
            result += f'-l:{path_val.name}'
        else:
            result += f'{prefix}{_bazel_str(val)}'

    return result


def _bazel_path_list_out(
    name: str,
    vals: list[pathlib.Path],
    path_prefix: str | None = None,
    indent: int = 0,
) -> str:
    """Outputs list of paths in Bazel format with common prefix."""
    if path_prefix is not None:
        str_vals = [f'{path_prefix}{str(val)}' for val in vals]
    else:
        str_vals = [str(f) for f in vals]

    return _bazel_str_list_out(name, sorted(set(str_vals)), indent=indent)


def bazel_output(
    project: Project,
    name: str,
    output_file: Path,
    params_output_file: Path,
    path_prefix: str | None = None,
    extra_args: dict[str, Any] | None = None,
    libraries: list[pathlib.Path] | None = None,
):
    """Output shell script for Bazel rule for a project with the specified components.

    Args:
        project: MCUXpresso project to output.
        name: target name to output.
        path_prefix: string prefix to prepend to all paths.
        extra_args: Dictionary of additional arguments to generated target.
    """
    os.makedirs(output_file.parents[0], exist_ok=True)
    os.makedirs(params_output_file.parents[0], exist_ok=True)
    with open(params_output_file, 'w') as f:
        with redirect_stdout(f):
            print(_bazel_str_list_defines_out(project.defines) + " " +
                  _bazel_str_list_includes_out(project.include_dirs, path_prefix=path_prefix))

    with open(output_file, 'w') as f:
        with redirect_stdout(f):
            print("CC=\"$1\"")
            print("AR=\"$2\"")
            print("options=\"$3\"")
            print("build_dir=\"$4\"")
            print("lib_name=\"$5\"")
            print("include_dir=\"$6\"")
            print(f"params_file=\"{params_output_file}\"")

            _bazel_str_list_src_out(project.sources, path_prefix=path_prefix)

            print("wait")

            copy_list = []
            if libraries is not None:
                for lib in libraries:
                    for src_lib in project.libs:
                        src_lib = pathlib.Path(src_lib)
                        if lib.name == src_lib.name:
                            copy_list.append((lib, src_lib))
            for copy in copy_list:
                import shutil
                shutil.copy(copy[1], copy[0])
                #copy[1].rename(copy[0])

            print("$AR -rvs $lib_name " + _bazel_str_list_lib_out(project.sources, path_prefix=path_prefix))

            for arg_name, arg_value in (extra_args or {}).items():
                if isinstance(arg_value, bool):
                    _bazel_bool_out(arg_name, arg_value, indent=1)
                elif isinstance(arg_value, int):
                    _bazel_int_out(arg_name, arg_value, indent=1)
                elif isinstance(arg_value, str):
                    _bazel_str_out(arg_name, arg_value, indent=1)
                elif isinstance(arg_value, list):
                    if all(isinstance(x, str) for x in arg_value):
                        _bazel_str_list_out(arg_name, arg_value, indent=1)
                    else:
                        raise TypeError(
                            f"Can't handle extra arg {arg_name!r}: "
                            f"a list of {type(arg_value[0])}"
                        )
                else:
                    raise TypeError(
                        f"Can't handle extra arg {arg_name!r}: {type(arg_value)}"
                    )

            #print(')')
