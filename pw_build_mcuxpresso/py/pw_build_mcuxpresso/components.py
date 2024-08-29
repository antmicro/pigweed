# Copyright 2021 The Pigweed Authors
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
"""Finds components for a given manifest."""

import os
import pathlib
import sys
from typing import Collection, Container, List
import xml.etree.ElementTree

def _element_is_compatible_with_device_core(
    element: xml.etree.ElementTree.Element, device_core: str | None
) -> bool:
    """Check whether element is compatible with the given core.

    Args:
        element: element to check.
        device_core: name of core to filter sources for.

    Returns:
        True if element can be used, False otherwise.
    """
    if device_core is None:
        return True

    value = element.attrib.get('device_cores', None)
    if value is None:
        return True

    device_cores = value.split()
    return device_core in device_cores


def get_component(
    root: xml.etree.ElementTree.Element,
    component_id: str,
    device_core: str | None = None,
) -> tuple[xml.etree.ElementTree.Element | None, pathlib.Path | None]:
    """Parse <component> manifest stanza.

    Schema:
        <component id="{component_id}" package_base_path="component"
                   device_cores="{device_core}...">
        </component>

    Args:
        root: root of element tree.
        component_id: id of component to return.
        device_core: name of core to filter sources for.

    Returns:
        (element, base_path) for the component, or (None, None).
    """
    xpath = f'./components/component[@id="{component_id}"]'
    component = root.find(xpath)
    if component is None or not _element_is_compatible_with_device_core(
        component, device_core
    ):
        return (None, None)

    try:
        base_path = pathlib.Path(component.attrib['package_base_path'])
        return (component, base_path)
    except KeyError:
        return (component, None)


def parse_defines(
    root: xml.etree.ElementTree.Element,
    component_id: str,
    device_core: str | None = None,
) -> list[str]:
    """Parse pre-processor definitions for a component.

    Schema:
        <defines>
          <define name="EXAMPLE" value="1" device_cores="{device_core}..."/>
          <define name="OTHER" device_cores="{device_core}..."/>
        </defines>

    Args:
        root: root of element tree.
        component_id: id of component to return.
        device_core: name of core to filter sources for.

    Returns:
        list of str NAME=VALUE or NAME for the component.
    """
    xpath = f'./components/component[@id="{component_id}"]/defines/define'
    return list(
        _parse_define(define)
        for define in root.findall(xpath)
        if _element_is_compatible_with_device_core(define, device_core)
    )


def _parse_define(define: xml.etree.ElementTree.Element) -> str:
    """Parse <define> manifest stanza.

    Schema:
        <define name="EXAMPLE" value="1"/>
        <define name="OTHER"/>

    Args:
        define: XML Element for <define>.

    Returns:
        str with a value NAME=VALUE or NAME.
    """
    name = define.attrib['name']
    value = define.attrib.get('value', None)
    if value is None:
        return name
    # some of the defines are between `'` characters, remove them.
    value = value.replace("'", "")

    return f'{name}={value}'


def parse_include_paths(
    root: tuple[xml.etree.ElementTree.Element, pathlib.Path],
    component_id: str,
    device_core: str | None = None,
) -> list[pathlib.Path]:
    """Parse include directories for a component.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <include_paths>
            <include_path relative_path="./" type="c_include"
                          device_cores="{device_core}..."/>
          </include_paths>
        </component>

    Args:
        root: root of element tree.
        component_id: id of component to return.
        device_core: name of core to filter sources for.

    Returns:
        list of include directories for the component.
    """
    (component, base_path) = get_component(root[0], component_id)
    if component is None:
        return []

    include_paths: list[pathlib.Path] = []
    for include_type in ('c_include', 'asm_include'):
        include_xpath = f'./include_paths/include_path[@type="{include_type}"]'
        parent = root[1].parent

        # workaround for hard-coded name of NXP SDK repository
        # `west` assumes that main NXP SDK repository will be cloned to `core` folder,
        # and clones CMSIS repository there, but in bazel, user chooses the name of the repository.
        # This workaround is needed to find the correct path to CMSIS repository.
        if base_path == pathlib.Path("../CMSIS/Core/Include"):
            base_path = pathlib.Path("../../core/CMSIS/Core/Include")
        include_paths.extend(
            parent / _parse_include_path(include_path, base_path)
            for include_path in component.findall(include_xpath)
            if _element_is_compatible_with_device_core(
                include_path, device_core
            )
        )
    return include_paths


def _parse_include_path(
    include_path: xml.etree.ElementTree.Element,
    base_path: pathlib.Path | None,
) -> pathlib.Path:
    """Parse <include_path> manifest stanza.

    Schema:
        <include_path relative_path="./" type="c_include"/>

    Args:
        include_path: XML Element for <input_path>.
        base_path: prefix for paths.

    Returns:
        Path, prefixed with `base_path`.
    """
    path = pathlib.Path(include_path.attrib['relative_path'])
    # workaround for bug in mcux-sdk-middleware-edgefast-bluetooth manifest
    # in this manifest, relative path to ethermind isn't from base path, but from manifest path.
    if base_path is not None and base_path == pathlib.Path("../../wireless/ethermind"):
        path = pathlib.Path(*path.parts[3:])
    if base_path is None:
        return path
    return base_path / path


def parse_headers(
    root: tuple[xml.etree.ElementTree.Element, pathlib.Path],
    component_id: str,
    device_core: str | None = None,
) -> list[pathlib.Path]:
    """Parse header files for a component.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <source relative_path="./" type="c_include"
                  device_cores="{device_core}...">
            <files mask="example.h"/>
          </source>
        </component>

    Args:
        root: root of element tree.
        component_id: id of component to return.
        device_core: name of core to filter sources for.

    Returns:
        list of header files for the component.
    """
    return _parse_sources(
        root, component_id, 'c_include', device_core=device_core
    )


def parse_sources(
    root: tuple[xml.etree.ElementTree.Element, pathlib.Path],
    component_id: str,
    device_core: str | None = None,
    exclude: Container[str] | None = None,
) -> list[pathlib.Path]:
    """Parse source files for a component.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <source relative_path="./" type="src" device_cores="{device_core}...">
            <files mask="example.cc"/>
          </source>
        </component>

    Args:
        root: root of element tree.
        component_id: id of component to return.
        device_core: name of core to filter sources for.
        exclude: container of component ids excluded from the project.

    Returns:
        list of source files for the component.
    """
    source_files = []
    for source_type in ('src', 'src_c', 'src_cpp', 'asm_include'):
        source_files.extend(
            _parse_sources(
                root, component_id, source_type, device_core=device_core, exclude=exclude
            )
        )
    return source_files


def parse_libs(
    root: tuple[xml.etree.ElementTree.Element, pathlib.Path],
    component_id: str,
    device_core: str | None = None,
) -> list[pathlib.Path]:
    """Parse pre-compiled libraries for a component.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <source relative_path="./" type="lib" device_cores="{device_core}...">
            <files mask="example.a"/>
          </source>
        </component>

    Args:
        root: root of element tree.
        component_id: id of component to return.
        device_core: name of core to filter sources for.

    Returns:
        list of pre-compiler libraries for the component.
    """
    return _parse_sources(root, component_id, 'lib', device_core=device_core)


def _parse_sources(
    root: tuple[xml.etree.ElementTree.Element, pathlib.Path],
    component_id: str,
    source_type: str,
    device_core: str | None = None,
    exclude: Container[str] | None = None,
) -> list[pathlib.Path]:
    """Parse <source> manifest stanza.

    Schema:
        <component id="{component_id}" package_base_path="component">
          <source relative_path="./" type="{source_type}"
                  device_cores="{device_core}...">
            <files mask="example.h"/>
          </source>
        </component>

    Args:
        root: root of element tree.
        component_id: id of component to return.
        source_type: type of source to search for.
        device_core: name of core to filter sources for.
        exclude: container of component ids excluded from the project.
    Returns:
        list of source files for the component.
    """
    (component, base_path) = get_component(root[0], component_id)
    if component is None:
        return []

    sources: list[pathlib.Path] = []
    source_xpath = f'./source[@type="{source_type}"]'
    for source in component.findall(source_xpath):
        if not _element_is_compatible_with_device_core(source, device_core):
            continue

        condition = source.attrib.get('condition', None)
        if condition is not None:
            if exclude is not None and condition not in exclude:
                continue
        toolchain = source.attrib.get('toolchain', None)
        # "mcuxpresso" toolchain contains additional files
        # that are not needed for compilation.
        if toolchain is not None and "mcuxpresso" in toolchain:
            continue
        relative_path = pathlib.Path(source.attrib['relative_path'])
        # workaround for bug in mcux-sdk-middleware-edgefast-bluetooth manifest
        # in this manifest, relative path to ethermind isn't from base path, but from manifest path.
        # In case of libraries, project relative path points to not existing file, but in case of other
        # files, it is correct.
        if base_path is not None and base_path == pathlib.Path("../../wireless/ethermind"):
            if component_id == "middleware.edgefast_bluetooth.ble.ethermind.lib.cm33.MIMXRT595S":
                relative_path = pathlib.Path(*relative_path.parts[3:])
            else:
                relative_path = pathlib.Path(source.attrib['project_relative_path'])
        if base_path is not None:
            relative_path = base_path / relative_path

        parent = root[1].parent
        for file in source.findall('./files'):
            filename = pathlib.Path(file.attrib['mask'])
            # Skip linker scripts, pigweed uses its own linker script.
            if filename.suffix != ".ldt":
                source_absolute = parent / relative_path / filename
                sources.append(source_absolute)
    return sources


def parse_dependencies(
    root: xml.etree.ElementTree.Element, component_id: str
) -> list[str]:
    """Parse the list of dependencies for a component.

    Optional dependencies are ignored for parsing since they have to be
    included explicitly.

    Schema:
        <dependencies>
          <all>
            <component_dependency value="component"/>
            <component_dependency value="component"/>
            <any_of>
              <component_dependency value="component"/>
              <component_dependency value="component"/>
            </any_of>
          </all>
        </dependencies>

    Args:
        root: root of element tree.
        component_id: id of component to return.

    Returns:
        list of component id dependencies of the component.
    """
    dependencies = []
    xpath = f'./components/component[@id="{component_id}"]/dependencies/*'
    for dependency in root.findall(xpath):
        dependencies.extend(_parse_dependency(dependency))
    return dependencies


def _parse_dependency(dependency: xml.etree.ElementTree.Element) -> list[str]:
    """Parse <all>, <any_of>, and <component_dependency> manifest stanzas.

    Schema:
        <all>
          <component_dependency value="component"/>
          <component_dependency value="component"/>
          <any_of>
            <component_dependency value="component"/>
            <component_dependency value="component"/>
          </any_of>
        </all>

    Args:
        dependency: XML Element of dependency.

    Returns:
        list of component id dependencies.
    """
    if dependency.tag == 'component_dependency':
        return [dependency.attrib['value']]
    if dependency.tag == 'all':
        dependencies = []
        for subdependency in dependency:
            dependencies.extend(_parse_dependency(subdependency))
        return dependencies
    if dependency.tag == 'any_of':
        # Explicitly ignore.
        return []

    # Unknown dependency tag type.
    return []


def check_dependencies(
    roots: List[tuple[xml.etree.ElementTree.Element, pathlib.Path]],
    component_id: str,
    include: Collection[str],
    exclude: Container[str] | None = None,
    device_core: str | None = None,
) -> bool:
    """Check the list of optional dependencies for a component.

    Verifies that the optional dependencies for a component are satisfied by
    components listed in `include` or `exclude`.

    Args:
        root: list of root of element trees.
        component_id: id of component to check.
        include: collection of component ids included in the project.
        exclude: optional container of component ids explicitly excluded from
            the project.
        device_core: name of core to filter sources for.

    Returns:
        True if dependencies are satisfied, False if not.
    """
    xpath = f'./components/component[@id="{component_id}"]/dependencies/*'
    for root in roots:
        for dependency in root[0].findall(xpath):
            if not _check_dependency(
                dependency, include, exclude=exclude, device_core=device_core
            ):
                return False
    return True


def _check_dependency(
    dependency: xml.etree.ElementTree.Element,
    include: Collection[str],
    exclude: Container[str] | None = None,
    device_core: str | None = None,
) -> bool:
    """Check a dependency for a component.

    Verifies that the given {dependency} is satisfied by components listed in
    `include` or `exclude`.

    Args:
        dependency: XML Element of dependency.
        include: collection of component ids included in the project.
        exclude: optional container of component ids explicitly excluded from
            the project.
        device_core: name of core to filter sources for.

    Returns:
        True if dependencies are satisfied, False if not.
    """
    if dependency.tag == 'component_dependency':
        component_id = dependency.attrib['value']
        return component_id in include or (
            exclude is not None and component_id in exclude
        )
    if dependency.tag == 'all':
        for subdependency in dependency:
            if not _check_dependency(
                subdependency, include, exclude=exclude, device_core=device_core
            ):
                return False
        return True
    if dependency.tag == 'any_of':
        for subdependency in dependency:
            if _check_dependency(
                subdependency, include, exclude=exclude, device_core=device_core
            ):
                return True

        tree = xml.etree.ElementTree.tostring(dependency).decode('utf-8')
        print(f'Unsatisfied dependency from: {tree}', file=sys.stderr)
        return False

    # Unknown dependency tag type.
    return True


def create_project(
    roots: List[tuple[xml.etree.ElementTree.Element, pathlib.Path]],
    include: Collection[str],
    exclude: Container[str] | None = None,
    device_core: str | None = None,
) -> tuple[
    list[str],
    list[str],
    list[pathlib.Path],
    list[pathlib.Path],
    list[pathlib.Path],
    list[pathlib.Path],
]:
    """Create a project from a list of specified components.

    Args:
        roots: list of root of element trees.
        include: collection of component ids included in the project.
        exclude: container of component ids excluded from the project.
        device_core: name of core to filter sources for.

    Returns:
        (component_ids, defines, include_paths, headers, sources, libs) for the
        project.
    """
    # Build the project list from the list of included components by expanding
    # dependencies.
    project_list = []
    pending_list = list(include)
    while len(pending_list) > 0:
        component_id = pending_list.pop(0)
        if component_id in project_list:
            continue
        if exclude is not None and component_id in exclude:
            continue

        project_list.append(component_id)
        for root in roots:
            pending_list.extend(parse_dependencies(root[0], component_id))

    return (
        project_list,
        sum(
            (
                parse_defines(root[0], component_id, device_core=device_core)
                for component_id in project_list for root in roots
            ),
            [],
        ),
        sum(
            (
                parse_include_paths(root, component_id, device_core=device_core)
                for component_id in project_list for root in roots
            ),
            [],
        ),
        sum(
            (
                parse_headers(root, component_id, device_core=device_core)
                for component_id in project_list for root in roots
            ),
            [],
        ),
        sum(
            (
                parse_sources(root, component_id, device_core=device_core, exclude=exclude)
                for component_id in project_list for root in roots
            ),
            [],
        ),
        sum(
            (
                parse_libs(root, component_id, device_core=device_core)
                for component_id in project_list for root in roots
            ),
            [],
        ),
    )


class Project:
    """Self-contained MCUXpresso project.

    Properties:
        component_ids: list of component ids compromising the project.
        defines: list of compiler definitions to build the project.
        include_dirs: list of include directory paths needed for the project.
        headers: list of header paths exported by the project.
        sources: list of source file paths built as part of the project.
        libs: list of libraries linked to the project.
        dependencies_satisfied: True if the project dependencies are satisfied.
    """

    @classmethod
    def from_file(
        cls,
        manifest_path: pathlib.Path,
        include: Collection[str],
        exclude: Container[str] | None = None,
        device_core: str | None = None,
    ):
        """Create a self-contained project with the specified components.

        Args:
            manifest_path: path to SDK manifest XML.
            include: collection of component ids included in the project.
            exclude: container of component ids excluded from the project.
            device_core: name of core to filter sources for.
        """
        tree = xml.etree.ElementTree.parse(manifest_path)

        # paths in the manifest are relative paths from manifest folder
        roots = [(tree.getroot(), manifest_path)]

        xpath = "./manifest_includes/*"
        for manifest_include in roots[0][0].findall(xpath):
            include_manifest_path = manifest_path.parents[0] / pathlib.Path(manifest_include.attrib["path"])
            roots.append((xml.etree.ElementTree.parse(include_manifest_path).getroot(), include_manifest_path))

        result = cls(
            roots, include=include, exclude=exclude, device_core=device_core
        )

        return result

    def __init__(
        self,
        manifest: List[tuple[xml.etree.ElementTree.Element, pathlib.Path]],
        include: Collection[str],
        exclude: Container[str] | None = None,
        device_core: str | None = None,
    ):
        """Create a self-contained project with the specified components.

        Args:
            manifest: list of parsed manifest XMLs.
            include: collection of component ids included in the project.
            exclude: container of component ids excluded from the project.
            device_core: name of core to filter sources for.
        """
        (
            self.component_ids,
            self.defines,
            self.include_dirs,
            self.headers,
            self.sources,
            self.libs,
        ) = create_project(
            manifest, include, exclude=exclude, device_core=device_core
        )

        for component_id in self.component_ids:
            if not check_dependencies(
                manifest,
                component_id,
                self.component_ids,
                exclude=exclude,
                device_core=device_core,
            ):
                self.dependencies_satisfied = False
                return

        self.dependencies_satisfied = True
