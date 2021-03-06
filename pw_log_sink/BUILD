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

load(
    "//pw_build:pigweed.bzl",
    "pw_cc_library",
    "pw_cc_test",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache License 2.0

pw_cc_library(
    name = "pw_log_sink",
    srcs = [ "log_sink.cc" ],
    includes = [ "public" ],
    deps = [
        "//pw_bytes",
        "//pw_log:facade",
        "//pw_log_multisink:log_queue",
        "//pw_preprocessor",
        "//pw_status",
        "//pw_sync:spin_lock",
    ],
    hdrs = [
        "public/pw_log_sink/log_sink.h",
        "public_overrides/pw_log_backend/log_backend.h"
    ]
)

pw_cc_test(
    name = "pw_log_sink_test",
    srcs = [ "log_sink_test.cc" ],
    deps = [
        ":pw_log_sink",
        "//pw_log:protos.pwpb",
        "//pw_unit_test",
    ]
)
