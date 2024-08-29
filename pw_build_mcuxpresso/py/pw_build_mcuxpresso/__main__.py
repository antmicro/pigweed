#!/usr/bin/env python3
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
"""Command line interface for mcuxpresso_builder."""

import argparse
import pathlib
import sys

try:
    from pw_build_mcuxpresso import bazel, components, gn, west
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    import components  # type: ignore
    import bazel  # type: ignore
    import gn  # type: ignore
    import west # type: ignore


def _parse_args() -> argparse.Namespace:
    """Setup argparse and parse command line args."""
    parser = argparse.ArgumentParser()

    subparsers = parser.add_subparsers(
        dest='command', metavar='<command>', required=True
    )

    subparser = subparsers.add_parser(
        'gn', help='output components of an MCUXpresso project as GN scope'
    )
    subparser.add_argument('manifest_filename', type=pathlib.Path)
    subparser.add_argument('--west', type=pathlib.Path)
    subparser.add_argument('--include', type=str, action='append')
    subparser.add_argument('--exclude', type=str, action='append')
    subparser.add_argument('--device-core', type=str)
    subparser.add_argument('--prefix', dest='path_prefix', type=str)

    subparser = subparsers.add_parser(
        'bazel', help='output an MCUXpresso project as a bazel target'
    )
    subparser.add_argument('manifest_filename', type=pathlib.Path)
    subparser.add_argument('--params_file', dest='params_file', type=pathlib.Path, required=True)
    subparser.add_argument('--name', dest='bazel_name', type=str, required=True)
    subparser.add_argument('--include', type=str, action='append')
    subparser.add_argument('--exclude', type=str, action='append')
    subparser.add_argument('--device-core', type=str)
    subparser.add_argument('--prefix', dest='path_prefix', type=str)
    subparser.add_argument('--output_file', type=pathlib.Path)
    subparser.add_argument('--output_path', type=pathlib.Path)
    subparser.add_argument('--libraries', nargs="*", type=pathlib.Path)


    return parser.parse_args()


def main():
    """Main command line function."""
    args = _parse_args()

    west.west_manifest(args.output_path)

    project = components.Project.from_file(
        args.output_path / "core" / args.manifest_filename,
        include=args.include,
        exclude=args.exclude,
        device_core=args.device_core,
    )

    if args.command == 'gn':
        gn.gn_output(project, path_prefix=args.path_prefix)
    if args.command == 'bazel':
        bazel.bazel_output(
            project, output_file=args.output_file, name=args.bazel_name, path_prefix=args.path_prefix,
            params_output_file=args.params_file, libraries=args.libraries
        )

    sys.exit(0)


if __name__ == '__main__':
    main()
