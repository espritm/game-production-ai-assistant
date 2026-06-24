# Unreal Editor Plugin

## Status

The V1 plugin is an Editor-only Unreal plugin scaffold. It opens a dockable assistant panel and connects to the local backend over HTTP.

Validated with:

- Unreal Engine 5.6 on Windows
- Win64 Editor plugin build through `RunUAT BuildPlugin`

Implemented actions:

- configure backend URL;
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

The selected mode and custom command are saved in the Unreal editor per-project user settings.

Start the backend before using the plugin:

```powershell
dotnet run --project GPAA.Api\GPAA.Api.csproj --urls http://localhost:5000
```

## Workflow

1. Open the Unreal project.
2. Start the local backend.
3. Open the assistant panel from the Tools menu.
4. Click `Check Backend`.
5. Use `Search` to inspect relevant source chunks.
6. Use `Ask` when you want a model-generated answer grounded in those chunks.
7. Use `History` to review recent local questions and answers for the project.

`Search` and `Ask` automatically refresh the backend index first. The refresh is incremental, so unchanged files reuse existing chunks and embeddings.

Search results and answer sources are displayed as separate lists. Each row includes the relative path, line range, similarity score, a short snippet, and an `Open` button. The button opens the referenced file from the configured project root with the workstation's default associated application.

## Current Limitations

- The plugin does not start or supervise the backend process yet.
- The backend supports streaming chat, but the plugin uses the stable non-streaming chat path for now.
- Conversation history is persisted locally by the backend and displayed as a text summary in the editor UI.
- Exact line navigation depends on the selected source opener and editor support.
- Error handling is intentionally simple for the first integration pass.

## Build Validation

From the repository root, the plugin can be validated with:

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="C:\Workspace\Game-Production-AI-Assistant\plugin\GameProductionAIAssistant.uplugin" `
  -Package="C:\Workspace\Game-Production-AI-Assistant\.gpaa\PluginPackage" `
  -TargetPlatforms=Win64 `
  -StrictIncludes
```

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
