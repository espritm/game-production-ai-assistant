# Unreal Plugin Packaging CI/CD Roadmap

The current Unreal CI job builds the native 0.2.0 package using `package-native-plugin.ps1` and validates it with `test-native-plugin.ps1`. It can select an installed UE 5.6 or UE 5.8 runner via manual input; actual compatibility is tracked in [native delivery status](native-delivery-a-status.md). The embedded .NET backend pipeline described below is the historical 0.1.0 baseline, not the Fab submission artifact for the native edition.

## Purpose

This document defines the path from the current local Unreal plugin packaging workflow to a reproducible CI/CD release pipeline.

The intended distribution channel is **Fab**, the current Epic ecosystem for Unreal Engine plugins and assets. A successful local package is therefore only the starting point: release artifacts must also satisfy the current [Fab Technical Requirements](https://www.fab.com/o/technical-requirements).

## Current Baseline

The repository currently provides `scripts/package-unreal-plugin.ps1`. It:

1. publishes `GPAA.Api` for `win-x64`;
2. embeds the backend under `plugin/Resources/Backend`;
3. produces a self-contained backend by default;
4. runs Unreal Automation Tool `BuildPlugin` for Win64;
5. writes the unpacked plugin package to `.gpaa/PluginPackage`.

The workflow creates a sanitized, verified, versioned zip with a stable file order and timestamps. The separate installation smoke test exercises the exact archive in Unreal Engine 5.6 on Windows. GitHub Release publication is not implemented yet.

## Target Pipeline

```txt
Pull request
  -> .NET build and tests
  -> UE 5.6 plugin build

Version tag (vX.Y.Z)
  -> all required CI checks
  -> self-contained Win64 backend
  -> BuildPlugin
  -> package policy verification
  -> install and smoke test in a minimal Unreal project
  -> versioned zip
  -> GitHub Release artifact
  -> Fab candidate review
```

## Phase 1: Pull Request CI

Create `ci-dotnet.yml` on a GitHub-hosted Windows runner to run:

```powershell
dotnet restore GameProductionAIAssistant.slnx
dotnet build GameProductionAIAssistant.slnx --configuration Release --no-restore
dotnet run --project GPAA.Tests\GPAA.Tests.csproj --configuration Release
```

Add a separate Unreal build job on a Windows self-hosted runner with Unreal Engine 5.6 installed. This job must invoke `BuildPlugin` and retain its logs when it fails.

The implemented `ci-unreal.yml` job expects the runner labels `windows`, `x64`, `unreal`, and `ue-5.6`. It reads the Unreal installation from the runner-level `UE_5_6_ROOT` environment variable and falls back to `C:\Program Files\Epic Games\UE_5.6`. For security, pull requests originating from forks do not execute code on the persistent self-hosted runner.

Formatting and lint checks can be introduced later, once their rules and developer workflow are agreed.

## Phase 2: Tagged Release Packaging

Create `package-unreal-plugin.yml`, triggered by version tags such as `v0.1.0`. The release job must:

1. repeat or depend on all required build and test checks;
2. publish `GPAA.Api` as a self-contained Win64 application;
3. invoke `RunUAT BuildPlugin`;
4. verify the resulting directory before archiving it;
5. create a deterministic, versioned zip;
6. attach the zip to the matching GitHub Release.

The release version and the `VersionName` in `GameProductionAIAssistant.uplugin` must not drift. The workflow should fail when the tag and descriptor disagree.

## Phase 3: Unreal Version Matrix

Start with UE 5.6 as the only release target, then extend the self-hosted runner fleet or runner labels to cover:

- UE 5.3;
- UE 5.4;
- UE 5.5;
- UE 5.6;
- later supported releases.

Each matrix entry must build and test its own artifact. A package compiled for one engine version must not be assumed binary-compatible with another.

The project must publish an explicit minimum supported Unreal version and keep the compatibility matrix in source control. Version-specific Unreal APIs should remain isolated behind adapters or compile-time guards.

## Fab Candidate Artifact Policy

The Fab candidate is a clean plugin archive, not a copy of the repository. Its exact contents remain subject to the current Fab requirements and should be reviewed before every submission.

Expected deliverables include:

- `GameProductionAIAssistant.uplugin`;
- `Binaries/` for the targeted Unreal version and platform;
- `Resources/Backend/` with the self-contained backend;
- `Config/` when required by the plugin;
- `Content/` only if the descriptor and product require it;
- minimal installation, usage, license, and support documentation;
- source files when required by the selected distribution model or Fab policy.

The archive must exclude at least:

- `Intermediate/`;
- `Saved/`;
- `.gpaa/`;
- logs, caches, local settings, secrets, and developer-only files;
- debug symbols or build products not intended for distribution.

Implement `scripts/verify-plugin-package.ps1` as a release gate. It should inspect the package directory or zip and fail when:

- a required file or directory is missing;
- `Resources/Backend/GPAA.Api.exe` is absent;
- the backend is framework-dependent when a self-contained release is expected;
- a forbidden path or extension is present;
- the archive contains an unexpected parent directory or unsafe path;
- the plugin descriptor is invalid or its version does not match the release tag.

Prefer an allowlist for top-level package paths, with narrowly defined exceptions, over an ever-growing denylist.

The verifier is implemented for both package directories and zip archives. Run it with:

```powershell
.\scripts\verify-plugin-package.ps1 `
  -PackagePath ".gpaa\PluginPackage" `
  -ExpectedVersion "v0.1.0"
```

It deliberately rejects the raw `BuildPlugin` output while that output still contains `Intermediate`, debug symbols, development configuration, or placeholders. The release packaging job must sanitize the output before invoking this gate.

## Automated Smoke Tests

`scripts/test-unreal-plugin-package.ps1` installs the candidate package into a fresh minimal content-only Unreal project and launches `UnrealEditor-Cmd.exe` unattended. This deliberately loads the precompiled C++ plugin without compiling a host project or rebuilding the package. The test verifies that:

- Unreal discovers and loads the plugin;
- the `GPAA.Plugin` automation tests pass;
- the packaged backend starts without a repository checkout or installed .NET runtime;
- the backend health endpoint becomes ready;
- the backend process stops after Unreal exits;
- Unreal automation and the editor log contain no release-blocking error.

```powershell
.\scripts\test-unreal-plugin-package.ps1 `
  -PackagePath ".gpaa\releases\GameProductionAIAssistant-v0.1.0-UE5.6-Win64.zip" `
  -UnrealEngineRoot "C:\Program Files\Epic Games\UE_5.6"
```

The test selects an available loopback port, observes the backend executable under the installed plugin with the expected editor parent ID, checks `/health`, and requires its exit within 15 seconds of the editor exit. It runs all `GPAA.Plugin` tests, including the opt-in packaged backend integration test. It does not require Ollama or make model requests. Unreal uses an in-memory derived-data cache to avoid a dependency on Zen or shared cache configuration. A five-minute timeout bounds the run; logs, JSON reports, and a success summary including the archive SHA256 remain under `.gpaa/smoke/<run-id>` on success or failure (the success summary is written only on success).

The Unreal CI job builds and verifies the complete archive, runs this smoke test against that archive, and uploads the archive only after success. Smoke evidence is retained on failure too. A compatible self-hosted runner must be configured before this job can execute on GitHub.

The fixture disables unrelated engine plugins by default. UE 5.6 can emit generic `LogAutomationTest: Error: Condition failed` messages while discovering its low-level tests, before the selected GPAA suite starts. The log gate excludes only that exact pre-suite diagnostic; all selected test failures, errors after the suite starts, fatal errors, and assertions still fail validation. Logs are preserved in full.

Unreal reports successful tests with warnings separately from successful tests without warnings. The gate counts both, while requiring every test state to be `Success` with zero errors. An initial health request may time out before the backend starts; this warning remains visible in the report and `testsWithWarnings` summary field.

Run the smoke test against the exact files that will be zipped. Rebuilding or copying a second package after the smoke test would invalidate the result.

## Release Gates

A release must not be published unless all of these gates pass:

| Gate | Required evidence |
| --- | --- |
| .NET build and tests | Release build succeeds and `GPAA.Tests` exits successfully |
| Unreal compilation | `BuildPlugin` succeeds for every advertised engine version |
| Package policy | Required files are present and forbidden files are absent |
| Installation smoke test | The packaged plugin loads in a clean minimal project |
| Backend lifecycle | The embedded backend starts, becomes healthy, and stops with Unreal |
| Version consistency | Git tag, plugin descriptor, and artifact name agree |

Release publication must depend on these jobs rather than duplicate them as best-effort steps.

## Runner Strategy

Unreal Engine is not a practical dependency to install from scratch on a standard GitHub-hosted runner for every build. The initial split should be:

- **GitHub-hosted Windows runner:** .NET restore, build, and tests;
- **self-hosted Windows runner:** Unreal build, packaging, and smoke tests with UE 5.6 already installed;
- **later custom cloud image:** optional path for a managed and scalable Unreal runner fleet.

The self-hosted runner should use explicit labels such as `self-hosted`, `windows`, `unreal`, and `ue-5.6`. Its Unreal installation path, test project path, output directory, and signing or publishing credentials must be supplied through controlled runner configuration and GitHub environments, not committed machine-specific values.

The Unreal job installs its SDK under the runner's temporary directory so the runner service does not need write access to `Program Files`. PowerShell execution policy is set only for the job's shell processes. A free drive letter from `Z:` through `T:` is temporarily mapped to the checkout with `subst` to keep UnrealBuildTool action paths below the Windows path limit; the mapping is removed after the job. The smoke test identifies its backend by its unique installation directory and editor parent ID, including when Windows reports a physical path instead of the mapped drive.

Self-hosted jobs must clean their dedicated working/output directories between runs and avoid executing untrusted fork code with release credentials.

## Implementation Order

- [x] Add `ci-dotnet.yml` on GitHub-hosted Windows.
- [x] Add UE 5.6 `BuildPlugin` validation on a self-hosted runner.
- [x] Add `verify-plugin-package.ps1` and automated tests for its rules.
- [x] Add minimal Unreal test project installation and smoke-test automation.
- [ ] Add tag-triggered packaging and GitHub Release publication.
- [ ] Produce and manually review the first Fab candidate.
- [ ] Add the Unreal version matrix and declare the minimum supported version.
- [ ] Revisit Fab requirements before submission and whenever Epic updates them.

## References

- [Epic Games: Plugins in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/plugins-in-unreal-engine?lang=en-US)
- [Fab Technical Requirements](https://www.fab.com/o/technical-requirements)

Fab requirements are external and can change. They are a release-time compliance input, not a rule set that should be copied once and assumed permanent.
