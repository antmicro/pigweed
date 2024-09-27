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
import shutil
from pathlib import Path

from west.app.main import WestApp

def west_manifest(
        mcuxpresso_repo: Path,
        output_path: Path,) -> Path:

    out_mcuxpresso_repo = output_path / 'external' / 'core'
    shutil.copytree(mcuxpresso_repo, out_mcuxpresso_repo)
    # west changes current working directory
    # save and restore it after running to so other relative paths will work 
    current_dir = os.getcwd()
    app = WestApp()
    app.run(['init', '-l', f'{out_mcuxpresso_repo}'])
    app.run(['update', '-o=--depth=1'])
    os.chdir(current_dir)
    return out_mcuxpresso_repo
