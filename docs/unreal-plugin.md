# Unreal Editor Plugin

## Status

The V1 plugin is an Editor-only Unreal plugin scaffold. It opens a dockable assistant panel and connects to the local backend over HTTP.

Validated with:

- Unreal Engine 5.6 on Windows
- Win64 Editor plugin build through `RunUAT BuildPlugin`

Compatibility goal:

- keep the baseline plugin workflow available across long-lived Unreal Engine 5 projects;
- validate and package per supported Unreal Engine version;
- isolate version-specific editor APIs behind small integration adapters;
- use newer experimental editor features only when detected and enabled.

Implemented actions:

- configure plugin settings from Unreal Project Settings;
- check backend and Ollama runtime status;
- detect the current Unreal project root;
- refresh the local project index automatically before search and ask requests;
- run semantic search without invoking the chat model;
- ask a project-aware question;
- load recent local chat history;
- display search results separately from chat answers;
- display answer source references as a dedicated list;
- open referenced source files from result rows with a configurable source opener command.

## Install in an Unreal Project

Using the helper script:

```powershell
.\scripts\install-unreal-plugin.ps1 -UnrealProjectRoot "C:\Path\To\YourProject"
```

The helper replaces an existing `Plugins/GameProductionAIAssistant` directory.

After running `RunUAT BuildPlugin`, install the precompiled package with:

```powershell
.\scripts\install-unreal-plugin.ps1 -UnrealProjectRoot "C:\Path\To\YourProject" -UsePrecompiledPackage
```

Manual installation:

Copy the repository `plugin` directory into your Unreal project as:

```txt
YourProject/
  Plugins/
    GameProductionAIAssistant/
      GameProductionAIAssistant.uplugin
      Source/
```

Then regenerate project files and rebuild the Unreal project.

## Open the Panel

In Unreal Editor:

```txt
Tools -> Game Production AI Assistant
```

The panel defaults to:

```txt
http://localhost:5000
```

The plugin can start the local backend automatically when a workflow needs it. Auto-start is enabled by default in Project Settings.

For packaged installs, GPAA first looks for:

```txt
Plugins/GameProductionAIAssistant/Resources/Backend/GPAA.Api.exe
```

When that packaged backend exists, users do not need the GitHub repository or `GPAA.Api.csproj`.

For development installs, GPAA falls back to:

```txt
dotnet run --project "{project}" --urls {url}
```

`{project}` is resolved from `Backend Project Path` when set, or auto-detected from common development layouts. `{url}` is the configured backend URL.

Plugin settings live in:

```txt
Edit -> Project Settings -> Plugins -> Game Production AI Assistant
```

The assistant panel includes a `Settings` button that opens this page directly.

The source opener defaults to `Auto`.

Available modes:

- `Auto`: tries Visual Studio Code, Rider, then Visual Studio, and falls back to the Windows default application.
- `Visual Studio Code`: opens with line navigation when VS Code is installed in a standard location or `code` is available.
- `Rider`: opens with line navigation when Rider is installed through JetBrains Toolbox or available as `rider`.
- `Visual Studio`: opens the file in Visual Studio. Exact line navigation is limited by Visual Studio command-line support.
- `Default Windows App`: opens the file with the associated Windows application.
- `Custom Command`: uses a command template with `{file}` and `{line}` placeholders.

The custom command default is:

```txt
code -g "{file}:{line}"
```

Backend URL, project root override, chat provider, OpenAI-compatible model settings, source opener mode, and custom source opener command are saved in the Unreal editor per-project user settings.

Backend auto-start settings are saved there too:

- `Auto Start Backend`: enables startup before search, ask, history, knowledge status, and refresh workflows.
- `Stop Backend On Editor Shutdown`: stops the backend on clean Unreal shutdown only when this editor session launched it. Launched backends also monitor the Unreal process id and exit when that parent process disappears.
- `Backend Project Path`: optional developer fallback path to `GPAA.Api.csproj` when no packaged backend is available.
- `Backend Executable Path`: optional override for advanced/custom installs.
- `Backend Launch Arguments`: optional override. Use `{url}`, `{project}`, or `{backend}` placeholders when needed.

If a backend was already running before Unreal opened, GPAA leaves it running on shutdown. This avoids killing a manually started or shared local backend.

The settings page also includes import/export fields and actions for a portable JSON profile. Secrets such as API keys are excluded from exports by default. Enable `Include Secrets In Export` only when you explicitly want to move secrets to another trusted workstation.

You can still start the backend manually when debugging:

```powershell
dotnet run --project GPAA.Api\GPAA.Api.csproj --urls http://localhost:5000
```

## Workflow

1. Open the Unreal project.
2. Open the assistant panel from the Tools menu.
3. Open `Settings` if you need to change the backend URL, chat provider, model, API key, source opener, backend auto-start command, or run `Check Backend` / `Test Prompt IA`.
4. Let the plugin start the local backend automatically, or start it manually when debugging.
5. Check the backend from Project Settings when needed.
6. Check `Project Knowledge` to see the current indexed files, chunks, synced Unreal assets, embedding model, and last index time.
7. Use `Refresh Knowledge` when you want to force an immediate Asset Registry sync and incremental index refresh.
8. Use the `Prompt IA` tab when you want a model-generated answer grounded in project context.
9. Use the `Search` tab to inspect relevant source chunks. The search tab includes a local result filter.
10. Use `History` to review recent local questions and answers for the project in a closable in-panel page.

`Search` and `Ask` automatically refresh the backend index first. The refresh is incremental, so unchanged files reuse existing chunks and embeddings.

The panel shows a lightweight loading animation while a backend request is running. When an automatic incremental refresh finds no changed files or chunks, the status stays focused on the current operation instead of reporting that Unreal assets are being reindexed.

Search results are displayed in the `Search` tab. Each row includes the source type, relative path, line range, similarity score, a short snippet, and an `Open` button. The button opens physical source files from the configured project root. Unreal asset metadata rows are virtual knowledge documents and show their package/object path in the snippet.

## Current Limitations

- Backend supervision is currently local and Windows-oriented because the packaged V1 backend is published for Win64.
- The backend supports streaming chat, but the plugin uses the stable non-streaming chat path for now.
- Conversation history is persisted locally by the backend and displayed as a text summary in the editor UI.
- Exact line navigation depends on the selected source opener and editor support.
- Error handling is intentionally simple for the first integration pass.
- Unreal MCP integration is not implemented yet. It is planned as an optional capability for newer Unreal versions, not as a requirement for baseline search and ask workflows.

## Compatibility Strategy

The plugin should remain compatible with older supported Unreal Engine versions used by long-running productions.

Baseline features should avoid experimental Unreal APIs:

- opening the assistant panel;
- saving Project Settings;
- checking backend health;
- refreshing project knowledge;
- searching indexed project knowledge;
- asking project-aware questions;
- opening physical source files.

Newer capabilities should be added through version-gated adapters. This keeps old projects on stable behavior while allowing newer projects to use engine-specific APIs.

## Unreal MCP Direction

Newer Unreal versions include an experimental Unreal MCP plugin identified as `ModelContextProtocol` in engine code. When available, GPAA should use it as an optional editor automation bridge.

Planned MCP capabilities:

- detect whether Unreal MCP is installed and enabled;
- connect to the editor-local MCP server from the backend or a dedicated integration service;
- discover available MCP tools at runtime;
- expose read-only inspection actions first;
- require explicit user approval before modifying levels, actors, assets, materials, project settings, or source-controlled files;
- keep an action history with parameters and results;
- provide rollback guidance when the underlying action supports it.

MCP-backed automation should degrade gracefully. If Unreal MCP is unavailable, the plugin should keep the current search, ask, indexing, settings, and source-opening workflows.

## Build Validation

For a distributable package with the backend embedded, run:

```powershell
.\scripts\package-unreal-plugin.ps1
```

By default, this creates a self-contained Windows backend so end users do not need to install the .NET runtime separately. For local development packages, use `-FrameworkDependent`.

This publishes the .NET backend into:

```txt
plugin/Resources/Backend
```

Then it runs Unreal `BuildPlugin`, producing:

```txt
.gpaa/PluginPackage
```

For a C++-only validation build, the plugin can also be validated with:

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="C:\Workspace\Game-Production-AI-Assistant\plugin\GameProductionAIAssistant.uplugin" `
  -Package="C:\Workspace\Game-Production-AI-Assistant\.gpaa\PluginPackage" `
  -TargetPlatforms=Win64 `
  -StrictIncludes
```

The current script is the local baseline for the planned automated release pipeline. See [Unreal Plugin Packaging CI/CD Roadmap](plugin-packaging-ci-cd.md) for the CI jobs, package policy, smoke tests, release gates, runner strategy, and Fab candidate process.

## Source Layout

```txt
plugin/
  GameProductionAIAssistant.uplugin
  Source/
    GPAAssistant/
      GPAAssistant.Build.cs
      Private/
        GPAAssistantModule.*
        Services/
          GPAAHttpClient.*
        UI/
          SGPAAssistantWidget.*
```
