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

*** Settings ***
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Reset Emulation
Test Teardown                 Test Teardown
Resource                      ${RENODEKEYWORDS}

*** Test Cases ***
Should Run Test Binary
    [Documentation]           Runs a demo application on a selected platform

    ${BIN} =                  Get Environment Variable    BIN
    ${SCRIPT} =               Get Environment Variable    SCRIPT
    ${UART} =                 Get Environment Variable    UART
    ${EXPECTED} =             Get Environment Variable    EXPECTED

    Execute Command           $bin = @${BIN}
    Execute Script            ${SCRIPT}

    Create Terminal Tester    ${UART}
    Execute Command           showAnalyzer ${UART} Antmicro.Renode.Analyzers.LoggingUartAnalyzer
    Start Emulation
    Write Line To Uart   version
    Write Line To Uart   version
    Write Line To Uart   version

    Wait For Line On Uart     ${EXPECTED}
