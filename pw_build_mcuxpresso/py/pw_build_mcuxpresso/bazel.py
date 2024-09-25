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
from glob import glob

import os
import pathlib
import shutil
from contextlib import redirect_stdout

try:
    from pw_build_mcuxpresso.components import Project
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    from components import Project  # type: ignore


def _bazel_str(val: Any) -> str:
    """Returns string in Bazel format with correct escaping."""
    return str(val).replace('"', r'\"').replace('$', r'\$')


def _bazel_str_list_includes_out(vals: list[Any],) -> str:
    """Outputs list of strings in Bazel format with correct escaping."""
    result = ""
    vals = [str(f) for f in vals]


    prefix = "-isystem"
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


def bazel_output(
    project: Project,
    output_file: Path,
    params_output_file: Path,
    mcuxpresso_repo: Path,
    libraries: list[pathlib.Path] | None = None,
):
    """Output shell script for Bazel rule for a project with the specified components.

    Args:
        project: MCUXpresso project to output.
    """
    os.makedirs(output_file.parents[0], exist_ok=True)
    os.makedirs(params_output_file.parents[0], exist_ok=True)
    sources_resolved = [src.absolute().resolve().relative_to(pathlib.Path.cwd()) for src in project.sources]
    headers_resolved = [hdr.absolute().resolve().relative_to(pathlib.Path.cwd()) for hdr in project.headers]
    include_dirs_resolved = [inc_dir.absolute().resolve().relative_to(pathlib.Path.cwd()) for inc_dir in project.include_dirs]
    project.sources = sources_resolved
    project.headers = headers_resolved
    project.include_dirs = include_dirs_resolved
    with open(params_output_file, 'w') as f:
        with redirect_stdout(f):
            print(_bazel_str_list_defines_out(project.defines) + " " +
                  _bazel_str_list_includes_out(project.include_dirs))

    with open(output_file, 'w') as f:
        with redirect_stdout(f):
            print("cc_files = [")
            for src in project.sources:
                if src.suffix != ".inc":
                    print(f"    '{src}',")
            print("]")

            copy_list = []
            if libraries is not None:
                for lib in libraries:
                    for src_lib in project.libs:
                        src_lib = pathlib.Path(src_lib)
                        if lib.name == src_lib.name:
                            copy_list.append((lib, src_lib))
            for copy in copy_list:
                shutil.copy(copy[1], copy[0])

    libs_name = [lib.name for lib in project.libs]
    sources_abs = [src.absolute() for src in project.sources]
    headers_abs = [hdr.absolute() for hdr in project.headers]
    # Use glob as pathlib doesn't work with symlinks
    for file in glob(f"{mcuxpresso_repo}/**", recursive=True):
        file = pathlib.Path(file).absolute()
        if file.is_dir():
            continue
        if file in sources_abs:
            continue
        if file in headers_abs:
            continue
        if file.name in libs_name:
            continue
        try:
            file.unlink()
        except FileNotFoundError:
            # it is probably a symlink, pathlib doesn't support removing symlinks
            try:
                os.unlink(file)
            except FileNotFoundError:
                pass
