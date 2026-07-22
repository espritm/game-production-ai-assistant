# Unreal Plugin Packaging CI/CD Roadmap

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

The workflow has been validated locally with Unreal Engine 5.6 on Windows. It is not yet a release pipeline: it does not create a versioned zip, validate package contents, run an installation smoke test, or publish a GitHub Release.

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

## Automated Smoke Tests

The release workflow must install the candidate package into a minimal C++ Unreal project and launch `UnrealEditor-Cmd.exe` unattended. The test must verify that:

- Unreal discovers and loads the plugin;
- the `GPAA.Plugin` automation tests pass;
- the packaged backend starts without a repository checkout or installed .NET runtime;
- the backend health endpoint becomes ready;
- the backend process stops after Unreal exits;
- Unreal and backend logs contain no release-blocking error.

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

Self-hosted jobs must clean their dedicated working/output directories between runs and avoid executing untrusted fork code with release credentials.

## Implementation Order

- [x] Add `ci-dotnet.yml` on GitHub-hosted Windows.
- [ ] Add UE 5.6 `BuildPlugin` validation on a self-hosted runner.
- [ ] Add `verify-plugin-package.ps1` and automated tests for its rules.
- [ ] Add minimal Unreal test project installation and smoke-test automation.
- [ ] Add tag-triggered packaging and GitHub Release publication.
- [ ] Produce and manually review the first Fab candidate.
- [ ] Add the Unreal version matrix and declare the minimum supported version.
- [ ] Revisit Fab requirements before submission and whenever Epic updates them.

## References

- [Epic Games: Plugins in Unreal Engine](https://dev.epicgames.com/documentation/unreal-engine/plugins-in-unreal-engine?lang=en-US)
- [Fab Technical Requirements](https://www.fab.com/o/technical-requirements)

Fab requirements are external and can change. They are a release-time compliance input, not a rule set that should be copied once and assumed permanent.
