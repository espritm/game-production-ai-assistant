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

## `POST /projects/assets/metadata`

Synchronizes Unreal Asset Registry metadata collected by the Unreal Editor plugin. The backend stores this metadata locally and indexes it as virtual asset metadata documents during the next automatic index refresh.

Request:

```json
{
  "projectRoot": "C:\\Path\\To\\Project",
  "assets": [
    {
      "objectPath": "/Game/UI/WBP_Inventory.WBP_Inventory",
      "packageName": "/Game/UI/WBP_Inventory",
      "packagePath": "/Game/UI",
      "assetName": "WBP_Inventory",
      "assetClass": "WidgetBlueprint",
      "tags": {
        "ParentClass": "UserWidget"
      },
      "dependencies": [
        "/Game/UI/WBP_InventoryStyle"
      ],
      "referencers": [
        "/Game/Maps/L_DebugInventory"
      ]
    }
  ]
}
```

## `GET /projects/knowledge/status`

Returns the current local knowledge state for a project, including index statistics and Unreal Asset Registry metadata counts.

Query:

```txt
projectRoot=C:\Path\To\Project
```

Response:

```json
{
  "statistics": {
    "filesScanned": 42,
    "filesIndexed": 42,
    "chunksIndexed": 120,
    "indexedAt": "2026-06-25T20:42:00Z",
    "filesReused": 42,
    "filesChanged": 0,
    "filesRemoved": 0,
    "chunksReused": 120,
    "chunksEmbedded": 0,
    "fullRebuild": false,
    "embeddingModel": "ollama:nomic-embed-text"
  },
  "embeddingModel": "ollama:nomic-embed-text",
  "codeFilesIndexed": 28,
  "assetsSynced": 14,
  "assetDocumentsIndexed": 14,
  "hasIndex": true
}
```

## `GET /runtime/chat-settings`

Returns the current local chat provider settings used by `Ask`.

Sensitive API keys are masked in responses.

## `POST /runtime/chat-settings`

Updates the local chat provider settings. The backend applies these settings to subsequent `Ask` requests without a restart.

Request:

```json
{
  "provider": "OpenAICompatible",
  "openAIBaseUrl": "https://api.openai.com/v1",
  "openAIModel": "gpt-4.1-mini",
  "openAIApiKey": "your-api-key",
  "openAIApiKeyEnvironmentVariable": "OPENAI_API_KEY",
  "timeoutSeconds": 120,
  "maxOutputTokens": 512,
  "temperature": 0.2
}
```

## `POST /runtime/chat-provider/test`

Sends a small health-check prompt to the currently saved chat provider. Use this endpoint to validate authentication, model access, timeout, and quota before asking a project question.

Response:

```json
{
  "provider": "OpenAICompatible",
  "model": "gpt-4.1-mini",
  "message": "GPAA chat provider ready.",
  "testedAt": "2026-06-25T20:42:00Z"
}
```

Provider failures are returned as Problem Details. Authentication, quota, and rate-limit failures are normalized into user-readable `detail` messages.

## `POST /search`

Runs semantic search against an indexed project.

Before ranking results, the backend refreshes the project index incrementally. Unchanged files reuse existing chunks and embeddings; changed files are re-embedded.

Request:

```json
{
  "projectRoot": "C:\\Path\\To\\Project",
  "query": "Where is stamina regeneration handled?",
  "topK": 5,
  "sourceFilter": "All"
}
```

`sourceFilter` accepts `All`, `Code`, or `Assets`. It defaults to `All`.

Response:

```json
[
  {
    "chunkId": "hash:1:80",
    "relativePath": "Source/Game/StaminaComponent.cpp",
    "startLine": 1,
    "endLine": 80,
    "content": "...",
    "score": 0.82,
    "sourceType": "Code"
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
  "topK": 5,
  "sourceFilter": "All"
}
```

`sourceFilter` accepts the same values as `POST /search`.

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
