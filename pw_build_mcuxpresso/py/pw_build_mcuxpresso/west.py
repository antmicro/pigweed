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
"""West manifest output support."""

import os
from pathlib import Path

from west.app.main import WestApp

def west_manifest(output_path: Path,):
    old_dir = os.getcwd()
    os.makedirs(output_path, exist_ok=True)
    os.chdir(output_path)
    app = WestApp()
    app.run(['init', '-m', "https://github.com/nxp-mcuxpresso/mcux-sdk.git", "--mr", "MCUX_2.16.000"])
    app.run(['update', '-o=--depth=1'])
    os.chdir(old_dir)
