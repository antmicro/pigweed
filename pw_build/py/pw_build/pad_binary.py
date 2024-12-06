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
"""Pads a given binary."""

from __future__ import annotations

import shutil
import io
import argparse
import struct
from pathlib import Path


PAD_DEFAULT_BYTE = 0x0


def parse_args() -> dict:
    """Parse arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        required=True,
        help="Path to the input binary file",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Path at which to write the output file",
    )
    parser.add_argument(
        "--pad-byte",
        type=int,
        required=False,
        default=PAD_DEFAULT_BYTE,
        help="Padding byte to use when padding the file",
    )
    parser.add_argument(
        "--pad-to-size",
        type=int,
        required=False,
        help="Pad the file to the given size",
    )
    parser.add_argument(
        "--pad-to-alignment",
        type=int,
        required=False,
        help="Pad the file to the given alignment",
    )

    return vars(parser.parse_args())


def main(
    input: Path,
    output: str,
    pad_byte: int,
    pad_to_size: int | None = None,
    pad_to_alignment: int | None = None,
) -> None:

    with open(input, "rb") as source:
        source_bytes = source.read()

    with open(output, "w+b") as destination:
        destination.write(source_bytes)
        n = destination.tell()

        if pad_to_size is not None and n < pad_to_size:
            pad = pad_to_size - n
            destination.write(struct.pack("B", pad_byte) * pad)
            n = destination.tell()

        if pad_to_alignment is not None and (n % pad_to_alignment) != 0:
            pad = pad_to_alignment - (n % pad_to_alignment)
            destination.write(struct.pack("B", pad_byte) * pad)
            n = destination.tell()


if __name__ == "__main__":
    main(**parse_args())
