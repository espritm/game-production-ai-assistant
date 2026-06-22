# Game Production AI Assistant

An open-source, local-first AI assistant for AAA game production pipelines.

## Current Implementation Status

The repository now contains the first local backend foundation for V1:

- ASP.NET Core local API
- project file scanning
- source chunking
- local embedding abstraction
- semantic search
- project-aware chat orchestration
- local runtime status endpoint
- local JSON-backed index repository
- Ollama adapters for embeddings and chat
- Unreal Editor plugin scaffold
- UE 5.6 Win64 plugin build validation
- automated unit test runner
- developer and user documentation

## Quick Start

Build the solution:

```powershell
dotnet build GameProductionAIAssistant.slnx
```

Run tests:

```powershell
dotnet run --project GPAA.Tests\GPAA.Tests.csproj
```

Start the local backend:

```powershell
dotnet run --project GPAA.Api\GPAA.Api.csproj
```

Read the docs:

- [Developer Guide](docs/developer-guide.md)
- [User Guide](docs/user-guide.md)
- [Local Backend API](docs/api.md)
- [Unreal Editor Plugin](docs/unreal-plugin.md)

## Vision

Game Production AI Assistant is a developer tooling platform designed to augment game developers, technical artists, and production teams working with modern game engines such as Unreal Engine.

The project focuses on:

- AI-assisted gameplay programming
- Production tooling
- Asset validation
- Semantic project search
- Automated pipeline assistance
- Local-first AI workflows
- Secure on-premise inference

The goal is to build a production-grade assistant that could realistically integrate into a AAA studio pipeline.

---

## Why This Project Exists

Modern AAA game production involves:

- massive codebases
- thousands of assets
- complex build systems
- technical debt
- onboarding difficulties
- repetitive engineering tasks

Existing AI assistants are often:

- cloud dependent
- not engine-aware
- not pipeline-aware
- unsafe for proprietary codebases

Game Production AI Assistant aims to solve this with:

- local inference
- engine integration
- production-aware tooling
- scalable architecture
- developer-first workflows

---

## Core Objectives

### 1. Local-First AI

No external API required.

The assistant should support:

- llama.cpp
- Ollama
- local embedding models
- offline workflows

This makes the project suitable for enterprise and AAA environments.

---

### 2. Engine-Aware Tooling

The assistant understands:

- Unreal Engine C++
- Gameplay Ability System
- assets
- metadata
- Blueprints
- project structure

Future support:

- Unity
- proprietary engines

---

### 3. Production Pipeline Integration

The assistant should help with:

- asset validation
- naming conventions
- dependency analysis
- build diagnostics
- profiling analysis
- crash log analysis

---

### 4. Developer Productivity

The project should reduce repetitive tasks:

- boilerplate generation
- gameplay scaffolding
- documentation lookup
- semantic code search
- commit review assistance

---

## Example Use Cases

### Semantic Search

```txt
Where is enemy perception initialized?
```

### Gameplay Boilerplate Generation

```txt
Create a replicated dash ability using GAS.
```

### Asset Validation

```txt
Find textures larger than 4K without mipmaps.
```

### Build Diagnostics

```txt
Analyze this Unreal packaging failure.
```

---

## High-Level Architecture

```txt
+------------------------------------------------------+
| Unreal Engine Plugin                                 |
|------------------------------------------------------|
| - Slate UI                                           |
| - Editor Integration                                 |
| - Asset Context                                      |
| - Gameplay Helpers                                   |
+-----------------------+------------------------------+
                        |
                        v
+------------------------------------------------------+
| Backend Orchestrator (.NET 8)                        |
|------------------------------------------------------|
| - AI orchestration                                   |
| - Embedding pipeline                                 |
| - Semantic search                                    |
| - Project indexing                                   |
| - Task execution                                     |
+-----------------------+------------------------------+
                        |
                        v
+------------------------------------------------------+
| Local AI Runtime                                     |
|------------------------------------------------------|
| - llama.cpp                                          |
| - Ollama                                             |
| - Embedding models                                   |
+------------------------------------------------------+
```

---

## Technology Stack

### Core

- C++
- C# / .NET 8
- Unreal Engine
- SQLite
- REST/gRPC

### AI

- llama.cpp
- Ollama
- sentence-transformers
- local embeddings

### Parsing & Indexing

- tree-sitter
- Clang tooling

### Future

- vector databases
- distributed indexing
- CI integrations

---

## Roadmap

### V1 — Foundation

- Unreal plugin
- local chat
- project indexing
- semantic search
- local inference support

### V2 — Production Tooling

- asset validation
- gameplay code generation
- dependency graph analysis
- build diagnostics

### V3 — Studio Pipeline

- CI integration
- multi-user knowledge base
- automated review assistant
- profiling analysis

---

## Open Source Goals

The project aims to demonstrate:

- production-grade architecture
- clean tooling design
- modern C++ practices
- scalable backend systems
- AI integration in game production

---

## Target Audience

- Gameplay programmers
- Engine programmers
- Technical artists
- Tools engineers
- Build engineers
- Technical directors

---

## Non-Goals

This project is **not**:

- a cloud SaaS
- a generic chatbot
- a game engine replacement
- a simple Copilot clone

The focus is production tooling for game development.

---

## Long-Term Vision

Become an extensible AI tooling platform for game production pipelines.

Potential future directions:

- multi-engine support
- autonomous pipeline agents
- AI-assisted debugging
- automated optimization suggestions
- production analytics

---

## License

MIT

---

## Status

Early architecture phase.
