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

Source rows can be opened through the plugin's configurable source opener. The default mode is `Auto`, which tries common local editor installations before falling back to the Windows default application.

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

## Ask a Question

```powershell
curl.exe -X POST http://localhost:5000/chat `
  -H "Content-Type: application/json" `
  -d "{\"projectRoot\":\"C:\\Path\\To\\YourUnrealProject\",\"question\":\"Where is stamina regeneration handled?\",\"topK\":5}"
```

The response includes an answer and the source chunks used as context. In the Unreal plugin, the answer and its sources are displayed separately so that source files can be opened without digging through the generated text.

The backend also exposes `POST /chat/stream` for streaming answer fragments as Server-Sent Events. The current Unreal plugin keeps the stable non-streaming request path, while the backend API is ready for streaming clients.

## Chat History

Each completed `Ask` request is stored locally in JSON under the backend storage root. The Unreal plugin includes a `History` button that loads recent local questions and answers for the current project.

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

## Check Runtime Status

The backend exposes a runtime status endpoint:

```powershell
curl.exe http://localhost:5000/runtime/status
```

The Unreal plugin also includes a `Check Backend` button. It reports whether the backend is reachable, which storage path is used, whether Ollama mode is enabled, and which Ollama models are installed.

## Privacy

The backend runs locally. Indexed files, embeddings, and prompts stay on your workstation unless you explicitly configure a remote inference provider later.

## Unreal Editor Plugin

See [Unreal Editor Plugin](unreal-plugin.md) for installation and usage.

## Current Limitations

- The Unreal Editor plugin is an initial integration scaffold with separate search, answer, and source sections.
- The backend supports streaming chat over Server-Sent Events; the Unreal plugin still uses the stable non-streaming chat call.
- The storage adapter uses a local JSON store behind a repository interface.
- Blueprint and asset metadata indexing are not included in this version.
