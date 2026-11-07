# Native plugin — independent machine acceptance

This kit runs the exact precompiled Windows archive without the source repository, a compiler, dependency downloads or paid cloud requests. Unreal Engine itself must already be installed. Use the matching engine version, a writable extraction directory and a standard Windows user session.

## Prepare on the development machine

```powershell
.\scripts\new-clean-machine-kit.ps1 -PackagePath '.gpaa\releases\GameProductionAIAssistant-native-v0.2.0-UE5.8-Win64.zip'
```

Transfer the generated kit ZIP under `.gpaa/acceptance-kits` to a second Windows x64 machine or a known fresh VM. Keep the printed kit hash and compare it after transfer with `Get-FileHash`. Extract the kit before running it. Do not install .NET, Ollama or the source build toolchain for this test. Use an environment without personal provider keys.

## Run on the acceptance machine

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run-clean-machine-acceptance.ps1 -UnrealEngineRoot 'C:\Program Files\Epic Games\UE_5.8' -RequireCleanBaseline
```

The script checks the plugin archive hash, records tool availability, installs the plugin into a fresh disposable host project and runs its 14 standard Unreal tests. It uses the bundled embedding model and simulated loopback HTTP providers. Nothing is downloaded and no real provider is contacted by the tests. Successful tests also verify that installed plugin files remain unchanged.

`-RequireCleanBaseline` rejects a .NET or Ollama command on PATH; this is an additional check, not proof of a clean machine. The script deliberately never automatically marks `cleanMachineAttested` true. Record the actual OS image provenance and observations below. Python availability is informational; it is not invoked by the tests.

## Manual observations

- Record Windows version, Unreal version, fresh image or second-PC provenance, test date and tester.
- Open the generated `Host.uproject`, then open the assistant panel. Use the project path printed in the smoke evidence, or locate it under the extracted kit's `.gpaa/native-smoke` folder.
- With internet disconnected, refresh knowledge and search for the stamina fixture. Verify source references open and the panel remains responsive. Leave networking enabled for localhost; the automated HTTP fixtures require loopback access.
- Restart the editor; check that the index can be reused and the panel opens without requesting a companion installation.
- Confirm that no user API key was needed, no provider key was bundled, and no separate program installation was requested.
- Do not treat a successful automated run on a developer machine as independent-machine acceptance.

Return `acceptance-result.json`, the smoke `smoke-result.json`, Unreal log, JSON report and these recorded observations. These files may contain machine names and local paths; review them before sharing outside the team. No live cloud request is part of this kit.

The smoke host is a disposable test project. This test does not validate a production project, Blueprint graph analysis, broad retrieval quality or Fab approval.
