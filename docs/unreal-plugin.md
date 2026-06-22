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
- request project indexing;
- ask a project-aware question;
- display the backend response.

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

Start the backend before using the plugin:

```powershell
dotnet run --project GPAA.Api\GPAA.Api.csproj --urls http://localhost:5000
```

## Workflow

1. Open the Unreal project.
2. Start the local backend.
3. Open the assistant panel from the Tools menu.
4. Click `Check Backend`.
5. Click `Index Project`.
6. Ask a question.

## Current Limitations

- The plugin does not start or supervise the backend process yet.
- Responses are non-streaming.
- Conversation history is not persisted in the editor UI.
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
