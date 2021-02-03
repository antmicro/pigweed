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
"""The script that runs unit tests in Renode."""

import argparse
import subprocess
import sys
import os

_TARGET_RENODE_COMMAND = 'renode-test'
_TESTS_STARTING_STRING = b'[==========] Running all tests.'
_TESTS_DONE_STRING = b'[==========] Done running all tests.'
_TEST_FAILURE_STRING = b'[  FAILED  ]'


def handle_test_results(test_output):
    """Parses test output to determine whether tests passed or failed."""
    if test_output.find(_TESTS_STARTING_STRING) == -1:
        return 1
    if test_output.rfind(_TESTS_DONE_STRING) == -1:
        return 1
    if test_output.rfind(_TEST_FAILURE_STRING) != -1:
        return 1
    return 0


def parse_args():
    """Parses command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', help='The target test binary to run')
    parser.add_argument('script', help='Renode script to be started')
    parser.add_argument('uart', help='UART port name used for test output')
    return parser.parse_args()


def launch_tests(binary: str, script: str, uart: str, expected: str) -> int:
    """Start a process that runs test on binary."""
    env = os.environ.copy()
    env['BIN'] = os.path.abspath(binary)
    env['SCRIPT'] = os.path.abspath(script)
    env['UART'] = uart
    env['EXPECTED'] = expected

    cmd = [
        _TARGET_RENODE_COMMAND, '--show-log', '-r', env['PWD'], 'test.robot'
    ]

    test_process = subprocess.run(cmd, stdout=subprocess.PIPE, env=env)
    print(test_process.stdout.decode('utf-8'))
    return handle_test_results(test_process.stdout)


def main():
    """Set up runner."""
    args = parse_args()
    return launch_tests(args.binary, args.script, args.uart, _TESTS_DONE_STRING)


if __name__ == '__main__':
    sys.exit(main())
