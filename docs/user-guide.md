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

## Index a Project

Send a request to the local backend:

```powershell
curl.exe -X POST http://localhost:5000/projects/index `
  -H "Content-Type: application/json" `
  -d "{\"projectRoot\":\"C:\\Path\\To\\YourUnrealProject\"}"
```

The backend scans supported files and stores a local index under:

```txt
.gpaa/indexes
```

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

## Ask a Question

```powershell
curl.exe -X POST http://localhost:5000/chat `
  -H "Content-Type: application/json" `
  -d "{\"projectRoot\":\"C:\\Path\\To\\YourUnrealProject\",\"question\":\"Where is stamina regeneration handled?\",\"topK\":5}"
```

By default, chat runs in echo mode so you can validate indexing and context retrieval without installing a model.

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

- The Unreal Editor plugin is an initial integration scaffold.
- Streaming responses are not implemented yet.
- The storage adapter uses a local JSON store behind a repository interface.
- Blueprint and asset metadata indexing are not included in this version.
