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
"""Bazel compile support."""

import argparse
import pathlib
import subprocess
import sys

from multiprocessing import cpu_count
from multiprocessing.pool import ThreadPool as Pool

def _parse_args() -> argparse.Namespace:
    """Setup argparse and parse command line args."""
    parser = argparse.ArgumentParser()

    parser.add_argument('--cc', type=pathlib.Path)
    parser.add_argument('--ar', type=pathlib.Path)
    parser.add_argument('--cflags', type=str, action='append')
    parser.add_argument('--build_dir', type=pathlib.Path)
    parser.add_argument('--param_file', type=pathlib.Path, required=True)
    parser.add_argument('--sources_file', type=pathlib.Path, required=True)

    parser.add_argument('--archive_name', type=str)

    return parser.parse_args()


def _compile(cc: pathlib.Path,
            cflags: list[str],
            param_file: pathlib.Path,
            build_dir: pathlib.Path,):
    """Compile a single source file."""
    def _compile_src(source_file: pathlib.Path):
        out_folder = build_dir / pathlib.Path(source_file).parent
        out_folder.mkdir(parents=True, exist_ok=True)
        subprocess.run([cc] + cflags + [f'@{param_file}', '-c', source_file, '-o', f'{build_dir}/{source_file}.o'])

    return _compile_src


def _archive(ar: pathlib.Path,
        archive_name: str,
        build_dir: pathlib.Path,
        cc_files: list[pathlib.Path],):
    """Link all compiled source files."""
    object_files = []
    for cc_file in cc_files:
        object_files.append(f'{build_dir}/{cc_file}.o')

    subprocess.run([ar, '-rvs', archive_name] + object_files)


def main():
    """Main command line function."""
    args = _parse_args()

    sources_file = args.sources_file
    sys.path.append(str(sources_file.parent.absolute()))
    # this file should be generated using mcuxpresso_builder
    # it contains list of files that we need to compile for specific SDK components
    from mcuxpresso_sdk import cc_files

    with Pool(cpu_count()) as p:
        p.map(_compile(args.cc, args.cflags, args.param_file, args.build_dir), cc_files)

    _archive(args.ar, args.archive_name, args.build_dir, cc_files)

if __name__ == '__main__':
    main()
