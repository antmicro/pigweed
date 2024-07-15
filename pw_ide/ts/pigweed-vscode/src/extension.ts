// Copyright 2023 The Pigweed Authors
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

import * as vscode from 'vscode';

import { checkExtensions } from './extensionManagement';
import { fileBug } from './links';
import { launchBootstrapTerminal, launchTerminal } from './terminal';

function registerCommands(context: vscode.ExtensionContext) {
  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.file-bug', fileBug),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.check-extensions',
      checkExtensions,
    ),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('pigweed.launch-terminal', launchTerminal),
  );

  context.subscriptions.push(
    vscode.commands.registerCommand(
      'pigweed.bootstrap-terminal',
      launchBootstrapTerminal,
    ),
  );
}

export async function activate(context: vscode.ExtensionContext) {
  registerCommands(context);

  const shouldEnforce = vscode.workspace
    .getConfiguration('pigweed')
    .get('enforceExtensionRecommendations') as string;

  if (shouldEnforce === 'true') {
    console.log('pigweed.enforceExtensionRecommendations: true');
    await checkExtensions();
  }
}

export function deactivate() {
  // Do nothing.
}
