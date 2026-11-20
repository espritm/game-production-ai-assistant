# User Guide

## What This Version Does

Game Production AI Assistant V1 runs a local backend on your workstation. It can index Unreal Engine project files, search them semantically, and answer questions using local project context.

No cloud service is required.

It also includes an Unreal Editor plugin scaffold that can call the local backend from a dockable editor panel.

## Requirements

- Windows 11
- .NET 8 SDK or newer
- Optional: Ollama for local LLM-backed answers

## Start the Local Backend

From the repository root:

```powershell
dotnet run --project GPAA.Api\GPAA.Api.csproj
```

Open the health endpoint in a browser:

```txt
http://localhost:5000/health
```

If ASP.NET selects another port, use the URL printed in the terminal.

## Automatic Project Indexing

The backend indexes projects automatically before `Search` and `Ask` requests. No manual indexing step is required in the Unreal plugin.

The manual endpoint remains available for diagnostics or scripts:

```powershell
curl.exe -X POST http://localhost:5000/projects/index `
  -H "Content-Type: application/json" `
  -d "{\"projectRoot\":\"C:\\Path\\To\\YourUnrealProject\"}"
```

The backend scans supported files and stores a local index under:

```txt
.gpaa/indexes
```

Indexing is incremental. When the same embedding model is still configured, unchanged files are reused from the previous JSON index and only new or modified files are embedded again. Deleted files are removed from the refreshed index. If you change the embedding model, the next search, ask, or manual indexing run performs a full rebuild so search vectors stay compatible.

The index includes source/config text files and lightweight Unreal asset metadata for `.uasset` and `.umap` files. Asset binaries are not parsed deeply yet, but their path, name, folder, extension, and likely type are searchable. This helps queries about Blueprints, Maps, Materials, Widgets, DataAssets, meshes, textures, and animation assets.

When using the Unreal plugin, `Search` and `Ask` also sync metadata from Unreal's Asset Registry before querying the backend. This adds richer information such as object paths, package paths, asset classes, selected registry tags, and package dependency/referencer relationships when Unreal exposes them.

## Check Index Status

```powershell
curl.exe "http://localhost:5000/projects/index/status?projectRoot=C:\Path\To\YourUnrealProject"
```

## Search the Project

```powershell
curl.exe -X POST http://localhost:5000/search `
  -H "Content-Type: application/json" `
  -d "{\"projectRoot\":\"C:\\Path\\To\\YourUnrealProject\",\"query\":\"Where is stamina regeneration handled?\",\"topK\":5}"
```

The response includes matching source chunks with file paths, line ranges, and similarity scores.

The Unreal plugin exposes the same workflow through the `Search` button. Search uses embeddings only and does not call the chat model. Results appear in a dedicated list with an `Open` button for each referenced file.

The backend search scope used by the Unreal plugin is always `All`, so code and Unreal asset metadata can both be returned:

- `All`: code and Unreal asset metadata.

The `Search` tab includes a local `Filter` combobox that narrows already-returned results without re-querying the backend:

- `All`: show every returned result.
- `Code`: show source files only.
- `Assets`: show Unreal asset metadata only.

The plugin shows a small loading animation while requests are running. If the incremental index reuses all existing files and chunks, the status stays focused on the current operation instead of claiming that Unreal assets are being synced or reindexed.

Source rows can be opened through the plugin's configurable source opener. Configure it from the same Project Settings page. The default mode is `Auto`, which tries common local editor installations before falling back to the Windows default application.

Available modes are:

- `Auto`
- `Visual Studio Code`
- `Rider`
- `Visual Studio`
- `Default Windows App`
- `Custom Command`

The custom command default is:

```txt
code -g "{file}:{line}"
```

You can replace it with another editor command that supports `{file}` and `{line}` placeholders. The selected mode and custom command are saved in the Unreal editor per-project user settings.

The settings page can export and import a portable JSON profile. API keys are not exported unless `Include Secrets In Export` is enabled.

## Ask a Question

```powershell
curl.exe -X POST http://localhost:5000/chat `
  -H "Content-Type: application/json" `
  -d "{\"projectRoot\":\"C:\\Path\\To\\YourUnrealProject\",\"question\":\"Where is stamina regeneration handled?\",\"topK\":5}"
```

The response includes an answer and the source chunks used as context. In the Unreal plugin, the answer and its sources are displayed separately so that source files can be opened without digging through the generated text.

The backend also exposes `POST /chat/stream` for streaming answer fragments as Server-Sent Events. The current Unreal plugin keeps the stable non-streaming request path, while the backend API is ready for streaming clients.

## Chat History

Each completed `Ask` request is stored locally in JSON under the backend storage root. The Unreal plugin includes a `History` button next to `Settings`; it opens a closable in-panel history page for the current project.

## Enable Ollama

Install Ollama and pull models:

```powershell
ollama pull llama3.1
ollama pull nomic-embed-text
```

Then edit `GPAA.Api/appsettings.json`:

```json
{
  "Ollama": {
    "BaseUrl": "http://localhost:11434",
    "ChatModel": "llama3.1",
    "EmbeddingModel": "nomic-embed-text",
    "UseOllamaEmbeddings": true,
    "UseOllamaChat": true
  }
}
```

Restart the backend after changing configuration.

You can replace `ChatModel` with any installed Ollama chat model. On the current test workstation, the backend is configured to use:

```txt
erwan2/DeepSeek-R1-Distill-Qwen-7B:latest
```

If you replace `EmbeddingModel`, the backend records the embedding model inside the local index and rebuilds automatically when it detects a mismatch.

## Enable Cloud Chat

Ollama can remain enabled for local embeddings while `Ask` uses a stronger cloud chat model. In the Unreal plugin, open:

```txt
Edit -> Project Settings -> Plugins -> Game Production AI Assistant
```

Then:

1. Set `Chat Provider` to `OpenAICompatible`.
2. Set `Open AI Base Url`, `Open AI Model`, and `Open AI Api Key`.
3. Use `Test Prompt IA` in the plugin Project Settings page to validate authentication, model access, and quota.
4. Use `Ask`. The plugin sends the saved settings to the local backend before the question.

The backend applies saved chat settings without a restart. Settings are stored locally under the backend storage root. The API key field accepts the secret key itself, usually starting with `sk-` or `sk-proj-`; do not paste the provider dashboard URL.

Common cloud chat errors:

- `Cloud chat authentication failed`: the API key is missing, invalid, or belongs to the wrong provider/project.
- `Cloud chat quota exceeded`: the provider account, project budget, or billing plan has no available quota.
- `Cloud chat rate limit reached`: wait briefly, reduce request frequency, or increase provider limits.

You can also configure defaults in `GPAA.Api/appsettings.json`:

```json
{
  "Chat": {
    "Provider": "OpenAICompatible"
  },
  "OpenAICompatible": {
    "BaseUrl": "https://api.openai.com/v1",
    "Model": "gpt-4.1-mini",
    "ApiKey": "",
    "ApiKeyEnvironmentVariable": "OPENAI_API_KEY",
    "TimeoutSeconds": 120,
    "MaxOutputTokens": 512,
    "Temperature": 0.2
  }
}
```

Prefer environment variables over storing secrets in `appsettings.json`:

```powershell
$env:OPENAI_API_KEY = "your-api-key"
```

Restart the backend only when changing defaults in `appsettings.json`. Settings changed from the Unreal plugin are applied immediately.

## Check Runtime Status

The backend exposes a runtime status endpoint:

```powershell
curl.exe http://localhost:5000/runtime/status
```

The Unreal plugin Project Settings page includes a `Check Backend` action. It reports whether the backend is reachable, which storage path is used, whether Ollama mode is enabled, and which Ollama models are installed.

The Unreal plugin also attempts to start the local backend automatically before workflows that need it, including `Refresh Knowledge`, `Search`, `Ask`, history loading, and knowledge status loading. Auto-start is enabled by default from:

```txt
Edit -> Project Settings -> Plugins -> Game Production AI Assistant
```

Packaged installs include `Resources/Backend/GPAA.Api.exe`, so users do not need the GitHub repository or a `GPAA.Api.csproj` path. `Backend Project Path` is only a developer fallback for source checkout installs.

When `Stop Backend On Editor Shutdown` is enabled, the plugin stops the backend during a clean Unreal shutdown only if this Unreal session launched it. The launched backend also monitors Unreal's process id and shuts itself down if the editor process exits before the plugin shutdown callback can run. A backend that was already running before Unreal opened is left alone.

## Project Knowledge

The Unreal plugin includes a `Project Knowledge` section with the current local index state: indexed files, chunks, synced Unreal assets, indexed asset metadata documents, embedding model, and last index timestamp.

Use `Refresh Knowledge` when you want to force an immediate Unreal Asset Registry sync and incremental index refresh. `Search` and `Ask` still refresh automatically, but the manual button is useful when you want to inspect whether the backend sees the latest project state before asking a question.

## Privacy

The backend runs locally. Indexed files and local embeddings stay on your workstation. If you configure a cloud chat provider, `Ask` sends the selected source snippets and the question to that provider.

## Unreal Editor Plugin

See [Unreal Editor Plugin](unreal-plugin.md) for installation and usage.

## Current Limitations

- The Unreal Editor plugin is an initial integration scaffold with separate search, answer, and source sections.
- The backend supports streaming chat over Server-Sent Events; the Unreal plugin still uses the stable non-streaming chat call.
- The storage adapter uses a local JSON store behind a repository interface.
- Unreal asset indexing is metadata-only; deep Blueprint graph and binary asset content parsing are not included yet.
