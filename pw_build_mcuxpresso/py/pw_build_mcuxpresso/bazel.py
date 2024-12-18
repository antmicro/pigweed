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

import collections
from dataclasses import asdict, dataclass
from itertools import chain
from pathlib import Path
from typing import Any, Iterable, Iterator

try:
    from pw_build_mcuxpresso.components import Component, Project
    from pw_build_mcuxpresso.consts import (
        SDK_USER_CONFIG_NAME,
        SDK_APP_INCLUDE_NAME,
        SDK_DEFAULT_COPTS,
        SDK_COMMONS_NAME,
    )
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    from components import Component, Project  # type: ignore
    from consts import SDK_USER_CONFIG_NAME  # type: ignore
    from consts import SDK_APP_INCLUDE_NAME # type: ignore
    from consts import SDK_DEFAULT_COPTS  # type: ignore
    from consts import SDK_COMMONS_NAME  # type: ignore


@dataclass
class BazelVariable:
    """Representation of Bazel variable"""

    name: str
    value: Any

    def __str__(self) -> str:
        return f"{self.name} = {self.value}"


@dataclass
class BazelTarget:
    """Representation of single Bazel build target.
    Target's attributes can be represented as dictionary in which
    the keys are strings (attribute names) and values are either
    strings, lists, integers or booleans.

    Properties:
        target_type: name of rule to instantiate
        attrs: target attributes
    """

    target_type: str
    attrs: dict[str, Any]

    def format(self, indent: int = 4) -> str:
        """Generates string representation of the target

        Args:
            indent: specifies amount of indentation added for each nesting level
        """
        text = f"{self.target_type}(\n"

        for key, value in sorted(self.attrs.items(), key=lambda kv: kv[0]):
            # Handle formatting for specific types
            if isinstance(value, str):
                # Wrap string in quotes and escape inner ones
                value = '"{}"'.format(value.replace('"', r'\"'))
            elif isinstance(value, list):
                # Skip printing empty arrays
                if len(value) == 0:
                    continue
                for i, v in enumerate(value):
                    if isinstance(v, BazelTarget):
                        value[i] = v.label()
            elif isinstance(value, BazelVariable):
                # Print variable's name
                value = value.name

            # otherwise, use default string conversion
            text += f'{" " * indent}{key} = {value},\n'

        text += ")"
        return text

    def name(self) -> str:
        """Returns name of this target"""
        if self.attrs.get("name") is None:
            raise KeyError('Target with empty "name" attribute')
        return self.attrs["name"]

    def label(self) -> str:
        """Returns string representation of this target's label."""
        return f":{self.name()}"

    def __lt__(self, other) -> bool:
        return self.label() < other.label()

    def __str__(self) -> str:
        return self.label()


APP_INCLUDE_TARGET = BazelTarget(
    "label_flag",
    {
        "name": SDK_APP_INCLUDE_NAME,
        "build_setting_default": ":empty.h",
    },
)

SHARED_BAZEL_COPTS = BazelVariable("COPTS", SDK_DEFAULT_COPTS + [f"-include $(location {APP_INCLUDE_TARGET.label()})"])

# pylint: disable=line-too-long
BUILDFILE_HEADER = rf'''### This file was auto generated. Do not edit manually. ###
package(default_visibility = ["//visibility:public"])
{SHARED_BAZEL_COPTS}

exports_files(["empty.h"])

'''
# pylint: enable=line-too-long

USER_CONFIG_TARGET = BazelTarget(
    "label_flag",
    {
        "name": SDK_USER_CONFIG_NAME,
        "build_setting_default": "@pigweed//pw_build:empty_cc_library",
    },
)


def _normalize_path_list(targets: list[Path]) -> list[str]:
    """Converts given paths to their string representation
    using '/' as a path separator
    """
    return [target.as_posix() for target in targets]


def _path_to_component_id(target: Path | str) -> str:
    """Converts path to component id by replacing '/' with '.'"""
    if isinstance(target, Path):
        target = target.as_posix()
    return target.replace("/", ".")


def _resolve_component_dep_cycles(project: Project) -> dict[str, Component]:
    """Resolves dependency cycles between components

    Args:
        project: mcuxpresso project
    Returns:
        mapping of component ids to components without dependency cycles
    """

    components = {
        c.id: Component(**asdict(c)) for c in project.components.values()
    }
    extracted_common_components = dict()

    dependencies: collections.deque[str] = collections.deque()
    for component in components.values():
        checked = {component.id}
        dependencies.extend(component.dependencies)

        while len(dependencies) > 0:
            dep_id = dependencies.popleft()
            dep = components.get(dep_id)
            if dep is None:
                continue

            if component.id in dep.dependencies:
                # Workaround for bug in serial_manager_uart component from
                # manifest. It has circular dependency with serial_manager
                # component as both serial_manager has dependency on
                # serial_manager_uart and vice versa. Fix this by extracting
                # common dependencies using serial_manager_uart as base
                # component
                if component.id == "component.serial_manager_uart.MIMXRT595S":
                    c = component.extract_common(dep)
                else:
                    c = dep.extract_common(component)
                extracted_common_components[c.id] = c

            dependencies.extend(dep.dependencies.difference(checked))
            checked.update(dep.dependencies)

    return dict(chain(components.items(), extracted_common_components.items()))


def headers_cc_library(project: Project) -> BazelTarget:
    """Generates common headers Bazel cc_library target

    Args:
        project: mcuxpresso project
        output_path: path to output directory
    """
    deduplicate_chain = lambda it: sorted(set(chain.from_iterable(it)))

    components = project.components.values()

    defines = deduplicate_chain(component.defines for component in components)

    headers = deduplicate_chain(component.headers for component in components)
    headers = _normalize_path_list(headers)

    includes = deduplicate_chain(
        component.include_dirs for component in components
    )
    includes = _normalize_path_list(includes)

    return BazelTarget(
        "cc_library",
        {
            "name": SDK_COMMONS_NAME,
            "deps": [USER_CONFIG_TARGET],
            "defines": defines,
            "hdrs": headers,
            "srcs": [APP_INCLUDE_TARGET],
            "includes": includes,
            "copts": SHARED_BAZEL_COPTS,
        },
    )


def import_targets(libraries: Iterable[Path]) -> list[BazelTarget]:
    """Generates Bazel cc_import targets for static libraries

    Args:
        libraries: paths to '.a' library files
    """

    return sorted(
        (
            BazelTarget(
                "cc_import",
                {
                    "name": _path_to_component_id(library),
                    "static_library": library.as_posix(),
                },
            )
            for library in libraries
        ),
    )


def component_targets(
    components: dict[str, Component],
    imports: list[BazelTarget],
    commons: BazelTarget,
) -> list[BazelTarget]:
    """Returns Bazel cc_library targets representing SDK components

    Args:
        components: mapping of component ids to SDK components
        imports: list of import targets
        commons: target containing common definitions
    """

    libraries = {target.name(): target for target in imports}
    parsed: dict[str, BazelTarget] = dict()

    def component_target(component: Component) -> BazelTarget:
        if component.id in parsed.keys():
            return parsed[component.id]

        libs = [libraries[_path_to_component_id(lib)] for lib in component.libs]

        deps = sorted(
            chain(
                [commons],
                libs,
                map(
                    lambda dep_id: component_target(components[dep_id])
                    if dep_id not in parsed.keys()
                    else parsed[dep_id],
                    component.dependencies,
                ),
            ),
        )

        sources = _normalize_path_list(component.sources)

        attrs: dict[str, Any] = {
            "name": component.id,
            "srcs": sources + [APP_INCLUDE_TARGET],
            "deps": deps,
            "copts": SHARED_BAZEL_COPTS,
        }

        if component.private:
            attrs["visibility"] = ["//visibility:private"]

        if component.alwayslink:
            attrs["alwayslink"] = True

        target = BazelTarget("cc_library", attrs)
        parsed[component.id] = target

        return target

    for component in components.values():
        component_target(component)

    return sorted(parsed.values())


def generate_project_targets(project: Project) -> Iterator[BazelTarget]:
    """Generates Bazel targets for a project

    Args:
        project: MCUXpresso project to output
        output_path: Path to output directory
    """
    components = _resolve_component_dep_cycles(project)
    libraries = set(
        chain.from_iterable(component.libs for component in components.values())
    )

    commons = headers_cc_library(project)
    imports = import_targets(libraries)

    return chain(
        [USER_CONFIG_TARGET, APP_INCLUDE_TARGET, commons],
        imports,
        component_targets(components, imports, commons),
    )


def generate_bazel_files(
    project: Project,
    output_path: Path,
):
    """Generates Bazel files for a project in specified output directory.

    Args:
        project: MCUXpresso project to output
        output_path: Path to output directory
    """
    print("# Generating bazel files... ")

    output_path.mkdir(parents=True, exist_ok=True)

    module_file = output_path / "MODULE.bazel"
    module_file.touch()

    empty_file = output_path / "empty.h"
    empty_file.touch()

    build_file = output_path / "BUILD.bazel"
    with open(build_file, "w") as f:
        f.write(BUILDFILE_HEADER)

        for target in generate_project_targets(project):
            f.write(f"{target.format()}\n")

        print(f"Generated targets for {len(project.components)} components")
