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
  "indexedAt": "2026-06-21T00:00:00Z"
}
```

## `GET /projects/index/status`

Returns index statistics for a project.

Query string:

```txt
projectRoot=C:\Path\To\Project
```

## `POST /search`

Runs semantic search against an indexed project.

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

Request:

```json
{
  "projectRoot": "C:\\Path\\To\\Project",
  "question": "Where is stamina regeneration handled?",
  "topK": 5
}
```

Response:

```json
{
  "answer": "..."
}
```
