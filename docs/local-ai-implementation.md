# Local AI delivery — 0.3.0 Windows beta

## Architecture

The existing IGPAAChatProvider interface now has a native FGPAALocalChat implementation. GPAAInference statically links a pinned CPU-only llama.cpp/ggml build. No executable, HTTP server, Python, Ollama or generic shared ggml DLL is added. AVX2 is checked before entering the backend. The initial backend is CPU-only; GPU acceleration remains future work.

FGPAANativeRuntime selects Local or the existing cloud provider explicitly. Local does not read a provider key, require cloud consent or fall back to the cloud. Retrieval/embedding remain on the existing native MiniLM pipeline. The question and selected excerpts are formatted using the model's chat template with a separate system message. Character-based retrieval trimming reserves answer space, followed by exact token-count validation. Questions are never silently truncated. Empty project evidence is handled deterministically without generating an invented path. A final check flags common generated project paths/source filenames absent from the supplied evidence, retaining the answer as unverified instead of declaring it complete. This is not a semantic correctness guarantee.

Model loading, prompt evaluation and decoding run on a worker with bounded thread/context budgets. The editor receives status and complete UTF-8 text pieces on the game thread. Cancellation stops delivery immediately and cooperatively aborts CPU evaluation/loading. Models/contexts are released after each request. Providers are retained centrally while tasks run to avoid callback-only ownership races. Engine pre-exit cancels and joins inference and model download work before module shutdown.

## Model management

The curated catalogue has Light Qwen2.5 Coder 1.5B Instruct Q4_K_M and an optional larger 7B variant. Explicit download uses native Windows HTTPS, range requests and a partial file in a user cache. Resume requires an exact Content-Range. A per-model named lock prevents simultaneous editor instances from appending to the same partial file or removing an active download. Each downloaded file must pass the pinned size and SHA256 before activation. Native network operations have 10-second timeouts; cancellation can wait for a blocked operation without blocking Slate. Curated weights are rechecked before generation. Custom GGUF selection performs header/runtime/template checks; compatibility and integrity of arbitrary models are not guaranteed.

Only fixed catalogue filenames in the managed cache can be removed. User-selected external models are not deleted. The package verifier rejects GGUF weights inside distributable archives. Only license texts/model cards are bundled. On Windows, the normal cache is %LOCALAPPDATA%/GPAA/Models (the exact path is displayed in Settings).

## Persistence and user interface

Settings add the Local provider, model catalogue, file picker, explicit Download/Cancel/Remove actions, CPU threads and context budget. Profiles carry portable preferences but exclude local filesystem model paths and secrets. History migrates transactionally to schema 4, preserving the index and old entries; new prompts record the actual provider and model. The answer's provider label is captured at request start so changing Settings cannot relabel a cloud response as local.

## Validation procedure

Standard tests require no model download, provider key or paid request:

    scripts/test-native-plugin.ps1 -UnrealEngineRoot <engine> -PackagePath <archive> -TestFilter GPAA

Opt-in real acceptance downloads about 1.04 GiB only when the test cache is missing, exercises cancellation/resume, checks integrity and generates code/asset/French answers. A no-evidence case verifies the deterministic guard. It also checks native retrieval/history, actual token-stream cancellation and late-request rejection after shutdown:

    scripts/test-native-plugin.ps1 -UnrealEngineRoot <engine> -PackagePath <archive> -LocalAIAcceptance

Reuse the verified cache without any download:

    scripts/test-native-plugin.ps1 -UnrealEngineRoot <engine> -PackagePath <archive> -LocalAIAcceptance -LocalOfflineOnly

The offline-only flag prevents the test from downloading a missing model. Generation code itself has no network calls; no OS network settings are changed by the tests. This is not a claim that the host network interface was disabled. CI always runs the no-model regression tests; the real-model test is a manually enabled workflow input with a runner cache.

## Limits and findings

The first real test exposed fabricated inventory paths with empty evidence; the deterministic guard was added to address that concrete failure. The initial French fixture also had an encoding issue, corrected before final validation. The evaluation remains small and synthetic: matching two source paths and a French variant does not prove broad production-code reasoning quality. Users must inspect references. The larger 7B model's performance is not covered by the Light benchmark.

Metrics log verification/load/first-token time, token counts, decode speed and process memory without prompts or secrets. Figures depend on CPU, other Unreal workloads, context size and filesystem cache. Headless fixture results are not performance guarantees for a heavy editor session.

The original 0.2.0 Fab dossier is kept as a separate immutable artifact. The 0.3.0 package needs manual user acceptance before replacing any intended store submission.

## Final validation — 11 November 2026

- UE 5.8.4: 29 standard tests pass on the final archive (`native-smoke/e20fd17ecb2a4133900d881f0c2d55a3`).
- UE 5.6.1: 29 standard tests pass (`native-smoke/9a8c2ea3527b4ed4886bfe87c2082ff9`).
- UE 5.8.4: fresh real-model acceptance passes, including interrupted/resumed download, pinned hash validation, code/asset/French path checks, no-source guard, native retrieval/history, token-stream cancellation and shutdown (`native-smoke/08c44544ada54660a2a5f1443ba0ea7c`).
- UE 5.6.1: real generation acceptance passes with the verified existing cache and offline-only test mode (`native-smoke/4d567377c2f44adea3f2dd4adae28ce4`).
- Separate HTTP pending-request shutdown acceptance passes (`native-smoke/979c7c71ffbf412a9d4dbb6bdd78ee98`).
- Packaging rejects bundled GGUF weights, modified inference libraries, executables and corrupted embedding dependencies.

Windows archive SHA256: UE 5.8 `6DDDC18A8E4F6FBEAC9F6692FF9DE347D730C6A40005D2499BC7DB885551765A`; UE 5.6 `E58BC5B1E4BC90668225C9CFEBC2C055D3C4D7EB341B52AC5762BF7B258E155F`.

Path checks do not evaluate every semantic claim. The French fixture identified the correct file but described stamina in health-related terms, illustrating the small model's limits. Native UI acceptance and an independent-machine check of 0.3.0 remain separate from the previous 0.2.0 acceptance. No desktop control or paid cloud call was used for this implementation's validation.

## Guided setup

Fresh settings leave ChatProvider empty. Explicit Local/Cloud radios choose the active provider; cloud selection authorizes context sharing. The old consent config key is retained only for compatibility and is hidden/ignored. Cloud profile import requires an explicit new Cloud selection. Quick Start configures Local and the pinned Light model, polls structured download success, and runs a real short local generation without project data or history writes. The panel owns cancellation and the game-thread controller; weak callbacks prevent access after it closes. `scripts/test-native-plugin.ps1 -QuickStartAcceptance` tests the controller with an already cached Light model, without downloading weights.
