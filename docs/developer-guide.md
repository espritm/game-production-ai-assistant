# Developer Guide

This document describes the retained .NET reference implementation. The current editor plugin is native C++; see [Native user guide](native-user-guide.md), [migration plan](native-plugin-implementation-plan.md) and [delivery status](native-delivery-a-status.md). The native distribution does not include the backend client or supervisor described below.

## Architecture

Game Production AI Assistant V1 is built as a local-first backend that runs on the user's workstation.

The backend follows a layered architecture:

```txt
GPAA.Api
  HTTP endpoints and composition root

GPAA.Application
  Use cases and ports

GPAA.Core
  Domain model and pure domain services

GPAA.Infrastructure
  Filesystem, local storage, embeddings, and inference adapters
```

Dependencies point inward:

```txt
Api -> Application -> Core
Api -> Infrastructure -> Application/Core
Infrastructure -> Application/Core
```

The domain does not depend on ASP.NET, storage, Ollama, or the filesystem.

## Design Principles

The codebase is intentionally small and explicit.

- **SOLID**: use cases depend on interfaces, adapters implement those interfaces.
- **KISS**: the V1 pipeline favors clear, testable components over early abstractions.
- **DRY**: shared rules such as supported file extensions and vector math live in domain services.
- **DDD**: project indexing, source chunks, index statistics, and semantic results are represented as domain concepts.

## Main Use Cases

### Project Indexing

`ProjectIndexingService` scans supported files, chunks content, generates embeddings, and persists a project index.

Indexing is incremental. The JSON index stores `ProjectIndexMetadata`, including the embedding model id used to generate vectors. When the current provider model id matches the stored metadata, unchanged files are detected by content hash and their chunks are reused. If the model id changes, the service performs a full rebuild so vector dimensions and semantic meaning cannot drift across providers.

`SourceContextExtractor` adds lightweight Unreal/C++ context before embedding chunks. It extracts nearby class, struct, enum, Unreal macro, function, and `.Build.cs` module rule declarations so semantic search has stronger signals than raw line chunks alone.

Supported source files:

- `.h`
- `.hpp`
- `.cpp`
- `.cs`
- `.ini`
- `.uplugin`
- `.uproject`

Supported Unreal asset metadata files:

- `.uasset`
- `.umap`

Binary Unreal assets are not parsed deeply in V1. `UnrealAssetMetadataExtractor` creates a synthetic indexable document from path metadata: asset name, extension, folder, relative path, and inferred asset type. Inferred types include Blueprint, Widget Blueprint, Map, Material, Material Instance, Texture, Static Mesh, Skeletal Mesh, Data Asset, Data Table, and Animation assets.

When the Unreal plugin is used, it also syncs Asset Registry metadata before `Search` and `Ask`. The backend stores this data through `IUnrealAssetMetadataRepository` and indexes it as virtual `.gpaa/unreal-assets/*.asset-metadata` documents. These documents include object path, package name, package path, asset name, asset class, a bounded set of Asset Registry tags such as generated class or parent class when Unreal exposes them, and package dependency/referencer relationships for assets under `/Game`.

Ignored directories:

- `.git`
- `.vs`
- `Binaries`
- `DerivedDataCache`
- `Intermediate`
- `Saved`

### Semantic Search

`SemanticSearchService` refreshes the project index first, embeds the user query, and ranks indexed chunks with cosine similarity. The refresh delegates to `ProjectIndexingService`, so modified files are embedded before results are returned.

### Project Chat

`ProjectChatService` retrieves semantic context, builds a grounded prompt, and calls the configured chat provider.

Completed chat requests are stored through `IChatHistoryRepository`. The V1 adapter is `LocalChatHistoryRepository`, which keeps recent entries in JSON next to the project index.

`IStreamingChatCompletionProvider` exposes streaming chat. `OllamaChatCompletionProvider` reads Ollama's streaming JSON lines, and `POST /chat/stream` forwards fragments as Server-Sent Events.

## Storage

The current V1 implementation uses `LocalKnowledgeRepository`, an atomic JSON-backed local store. JSON is the preferred storage format for the current phase because it is simple to inspect, easy to debug, and dependency-free.

The repository remains backward-compatible with earlier JSON index files. Legacy indexes without metadata are loaded as `legacy-unknown` and rebuilt on the next indexing run.

Default storage path:

```txt
.gpaa/indexes
```

## Inference Providers

Two provider modes exist.
`Chat.Provider` selects the chat provider when configured. If it is empty, the backend keeps backward compatibility by using Ollama when `Ollama.UseOllamaChat` is true and echo mode otherwise.

### Default Offline Safe Mode

By default, the backend uses:

- `DeterministicEmbeddingProvider`
- `ContextEchoChatCompletionProvider`

This mode requires no model installation and keeps tests deterministic.

### Ollama Mode

Set these values in `GPAA.Api/appsettings.json`:

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

### OpenAI-Compatible Cloud Chat

Set `Chat.Provider` to `OpenAICompatible` to route `Ask` through a cloud or gateway API that implements the `/v1/chat/completions` shape. Embeddings can still use Ollama locally.

```json
{
  "Chat": {
    "Provider": "OpenAICompatible"
  },
  "OpenAICompatible": {
    "BaseUrl": "https://api.openai.com/v1",
    "Model": "gpt-4.1-mini",
    "ApiKeyEnvironmentVariable": "OPENAI_API_KEY",
    "TimeoutSeconds": 120,
    "MaxOutputTokens": 512,
    "Temperature": 0.2
  }
}
```

The first cloud implementation is intentionally narrow: it depends only on `IChatCompletionProvider`, keeps secrets out of source by default, and can be replaced by direct Anthropic, Azure OpenAI, or other adapters without changing application services.

Runtime chat settings can be changed from the Unreal plugin through `/runtime/chat-settings`; no backend restart is required for those changes. `/runtime/chat-provider/test` sends a small provider health-check prompt and is the preferred way to validate authentication, model access, quota, and timeout before running a full project `Ask`.

Cloud provider failures should be normalized at the infrastructure boundary. `OpenAICompatibleChatCompletionProvider` turns common provider errors such as invalid API keys, insufficient quota, and rate limits into user-readable `InvalidOperationException` messages. API endpoints convert those exceptions to Problem Details responses with gateway-style status codes.

## Build

```powershell
dotnet build GameProductionAIAssistant.slnx
```

## Tests

The repository currently uses a lightweight console test runner to avoid external NuGet dependencies in the bootstrap phase.

```powershell
dotnet run --project GPAA.Tests\GPAA.Tests.csproj
```

Current coverage includes:

- source chunking with overlap;
- vector similarity ranking;
- Unreal generated directory filtering;
- Unreal asset metadata scanning;
- end-to-end indexing and semantic search;
- incremental index reuse;
- automatic search-time refresh after file changes;
- embedding model mismatch rebuilds;
- local chat history persistence.

## API Project

Run the local backend:

```powershell
dotnet run --project GPAA.Api\GPAA.Api.csproj
```

Default local URL:

```txt
http://localhost:5000
```

The actual port can vary depending on ASP.NET launch settings.

## Unreal Plugin

The Unreal Editor plugin lives under `plugin/`.

It is intentionally thin:

- Slate UI;
- HTTP client;
- menu and tab registration;
- no indexing or inference logic inside the editor process.

The plugin talks to the backend through the public local API. This keeps Unreal code small and avoids duplicating backend behavior in C++.

Editor-facing configuration is centralized in `UGPAAEditorSettings` and registered under:

```txt
Project Settings -> Plugins -> Game Production AI Assistant
```

The settings object uses Unreal editor per-project user settings, so local values persist across editor restarts without forcing secrets into shared project files. It also supports JSON profile import/export for users moving between workstations; API keys are omitted unless explicitly included.

Backend lifecycle checks are handled by `FGPAABackendSupervisor`. UI workflows call it before making backend requests. The supervisor checks `/health`, starts the packaged backend when available, and retries health checks before the original workflow continues. This keeps search, ask, history, knowledge status, and refresh workflows usable after opening Unreal without manually starting the .NET API.

Packaged installs should include the published backend at:

```txt
plugin/Resources/Backend/GPAA.Api.exe
```

Use `scripts/package-unreal-plugin.ps1` to publish the backend and create the Unreal plugin package. The script creates a self-contained Windows backend by default; use `-FrameworkDependent` only for developer packages where the target machine already has the .NET runtime. If no packaged backend is available, the supervisor falls back to developer mode and tries to resolve `GPAA.Api.csproj`.

The supervisor keeps a process handle only for backend processes it launches. `FGPAABackendSupervisor::ShutdownLaunchedBackend()` is called from the plugin module shutdown path and the engine pre-exit delegate, then terminates that owned process when `Stop Backend On Editor Shutdown` is enabled. It intentionally does not terminate pre-existing backend processes.

For robustness, the supervisor also appends `--GpaaParentProcessId <pid>` to launched backend commands. `ParentProcessMonitorService` uses this value to stop the backend if the Unreal editor process disappears before plugin shutdown completes.

See [Unreal Editor Plugin](unreal-plugin.md).

## Adding SQLite Later

To add SQLite, create a new adapter in `GPAA.Infrastructure.Storage` that implements `IKnowledgeRepository`.

The application layer should not change.

Suggested implementation:

- create tables for projects, indexed files, chunks, embeddings, and index runs;
- store vectors as binary blobs or normalized JSON;
- keep writes transactional;
- replace `LocalKnowledgeRepository` registration in `GPAA.Api/Program.cs`.
