# Game Production AI Assistant

An open-source, local-first AI assistant for AAA game production pipelines.

## Current Implementation Status

The repository contains the original V1 backend and a native Unreal migration for delivery A. The native plugin embeds local CPU search and connects directly to a user-selected cloud chat provider, without requiring a companion application. UE 5.6 validation is available; UE 5.8 and the remaining Fab release gates are tracked in [Native delivery A status](docs/native-delivery-a-status.md).

The original foundation includes:

- ASP.NET Core local API
- project file scanning
- source chunking
- local embedding abstraction
- semantic search
- Unreal semantic search panel
- project-aware chat orchestration
- structured chat sources
- local runtime status endpoint
- local JSON-backed index repository
- Ollama adapters for embeddings and chat
- Unreal Editor plugin scaffold
- UE 5.6 Win64 plugin build validation
- automated unit test runner
- developer and user documentation

## Quick Start

Build and validate the native plugin (build-time dependency downloads are pinned by SHA256):

```powershell
.\scripts\package-native-plugin.ps1
.\scripts\test-native-plugin.ps1 -PackagePath .gpaa\releases\GameProductionAIAssistant-native-v0.2.0-UE5.6-Win64.zip -TestFilter GPAA
```

Read the [native user guide](docs/native-user-guide.md) and the [implementation plan](docs/native-plugin-implementation-plan.md). For the retained .NET reference backend, use the following commands.

Use the .NET 10 SDK to build the `.slnx` solution. The application targets .NET 8; install the .NET 8 runtime for development runs. Distribution packages include their own runtime.

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
- [Unreal Plugin Packaging CI/CD Roadmap](docs/plugin-packaging-ci-cd.md)

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

### Expanded Roadmap - Unreal Compatibility and MCP

#### V1.x - Unreal Version Compatibility

The tool must remain useful for long-lived productions that cannot freely upgrade Unreal Engine.

- Define a supported Unreal compatibility matrix, starting from the oldest practical UE5 version and extending through the latest validated version.
- Keep a stable core plugin layer that only depends on widely available Unreal Editor, HTTP, and Slate APIs.
- Isolate version-specific Unreal code behind small adapter modules or preprocessor-guarded integration classes.
- Package and validate the plugin per Unreal version instead of assuming that a single binary works everywhere.
- Add automated compile validation for each supported Unreal version available in CI or in local release tooling.
- Document known limitations by engine version, especially for source opening, Asset Registry metadata, Project Settings behavior, and experimental engine APIs.
- Prefer graceful feature degradation: older Unreal versions should keep search, ask, indexing, and settings even when newer automation capabilities are unavailable.

#### V2.x - Unreal MCP Integration

Newer Unreal versions expose an experimental Unreal MCP plugin, identified in engine code as `ModelContextProtocol`. GPAA should use it when available, without making it mandatory for older projects.

- Detect whether Unreal MCP is installed and enabled in the current editor session.
- Add a backend-side MCP client abstraction that can connect to the editor-local MCP server.
- Discover available MCP tools at runtime instead of hard-coding assumptions about experimental tool names.
- Introduce a capability model so the assistant can distinguish read-only workflows from project-modifying workflows.
- Route safe read operations through existing indexing/search paths, and route editor actions through MCP only when available.
- Add an explicit user approval flow before any MCP action that modifies levels, actors, assets, materials, settings, or source-controlled files.
- Record an action history with requested action, tool call, parameters, result, and rollback guidance when possible.
- Start with low-risk editor actions such as inspecting selection, listing actors, running automation tests, and opening editor panels.
- Progressively add controlled write actions such as creating folders, spawning actors in a test level, creating material instances, fixing metadata, and applying simple project settings.
- Keep MCP support optional, experimental, and version-gated until Unreal MCP APIs stabilize.

#### V3.x - Validated Project Automation

- Add project modification proposals with diff previews, test execution, and approval checkpoints.
- Combine semantic search, chat reasoning, Unreal MCP tools, and backend validators in plan-and-execute workflows.
- Provide studio-specific tool packs for coding standards, content rules, build systems, and release workflows.
- Preserve the compatibility strategy when expanding toward multi-engine support.

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

V1 beta implementation. Unreal Engine 5.6 / Win64 packaging and installation smoke-test tooling are available. Automated release publication and broader Unreal version validation remain planned.
