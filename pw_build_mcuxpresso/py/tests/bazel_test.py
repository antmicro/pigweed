# Copyright 2024 The Pigweed Authors
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
"""Bazel Tests."""

import pathlib
import unittest
import xml.etree.ElementTree

from pw_build_mcuxpresso.bazel import (
    BazelTarget,
    BazelVariable,
    generate_project_targets,
    headers_cc_library,
    import_targets,
)
from pw_build_mcuxpresso.components import Project

# pylint: disable=missing-function-docstring
# pylint: disable=line-too-long


def setup_test_project() -> Project:
    test_manifest_xml = '''
    <manifest>
      <components>
        <component id="test">
          <dependencies>
            <component_dependency value="foo"/>
            <component_dependency value="bar"/>
          </dependencies>
        </component>
        <component id="foo" package_base_path="foo">
          <defines>
            <define name="FOO"/>
          </defines>
          <source relative_path="include" type="c_include">
            <files mask="foo.h"/>
          </source>
          <source relative_path="src" type="src">
            <files mask="foo.cc"/>
          </source>
          <include_paths>
            <include_path relative_path="include" type="c_include"/>
          </include_paths>
        </component>
        <component id="bar" package_base_path="bar">
          <defines>
            <define name="BAR"/>
          </defines>
          <source relative_path="include" type="c_include">
            <files mask="bar.h"/>
          </source>
          <source relative_path="src" type="src">
            <files mask="bar.cc"/>
          </source>
          <include_paths>
            <include_path relative_path="include" type="c_include"/>
          </include_paths>
        </component>
        <component id="frodo" package_base_path="frodo">
          <dependencies>
            <component_dependency value="bilbo"/>
            <component_dependency value="smeagol"/>
          </dependencies>
          <defines>
            <define name="FRODO"/>
          </defines>
          <source relative_path="include" type="c_include">
            <files mask="frodo.h"/>
          </source>
          <source relative_path="src" type="src">
            <files mask="frodo.cc"/>
          </source>
          <source toolchain="armgcc" relative_path="./" type="lib">
            <files mask="libonering.a"/>
          </source>
          <include_paths>
            <include_path relative_path="include" type="c_include"/>
          </include_paths>
        </component>
        <component id="bilbo" package_base_path="bilbo">
          <defines>
            <define name="BILBO"/>
          </defines>
          <source relative_path="include" type="c_include">
            <files mask="bilbo.h"/>
          </source>
          <source relative_path="src" type="src">
            <files mask="bilbo.cc"/>
          </source>
          <include_paths>
            <include_path relative_path="include" type="c_include"/>
          </include_paths>
        </component>
        <component id="smeagol" package_base_path="smeagol">
          <dependencies>
            <component_dependency value="gollum"/>
          </dependencies>
          <source toolchain="armgcc" relative_path="./" type="lib">
            <files mask="libonering.a"/>
          </source>
        </component>
        <component id="gollum" package_base_path="gollum">
          <dependencies>
            <component_dependency value="smeagol"/>
          </dependencies>
          <source toolchain="armgcc" relative_path="./" type="lib">
            <files mask="libonering.a"/>
          </source>
        </component>
      </components>
    </manifest>
    '''
    manifest = (
        xml.etree.ElementTree.fromstring(test_manifest_xml),
        pathlib.Path.cwd() / "manifest.xml",
    )
    return Project([manifest], pathlib.Path.cwd(), ["test", "frodo"])


class BazelTargetTest(unittest.TestCase):
    """BazelTarget tests."""

    def test_name(self):
        target = BazelTarget("test_rule", {"name": "foo"})

        self.assertEqual(target.name(), "foo")

    def test_no_name(self):
        target = BazelTarget("test_rule", {"noname": "foo"})

        self.assertRaises(KeyError, target.name)

    def test_label(self):
        target = BazelTarget("test_rule", {"name": "foo"})

        self.assertEqual(target.label(), ":foo")

    def test_str_attrib(self):
        expected_format = r'''
test_rule(
    bar = "baz",
    name = "foo",
)
'''.strip()

        target = BazelTarget("test_rule", {"name": "foo", "bar": "baz"})

        self.assertEqual(target.format(), expected_format)

    def test_num_attr(self):
        expected_format = r'''
test_rule(
    bar = 13,
    name = "foo",
)
'''.strip()

        target = BazelTarget(
            "test_rule",
            {
                "name": "foo",
                "bar": 13,
            },
        )

        self.assertEqual(target.format(), expected_format)

    def test_bool_attr(self):
        expected_format = r'''
test_rule(
    bar = True,
    baz = False,
    name = "foo",
)
'''.strip()

        target = BazelTarget(
            "test_rule",
            {
                "name": "foo",
                "bar": True,
                "baz": False,
            },
        )

        self.assertEqual(target.format(), expected_format)

    def test_bazel_var_attr(self):
        expected_format = r'''
test_rule(
    bar = MY_VAR,
    name = "foo",
)
'''.strip()

        target = BazelTarget(
            "test_rule",
            {"name": "foo", "bar": BazelVariable("MY_VAR", "")},
        )

        self.assertEqual(target.format(), expected_format)

    def test_list_attr(self):
        expected_format = r'''
test_rule(
    bar = ['baz', 13, True, ':quox'],
    name = "foo",
)
'''.strip()

        target = BazelTarget(
            "test_rule",
            {
                "name": "foo",
                "bar": [
                    "baz",
                    13,
                    True,
                    BazelTarget("test_rule", {"name": "quox"}),
                ],
            },
        )

        self.assertEqual(target.format(), expected_format)

    def test_full_rule(self):
        expected_format = r'''
lore(
    age = 600,
    is_hobbit = True,
    name = "gollum",
    needs = [':fish', ':ring'],
    other_name = "smeagol",
)
'''.strip()

        target = BazelTarget(
            "lore",
            {
                "name": "gollum",
                "other_name": "smeagol",
                "age": 600,
                "is_hobbit": True,
                "needs": [
                    BazelTarget("test_rule", {"name": "fish"}),
                    BazelTarget("test_rule", {"name": "ring"}),
                ],
            },
        )

        self.assertEqual(target.format(), expected_format)

    def test_indent(self):
        expected_format = r'''
test_rule(
  indent = 2,
  name = "foo",
)
'''.strip()

        target = BazelTarget("test_rule", {"name": "foo", "indent": 2})

        self.assertEqual(target.format(2), expected_format)


class ImportTargetsTest(unittest.TestCase):
    """import_targets tests."""

    def test_single_lib(self):
        library = [pathlib.Path("testlib.a")]

        targets = import_targets(library)

        self.assertEqual(len(targets), 1)
        self.assertListEqual(
            targets,
            [
                BazelTarget(
                    "cc_import",
                    {
                        "name": "testlib.a",
                        "static_library": "testlib.a",
                    },
                )
            ],
        )

    def test_nested_lib(self):
        library = [pathlib.Path("some/path/to/testlib.a")]

        targets = import_targets(library)

        self.assertEqual(len(targets), 1)
        self.assertListEqual(
            targets,
            [
                BazelTarget(
                    "cc_import",
                    {
                        "name": "some.path.to.testlib.a",
                        "static_library": "some/path/to/testlib.a",
                    },
                )
            ],
        )

    def test_multi_libs(self):
        libraries = [
            pathlib.Path("some/path/to/testlib.a"),
            pathlib.Path("testlib.a"),
            pathlib.Path("libonering.a"),
        ]

        targets = import_targets(libraries)

        self.assertEqual(len(targets), len(libraries))
        self.assertListEqual(
            targets,
            [
                BazelTarget(
                    "cc_import",
                    {
                        "name": "libonering.a",
                        "static_library": "libonering.a",
                    },
                ),
                BazelTarget(
                    "cc_import",
                    {
                        "name": "some.path.to.testlib.a",
                        "static_library": "some/path/to/testlib.a",
                    },
                ),
                BazelTarget(
                    "cc_import",
                    {
                        "name": "testlib.a",
                        "static_library": "testlib.a",
                    },
                ),
            ],
        )


class HeadersCommonTest(unittest.TestCase):
    """headers_cc_library tests."""

    def test_common_component(self):
        expected_format = '''
cc_library(
    copts = COPTS,
    defines = ['BAR', 'BILBO', 'FOO', 'FRODO'],
    deps = [':user_config'],
    hdrs = ['bar/include/bar.h', 'bilbo/include/bilbo.h', 'foo/include/foo.h', 'frodo/include/frodo.h'],
    includes = ['bar/include', 'bilbo/include', 'foo/include', 'frodo/include'],
    name = "commons",
)
'''.strip()

        project = setup_test_project()

        commons = headers_cc_library(project)

        self.assertIsNotNone(commons.attrs.get("defines"))
        self.assertListEqual(
            commons.attrs["defines"], ["BAR", "BILBO", "FOO", "FRODO"]
        )

        self.assertIsNotNone(commons.attrs.get("hdrs"))
        self.assertListEqual(
            commons.attrs["hdrs"],
            [
                'bar/include/bar.h',
                'bilbo/include/bilbo.h',
                'foo/include/foo.h',
                'frodo/include/frodo.h',
            ],
        )

        self.assertIsNotNone(commons.attrs.get("includes"))
        self.assertListEqual(
            commons.attrs["includes"],
            ['bar/include', 'bilbo/include', 'foo/include', 'frodo/include'],
        )

        self.assertEqual(commons.format(), expected_format)


class ProjectTargetsTest(unittest.TestCase):
    """generate_project_targets tests."""

    def test_project(self):
        expected_format = '''
label_flag(
    build_setting_default = "@pigweed//pw_build:empty_cc_library",
    name = "user_config",
)
cc_library(
    copts = COPTS,
    defines = ['BAR', 'BILBO', 'FOO', 'FRODO'],
    deps = [':user_config'],
    hdrs = ['bar/include/bar.h', 'bilbo/include/bilbo.h', 'foo/include/foo.h', 'frodo/include/frodo.h'],
    includes = ['bar/include', 'bilbo/include', 'foo/include', 'frodo/include'],
    name = "commons",
)
cc_import(
    name = "frodo.libonering.a",
    static_library = "frodo/libonering.a",
)
cc_import(
    name = "gollum.libonering.a",
    static_library = "gollum/libonering.a",
)
cc_import(
    name = "smeagol.libonering.a",
    static_library = "smeagol/libonering.a",
)
cc_library(
    copts = COPTS,
    deps = [':commons'],
    name = "bar",
    srcs = ['bar/src/bar.cc'],
)
cc_library(
    copts = COPTS,
    deps = [':commons'],
    name = "bilbo",
    srcs = ['bilbo/src/bilbo.cc'],
)
cc_library(
    copts = COPTS,
    deps = [':commons'],
    name = "foo",
    srcs = ['foo/src/foo.cc'],
)
cc_library(
    copts = COPTS,
    deps = [':bilbo', ':commons', ':frodo.libonering.a', ':smeagol'],
    name = "frodo",
    srcs = ['frodo/src/frodo.cc'],
)
cc_library(
    copts = COPTS,
    deps = [':commons', ':gollum.libonering.a', ':gollum.smeagol'],
    name = "gollum",
)
cc_library(
    copts = COPTS,
    deps = [':commons', ':gollum.libonering.a', ':smeagol.libonering.a'],
    name = "gollum.smeagol",
    visibility = ['//visibility:private'],
)
cc_library(
    copts = COPTS,
    deps = [':commons', ':gollum', ':gollum.smeagol', ':smeagol.libonering.a'],
    name = "smeagol",
)
cc_library(
    copts = COPTS,
    deps = [':bar', ':commons', ':foo'],
    name = "test",
)
'''.strip()

        project = setup_test_project()

        targets = list(generate_project_targets(project))
        self.assertEqual(len(targets), 13)

        targets = "\n".join(target.format() for target in targets)
        self.assertEqual(targets, expected_format)


if __name__ == '__main__':
    unittest.main()
