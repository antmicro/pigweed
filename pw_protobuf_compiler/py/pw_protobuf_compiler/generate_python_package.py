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
"""Generates a setup.py and __init__.py for a Python package."""

import argparse
from collections import defaultdict
from pathlib import Path
import sys
from typing import Dict, List, Set

# Make sure dependencies are optional, since this script may be run when
# installing Python package dependencies through GN.
try:
    from pw_cli.log import install as setup_logging
except ImportError:
    from logging import basicConfig as setup_logging  # type: ignore

_SETUP_TEMPLATE = """# Generated file. Do not modify.
import setuptools

setuptools.setup(
    name={name!r},
    version='0.0.1',
    author='Pigweed Authors',
    author_email='pigweed-developers@googlegroups.com',
    description='Generated protobuf files',
    packages={packages!r},
    package_data={package_data!r},
    include_package_data=True,
    zip_safe=False,
    install_requires=['protobuf'],
)
"""


def _parse_args():
    """Parses and returns the command line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',
                        required=True,
                        help='Name of the generated Python package')
    parser.add_argument('--setup',
                        required=True,
                        type=Path,
                        help='Path to setup.py file')
    parser.add_argument('sources',
                        type=Path,
                        nargs='+',
                        help='Relative paths to sources in the package')
    return parser.parse_args()


def main(package: str, setup: Path, sources: List[Path]) -> int:
    """Generates __init__.py and py.typed files and a setup.py."""
    base = setup.parent.resolve()
    base.mkdir(exist_ok=True)

    # Find all directories in the package, including empty ones.
    subpackages: Set[Path] = set()
    for source in sources:
        subpackages.update(base / path for path in source.parents)
    subpackages.remove(base)

    pkg_data: Dict[str, List[str]] = defaultdict(list)

    # Create __init__.py and py.typed files for each subdirectory.
    for pkg in subpackages:
        pkg.mkdir(exist_ok=True, parents=True)
        pkg.joinpath('__init__.py').touch()

        package_name = '.'.join(pkg.relative_to(base).as_posix().split('/'))
        pkg.joinpath('py.typed').touch()
        pkg_data[package_name].append('py.typed')

    # Add the .pyi for each source file.
    for source in sources:
        pkg = base / source.parent
        package_name = '.'.join(pkg.relative_to(base).as_posix().split('/'))

        path = base.joinpath(source).relative_to(pkg).with_suffix('.pyi')
        pkg_data[package_name].append(str(path))

    setup.write_text(
        _SETUP_TEMPLATE.format(name=package,
                               packages=list(pkg_data),
                               package_data=dict(pkg_data)))
    return 0


if __name__ == '__main__':
    setup_logging()
    sys.exit(main(**vars(_parse_args())))
