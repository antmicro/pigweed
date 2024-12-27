// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

/// Defines the maximum allowed DOWNLOAD size.
/// Data received during a DOWNLOAD will be stored in a fixed-size
/// buffer allocated from the system heap. This defines the size of
/// this buffer.
#ifndef PW_FASTBOOT_MAX_DOWNLOAD_SIZE
#define PW_FASTBOOT_MAX_DOWNLOAD_SIZE (64 * 1024 - 1)
#endif
