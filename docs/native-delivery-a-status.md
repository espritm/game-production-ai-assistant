# Native delivery A — implementation and release evidence

Updated: 7 November 2026. Development branch: `codex/native-delivery-a`.

## Implemented baseline

The plugin now runs its project pipeline in Unreal. `GPAACore`, `GPAAInference` and `GPAAssistant` implement local scanning, incremental indexing, metadata retrieval, SQLite persistence, CPU embeddings, source references, local history and direct cloud generation. The editor plugin no longer contains the local HTTP backend client, backend supervisor or backend launch settings. The C# projects remain a standalone development reference in the repository.

The small embedding model is included in native archives. ONNX Runtime 1.20.1 and MiniLM are downloaded only when building, from pinned upstream revisions with SHA256 verification. Native packages contain no EXE/MSI, .NET runtime or companion program. Keys are transient settings values saved explicitly to Windows Credential Manager; profiles exclude keys and require renewed cloud consent when imported.

| Planned lot | Implementation status | Remaining validation |
|---|---|---|
| 0 — Prototype and distribution risks | CPU model integrated, tokenizer and pooling exercised, upstream licenses/notices included, latest UE target identified as 5.8 | Larger real-project and multilingual retrieval evaluation; publisher review of third-party declarations |
| 1 — Modules and contracts | Three compiled modules, task cancellation and shutdown lifecycle | UE 5.8 compilation |
| 2 — Local indexing/storage | Incremental index, source context, bounded Asset Registry metadata, transactional SQLite and explicit legacy-history import | Production-sized projects and multi-editor stress |
| 3 — Local search | Bundled embeddings, model identity versioning, exact vector ranking and Code/Assets result filtering | Broader retrieval quality beyond the controlled corpus |
| 4 — Cloud and secrets | Responses and compatible Chat Completions, incremental SSE, cancellation, provider errors and OS credential store | An opt-in live request with the user's own provider account; CI uses simulated providers |
| 5 — User workflows | Existing Slate panel connected to the native services; first-use instructions, key actions, consent, streaming, cancellation, references and history | Final manual UX acceptance |
| 6 — Native distribution | Precompiled native ZIP, separate Fab source ZIP, dependency/policy checks and updated CI | UE 5.8 and runner availability, clean-machine test, dedicated engine-plugin installation check, actual Fab listing/declaration/review |

These are implemented development deliverables, not an assertion that all external release gates have passed. No Fab submission or publication has been performed.

## Local evidence

Validated locally with Unreal Engine 5.6.1 on Windows 11, Intel Core i5-14600K, 128 GiB system RAM; embeddings use CPU with two inference threads.

- Five controlled retrieval questions: Recall@1 = 5/5. Initial tokenizer and embedding corpus run approximately 0.19 seconds. This is a small functional corpus, not a general code-search benchmark.
- Reproducible synthetic workload: 1000 source files, initial indexing approximately 4.95 seconds; incremental search including refresh approximately 0.10 seconds; 1000/1000 file embeddings reused. These measurements are specific to this generated workload and machine.
- Native automation covers real local HTTP transport against simulated Responses/Chat Completions endpoints, fragmented UTF-8/SSE, incremental reuse/cancellation/model changes, OS credential round trips, secret-free profile export, atomic rollback, local history and offline indexing/retrieval.
- Existing model parsing and settings validation tests remain in the Unreal suite. The obsolete packaged-backend integration test has been removed from the native plugin.
- The final local Unreal run passes 14 tests with zero warnings/failures, including the first-launch Slate panel. Its installed plugin directory remains byte-for-byte unchanged. The synthetic benchmark in this run measured 4.94 seconds for initial indexing and 0.10 seconds for incremental search.
- The retained .NET reference suite passes its 21 tests.
- Native package policy tests reject executable files and corrupted tokenizer bytes.
- Smoke scripts preserve Unreal logs, JSON reports, benchmark data, the exact ZIP hash and a check that the installed plugin directory is unchanged by the run under `.gpaa/native-smoke/<run-id>`.

The test scripts do not make real paid cloud requests. The Windows developer machine has .NET installed, so a passing local test alone is not evidence of a clean-machine installation. The native build no longer publishes or embeds any .NET project; Unreal's own bundled build tooling remains part of the engine installation.

## Remaining release gates

1. Install Unreal 5.8 and validate the source build and smoke suite against that version. Only UE 5.6 is currently installed on this machine. A corresponding `ue-5.8` runner label and `UE_5_8_ROOT` can be used by the updated workflow.
2. Test the exact precompiled archive on a clean user environment without .NET/Ollama and in a dedicated engine-plugin installation location. The current smoke test installs the plugin into a fresh project and checks for writes into the plugin directory.
3. Review search relevance on representative real Unreal projects. The initial embedding model is English; equivalent French-query quality is not yet claimed.
4. Complete the Fab publisher profile, product ID/`FabURL`, third-party software declaration, listing media and user-facing compatibility claims. Validate current Fab requirements before submitting. The source ZIP has a plugin parent folder and excludes Unreal build binaries; it is a review candidate, not an approved store artifact.

## Build and test

```powershell
.\scripts\package-native-plugin.ps1 -UnrealEngineRoot 'C:\Program Files\Epic Games\UE_5.6'
.\scripts\tests\verify-native-package.Tests.ps1
.\scripts\test-native-plugin.ps1 -PackagePath '.gpaa\releases\GameProductionAIAssistant-native-v0.2.0-UE5.6-Win64.zip' -TestFilter GPAA
```

For UE 5.8, pass its installed engine directory and select the matching archive name. Do not advertise compatibility until that build and test have passed.

Sources: [implementation plan](native-plugin-implementation-plan.md), [user guide](native-user-guide.md), [third-party inventory](native-third-party-notices.md), [Fab requirements](https://www.fab.com/o/technical-requirements), [Epic's UE 5.8 release announcement](https://www.unrealengine.com/news/unreal-engine-5-8-is-now-available).
