# Developer Guide

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

Supported source files:

- `.h`
- `.hpp`
- `.cpp`
- `.ini`
- `.uplugin`
- `.uproject`

Ignored directories:

- `.git`
- `.vs`
- `Binaries`
- `DerivedDataCache`
- `Intermediate`
- `Saved`

### Semantic Search

`SemanticSearchService` embeds the user query and ranks indexed chunks with cosine similarity.

### Project Chat

`ProjectChatService` retrieves semantic context, builds a grounded prompt, and calls the configured chat provider.

## Storage

The current V1 implementation uses `LocalKnowledgeRepository`, an atomic JSON-backed local store. JSON is the preferred storage format for the current phase because it is simple to inspect, easy to debug, and dependency-free.

Default storage path:

```txt
.gpaa/indexes
```

## Inference Providers

Two provider modes exist.

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
- end-to-end indexing and semantic search.

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

See [Unreal Editor Plugin](unreal-plugin.md).

## Adding SQLite Later

To add SQLite, create a new adapter in `GPAA.Infrastructure.Storage` that implements `IKnowledgeRepository`.

The application layer should not change.

Suggested implementation:

- create tables for projects, indexed files, chunks, embeddings, and index runs;
- store vectors as binary blobs or normalized JSON;
- keep writes transactional;
- replace `LocalKnowledgeRepository` registration in `GPAA.Api/Program.cs`.
