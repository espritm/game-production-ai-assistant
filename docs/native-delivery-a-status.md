# Native delivery A — implementation and release evidence

Updated: 7 November 2026. Development branch: `codex/native-delivery-a`.

## Implemented baseline

The plugin now runs its project pipeline in Unreal. `GPAACore`, `GPAAInference` and `GPAAssistant` implement local scanning, incremental indexing, metadata retrieval, SQLite persistence, CPU embeddings, source references, local history and direct cloud generation. The editor plugin no longer contains the local HTTP backend client, backend supervisor or backend launch settings. The C# projects remain a standalone development reference in the repository.

The small embedding model is included in native archives. ONNX Runtime 1.20.1 and MiniLM are downloaded only when building, from pinned upstream revisions with SHA256 verification. Native packages contain no EXE/MSI, .NET runtime or companion program. Keys are transient settings values saved explicitly to Windows Credential Manager; profiles exclude keys and require renewed cloud consent when imported.

| Planned lot | Implementation status | Remaining validation |
|---|---|---|
| 0 — Prototype and distribution risks | CPU model integrated, tokenizer and pooling exercised, upstream licenses/notices included, latest UE target identified as 5.8 | Larger real-project and multilingual retrieval evaluation; publisher review of third-party declarations |
| 1 — Modules and contracts | Three compiled modules, task cancellation and shutdown lifecycle | UE 5.8.4 compilation and 14-test suite passed locally |
| 2 — Local indexing/storage | Incremental index, source context, bounded Asset Registry metadata, transactional SQLite and explicit legacy-history import | Production-sized projects and multi-editor stress |
| 3 — Local search | Bundled embeddings, model identity versioning, exact vector ranking and Code/Assets result filtering | Broader retrieval quality beyond the controlled corpus |
| 4 — Cloud and secrets | Responses and compatible Chat Completions, incremental SSE, cancellation, provider errors and OS credential store | An opt-in live request with the user's own provider account; CI uses simulated providers |
| 5 — User workflows | Existing Slate panel connected to the native services; first-use instructions, key actions, consent, streaming, cancellation, references and history | Final manual UX acceptance |
| 6 — Native distribution | Precompiled native ZIP, separate Fab source ZIP, dependency/policy checks and updated CI | UE 5.8.4 passed locally; clean-machine test, actual Fab listing/declaration/review |

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

## UE 5.8.4 validation

Validated on 7 November 2026 using `C:\Program Files\Epic Games\UE_5.8` (engine changelist 58902052). Native BuildPlugin completed without code changes. Both distribution policies and the negative executable/tokenizer-integrity checks passed. The exact Windows archive passed all 14 Unreal tests; the installed plugin remained unchanged. No paid cloud request was made.

- Windows archive SHA256: `2C45EE7163726C91B99E492A7806512119EEC6A044956E4BC9A7AAE0DADA171D`.
- Local evidence: `.gpaa/native-smoke/a95a49a25e2d47cdbabfbbe1fef6f997/`.
- UE 5.6 CI and reference .NET CI also passed for implementation commit `9e4957c` ([Unreal run](https://github.com/espritm/game-production-ai-assistant/actions/runs/42316608324), [.NET run](https://github.com/espritm/game-production-ai-assistant/actions/runs/42316608321)). UE 5.8.4 results above are local validation.

## Engine-plugin installation validation

The UE 5.8 Windows ZIP was installed into `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Marketplace\GameProductionAIAssistant`. A fresh host project loaded this engine installation and passed all 14 tests, with zero warnings and no changes to the installed plugin files. Evidence: `.gpaa/native-smoke/4eca4d158d104372b5327d42fa069986/`. The engine installation is retained for manual acceptance; the smoke script refuses to overwrite an existing installation.

Repeat on an engine without an existing GPAA installation with `scripts/test-native-plugin.ps1 -UnrealEngineRoot <engine> -PackagePath <zip> -TestFilter GPAA -EngineInstallation`. The default remains a project-plugin installation. This does not establish clean-machine acceptance.

The available `C:\Workspace\Unreal-test-proj\test_proj` has 434 assets and no project C++ source directory. It cannot establish retrieval relevance on a representative game codebase. The owner selected this project for the asset evaluation below.

A [store listing draft](fab-listing-draft.md) now records product copy, limitations, publisher inputs and the demonstration capture plan. No listing has been submitted.

## Real project asset evaluation

The owner authorized `C:\Workspace\Unreal-test-proj\test_proj`. Content was copied into an isolated host; the original project was not migrated or modified. The host uses the original descriptor's enabled engine plugins. An initial minimal-plugin host could not register several animation assets (unknown custom versions); that attempt is invalid for relevance measurement and was superseded.

With the normal engine plugins enabled, UE 5.8.4 registered 436 asset records (the project has 434 `.uasset` files; registry records also include map objects). Native metadata indexing completed in 11.35 seconds. The exact archive passed 15 tests with zero warnings and unchanged installed plugin files. Evidence: `.gpaa/native-smoke/6d44a8acea324893a9bc6a2c7a23d5ea/`, including `Host/Saved/GPAA/Acceptance/asset-search.json`.

| Query | Expected asset | Rank within top 10 |
|---|---|---|
| pistol reload animation | MM_Pistol_Reload | 1 |
| rifle equip animation | MM_Rifle_Equip | 2 |
| death animation falling backwards | MM_Death_Back_01 | 2 |
| animation de rechargement du pistolet | MM_Pistol_Reload | 4 |
| animation pour equiper le fusil | MM_Rifle_Equip | Absent |
| animation de mort vers l'arriere | MM_Death_Back_01 | Absent |

On this small, manually selected set, English Recall@3 is 3/3 and French Recall@10 is 1/3. These are functional observations on asset metadata, not general benchmark claims or validation of C++ retrieval. The acceptance test reports ranks independently of technical test success; an absent expected registry record invalidates the experiment. No cloud request was made. The final acceptance archive SHA256 is `732F2A29CC206E085958B359253DC3555F035EC38F85DF6DA4B2861E9A7D8A59`. The simulated HTTP tests now request an OS-assigned ephemeral port rather than choosing a random port, fixing an intermittent Windows bind failure.

Reproduce with `scripts/test-native-plugin.ps1 -UnrealEngineRoot <engine> -PackagePath <zip> -TestFilter GPAA -AcceptanceProject <project-directory>`. This optional acceptance case is not registered in normal CI runs without real assets. The script copies Content and uses enabled engine plugins; it does not copy custom project plugins or convert project code.

## Independent-machine kit and submission preparation

A portable acceptance kit is generated by `scripts/new-clean-machine-kit.ps1`. It contains the exact precompiled ZIP, a SHA256 manifest, standalone smoke scripts and manual acceptance instructions. It requires an existing matching Unreal installation but no source repository, build-time dependency downloads or paid API calls. See [clean-machine acceptance](clean-machine-acceptance.md).

The kit has been extracted and exercised locally with the 14 standard tests. Final portable validation evidence: `.gpaa/kit-final/.gpaa/native-smoke/15753a901a7e4d9fbf0c58015f0a12ca/`. Current Windows ZIP SHA256: `71D28DB7DF29E027397A8A677BBBE4273AFBD8266B48D9EE79C76407A7F4967C`. Transfer kit: `.gpaa/acceptance-kits/10b2b8319e7d433d8132250cedf47126.zip`, SHA256 `AF27CC145CA88E370077CCD7CF5C1A4970BBC9AAAC19E406D6D472DAA7C005EF`. This developer machine has .NET, Ollama and Python on PATH; it is not an independent clean environment. Windows denies this session Hyper-V management access, so no VM acceptance is claimed. The result explicitly records `cleanMachineAttested=false`.

The [submission preparation record](fab-submission-readiness.md) covers current evidence, runtime DLL inspection, media specifications checked against Epic documentation and outstanding owner inputs. Standard plugin directories are now explicitly retained in ZIPs even when empty. Publisher identity, support contact and pricing are confirmed: Maxime ESPRIT, contact@maximeesprit.fr, free. The descriptor author and listing draft are updated; store license selection and store-assigned URLs remain pending. Previously generated archives and the transfer kit retain their validated bytes; rebuild them to include these metadata/documentation updates.

## Manual acceptance fix — opening asset references

The owner confirmed indexing and retrieval in the prepared real-asset host, then reported that **Open** only displayed the inherited virtual-metadata message. The opener previously returned success without activating an asset editor. It now resolves native `asset:/Game/...` object paths (and legacy metadata object paths), validates them and opens the object through Unreal's AssetEditorSubsystem. Invalid references return failure; code references retain their external-editor behavior.

A regression test covers native/legacy path resolution, malformed references and actual editor activation/closure with a transient curve asset. All 15 standard tests pass locally on UE 5.8.4 with zero warnings. Evidence: `.gpaa/native-smoke/1b2d3a39982f42b593240644b1e7bdbf/`. Windows archive SHA256: `E9D72B31B82F7F32FE7569CDE9B3ECFCFA2747B8CD09DBA3D742ADFA1562799D`.

After the owner closed Unreal, this archive was installed into the existing manual host `.gpaa/native-smoke/6d44a8acea324893a9bc6a2c7a23d5ea/Host/Plugins/GameProductionAIAssistant`, with installed file hashes checked against the package and the previous plugin backed up under `.gpaa/manual-plugin-backups`. The original project and engine installation were not modified. The owner subsequently confirmed that the animation is found and opens correctly in Unreal in the distinctly named GPAA_Manual project, and that retrieval persists after restarting the editor. Earlier transfer kits refer to the older validated archives and must be regenerated for final distribution.

## Manual acceptance fix — personal key actions in Settings

On 8 November 2026 the owner reported that Ask could not find a saved key despite a populated password field. The Settings screenshot showed no Save Personal Key action. `CallInEditor` functions alone were not displayed in the developer-settings view. An explicitly registered detail customization now provides save/remove key, diagnostic and profile/history actions, with a visible status message. Personal non-secret preferences use user configuration instead of forcing the shared default config; the password field remains transient and is never serialized. Cloud consent remains an explicit user choice.

The new offscreen PropertyEditor regression constructs the actual Settings detail view, generates its virtualized rows and verifies that both credential buttons exist. All 16 standard tests pass on UE 5.8.4 with zero warnings, without paid cloud requests. Evidence: `.gpaa/native-smoke/060d8cde7a63405eb1a399113e72b0a3/`. Windows ZIP SHA256: `E6DE4395F159201E88CA64211501325526C4CD61059A2FEC62ADA91751B62974`.

After the owner closed Unreal, this package was installed in `.gpaa/manual-acceptance/Plugins/GameProductionAIAssistant`, with a backup under `.gpaa/manual-plugin-backups` and installed file hashes verified. Manual credential saving and cloud generation are pending owner retest. No real key was accessed by the development tools.

## Automatic indexing and exclusive history view — 8 November 2026

The owner confirmed cloud answers grounded in the correct real-asset project and local conversation history retained after restarting Unreal. At the owner's request, the History overlay is replaced by an exclusive panel view with Back navigation. Main search/chat state is preserved; the two views cannot overlap. The manual Refresh Knowledge button and Project Knowledge box have been removed.

An editor-module ticker refreshes the local index once the initial Asset Registry scan is ready, then every minute. It runs without the assistant panel. Each search/question also refreshes changed files as before. Periodic work checks the shared runtime's complete busy state, including cloud response/history persistence, and defers rather than cancelling foreground work. Foreground requests retain priority and can cancel an in-progress background index. Closing the editor removes the ticker before shutting down the runtime. Background indexing updates neither the displayed answer nor the search results and makes no provider request.

All 18 standard tests pass locally on UE 5.8.4, with no warnings and unchanged installed plugin files. New coverage checks History/Back state preservation, removed UI controls and background indexing without a panel, including non-preemption of an active search. Evidence: `.gpaa/native-smoke/e5a00eb3710b4f3298effb13d768320b/`.

An additional real-editor smoke run opens a fresh host without an assistant panel or automation commands. It observes initial indexing, adds one source file, and verifies that the next automatic cycle picks it up. This passed: 3 initial files, 4 files after the next cycle, 58.41 seconds after observing the first completion. Evidence: `.gpaa/automatic-index-smoke/53252e74b80b4e01aaedb3c80c1a4cf9/result.json`. The periodic deadline is 60 seconds after starting the preceding refresh; observation happens after that refresh completes. This smoke is included in Unreal CI. Windows archive SHA256: `7FF059F83AA637B4C98BC713C9C6045929F7E5734785BB04DFE59D65AA17FB98`.

CI subsequently reached the tests and exposed an engine window-restoration fatal error when opening a native asset toolkit in the Windows service session. Reference validation remains in the standard suite; native asset-window activation is now an explicit opt-in acceptance test (`test-native-plugin.ps1 -InteractiveAssetEditor`) for an interactive desktop, not silently skipped within a successful test. That activation had already passed locally on UE 5.8.4 and the owner confirmed real animation opening. The offscreen settings/history checks remain in CI. The previous CI attempt failed before compilation on an upstream HTTP 504 downloading ONNX Runtime. Pinned dependency downloads now retry up to three times; the same mandatory hash verification remains in place. Repeated HTTP 504 responses required seeding this runner's tool cache with the already downloaded, lock-file-verified upstream assets. CI now retains that cache outside checkout cleanup via `GPAA_NATIVE_DEPENDENCY_CACHE`; absent or invalid entries still use the pinned download and mandatory hash verification. No live key was read or paid cloud request made during development tests. After the owner closed Unreal, the verified archive was installed in `.gpaa/manual-acceptance/Plugins/GameProductionAIAssistant`; the previous plugin was backed up and installed file hashes were verified. Existing local data and credentials were preserved.

## Remaining release gates

1. UE 5.8.4 source compilation, archive installation and all 14 Unreal tests now pass locally. To repeat this validation in CI, configure the corresponding `ue-5.8` runner label and optionally `UE_5_8_ROOT`; the workflow supports selecting 5.8 manually.
2. Test the exact precompiled archive on a clean user environment without .NET/Ollama. Project-plugin and engine-plugin installation tests now pass on the developer machine; that machine still has development tooling installed.
3. Extend the small real-asset evaluation below to representative game C++ source and more queries. The initial embedding model is English; equivalent French-query quality is not claimed.
4. Complete the Fab publisher profile, product ID/`FabURL`, third-party software declaration, listing media and user-facing compatibility claims. Validate current Fab requirements before submitting. The source ZIP has a plugin parent folder and excludes Unreal build binaries; it is a review candidate, not an approved store artifact.

## Build and test

```powershell
.\scripts\package-native-plugin.ps1 -UnrealEngineRoot 'C:\Program Files\Epic Games\UE_5.6'
.\scripts\tests\verify-native-package.Tests.ps1
.\scripts\test-native-plugin.ps1 -PackagePath '.gpaa\releases\GameProductionAIAssistant-native-v0.2.0-UE5.6-Win64.zip' -TestFilter GPAA
```

For UE 5.8, pass its installed engine directory and select the matching archive name. Do not advertise compatibility until that build and test have passed.

Sources: [implementation plan](native-plugin-implementation-plan.md), [user guide](native-user-guide.md), [third-party inventory](native-third-party-notices.md), [Fab requirements](https://www.fab.com/o/technical-requirements), [Epic's UE 5.8 release announcement](https://www.unrealengine.com/news/unreal-engine-5-8-is-now-available).
