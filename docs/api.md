# Local Backend API

## `GET /health`

Returns backend health.

Response:

```json
{
  "status": "ok",
  "checkedAt": "2026-06-21T00:00:00Z"
}
```

## `POST /projects/index`

Indexes a local Unreal project.

Request:

```json
{
  "projectRoot": "C:\\Path\\To\\Project"
}
```

## `GET /runtime/status`

Returns local runtime configuration and Ollama availability.

Response:

```json
{
  "backend": "ok",
  "checkedAt": "2026-06-22T12:00:00Z",
  "storageRoot": "C:\\Workspace\\Game-Production-AI-Assistant\\GPAA.Api\\.gpaa\\indexes",
  "ollama": {
    "baseUrl": "http://localhost:11434",
    "chatModel": "llama3.1",
    "embeddingModel": "nomic-embed-text",
    "chatEnabled": true,
    "embeddingsEnabled": true,
    "isReachable": true,
    "installedModels": [
      "llama3.1:latest",
      "nomic-embed-text:latest"
    ],
    "error": null
  }
}
```

Response:

```json
{
  "filesScanned": 42,
  "filesIndexed": 42,
  "chunksIndexed": 120,
  "indexedAt": "2026-06-21T00:00:00Z",
  "filesReused": 40,
  "filesChanged": 2,
  "filesRemoved": 1,
  "chunksReused": 116,
  "chunksEmbedded": 4,
  "fullRebuild": false,
  "embeddingModel": "ollama:nomic-embed-text"
}
```

## `GET /projects/index/status`

Returns index statistics for a project.

Query string:

```txt
projectRoot=C:\Path\To\Project
```

The response uses the same statistics shape as `POST /projects/index`.

## `POST /search`

Runs semantic search against an indexed project.

Before ranking results, the backend refreshes the project index incrementally. Unchanged files reuse existing chunks and embeddings; changed files are re-embedded.

Request:

```json
{
  "projectRoot": "C:\\Path\\To\\Project",
  "query": "Where is stamina regeneration handled?",
  "topK": 5
}
```

Response:

```json
[
  {
    "chunkId": "hash:1:80",
    "relativePath": "Source/Game/StaminaComponent.cpp",
    "startLine": 1,
    "endLine": 80,
    "content": "...",
    "score": 0.82
  }
]
```

## `POST /chat`

Asks a project-aware question.

Chat uses semantic search internally, so it also refreshes the project index incrementally before building context.

Request:

```json
{
  "projectRoot": "C:\\Path\\To\\Project",
  "question": "Where is stamina regeneration handled?",
  "topK": 5
}
```

## `POST /chat/stream`

Streams a project-aware answer as Server-Sent Events. It refreshes the project index before generating context, like `POST /chat`.

Request:

```json
{
  "projectRoot": "C:\\Path\\To\\Project",
  "question": "Where is stamina regeneration handled?",
  "topK": 5
}
```

Response content type:

```txt
text/event-stream
```

Each event contains an answer fragment:

```txt
data: partial answer text
```

## `GET /chat/history`

Returns recent locally stored chat entries for a project.

Query string:

```txt
projectRoot=C:\Path\To\Project&limit=20
```

Response:

```json
{
  "answer": "...",
  "sources": [
    {
      "chunkId": "hash:1:40",
      "relativePath": "Source/Game/StaminaComponent.cpp",
      "startLine": 1,
      "endLine": 40,
      "content": "...",
      "score": 0.82
    }
  ]
}
```
