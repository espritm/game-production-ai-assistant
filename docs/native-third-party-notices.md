# Native dependency inventory

This inventory is an input to the Fab third-party declaration. It is not a substitute for the publisher completing Epic's declaration and review.

## ONNX Runtime

- Publisher: Microsoft and ONNX Runtime contributors.
- Version: 1.20.1, Windows x64 CPU build.
- Source: https://github.com/microsoft/onnxruntime/releases/tag/v1.20.1
- License: MIT; the unmodified upstream `LICENSE` and `ThirdPartyNotices.txt` are included under `Source/ThirdParty/OnnxRuntime`.
- The upstream `onnxruntime.dll` is named `gpaa_onnxruntime.dll` in this plugin to avoid a basename collision with runtimes loaded by other Unreal plugins. It is dynamically loaded through the C API; no runtime executable or installer is distributed.
- Transitive third-party notices must remain included when repackaging.

## MiniLM embeddings model and WordPiece vocabulary

- Repository: https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2
- Pinned revision: `1110a243fdf4706b3f48f1d95db1a4f5529b4d41`.
- Model file: `onnx/model.onnx`; vocabulary: `vocab.txt`.
- The model card declares Apache-2.0. Its original model card and the Apache-2.0 license text are distributed under `Source/ThirdParty/MiniLM`.
- A tokenizer is implemented in this project using the supplied vocabulary: uncased WordPiece, NFD accent stripping, basic punctuation tokenization, CLS/SEP, at most 256 tokens. Encoding uses attention-mask mean pooling and L2 normalization to 384 dimensions.
- This English sentence model is a retrieval baseline, not a guarantee of production code-search quality across languages and project sizes.

## Engine-provided dependencies

SQLiteCore and ICU are provided by the supported Unreal Engine installation. The plugin uses their engine interfaces and libraries. They are not downloaded as companion programs by the plugin. Their notices and availability are governed by the engine distribution.

## Cloud services

OpenAI Responses and optional compatible Chat Completions endpoints are external services selected by the user. The plugin supplies no provider key, model entitlement or API credits. Applicable service terms, charges and data-handling policies are those of the selected provider.

## Reproducibility

`scripts/native-dependencies.lock.json` records archive/model/vocabulary/license/model-card SHA256 values. `prepare-native-dependencies.ps1` downloads from the upstream sources and rejects mismatched bytes. `-RecordHashes` is only for an explicitly reviewed dependency update; normal builds must use the existing lock file.

## Native local chat runtime (0.3.0)

- Project: ggml-org/llama.cpp, pinned tag `b12284`.
- Source: https://github.com/ggml-org/llama.cpp/tree/b12284
- License: MIT, retained at `Source/ThirdParty/Llama/LICENSE`.
- The source archive is pinned in native-dependencies.lock.json. CPU-only static libraries (llama, ggml, ggml-base, ggml-cpu) are built with MSVC and linked into GPAAInference. No standalone llama executable, shared ggml DLL, OpenMP library or HTTP server is distributed.
- Build configuration disables host-native optimization and uses an explicitly checked AVX2 CPU baseline. Compiler/build flags and generated library hashes are recorded in build-manifest.json. CMake and Visual Studio are build-machine tools, not end-user prerequisites.

## Optional local chat models (0.3.0)

Weights are downloaded only after an explicit action and are not included in the plugin ZIP. The catalogue is defined in GPAALocalModels.cpp with pinned revisions, sizes and SHA256 values:

- Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF, revision `f86cb2c1fa58255f8052cc32aeede1b7482d4361`, `qwen2.5-coder-1.5b-instruct-q4_k_m.gguf` (1,117,320,768 bytes); SHA256 `cc324af070c2ecbfd324a30884d2f951a7ff756aba85cb811a6ec436933bb046`.
- Qwen/Qwen2.5-Coder-7B-Instruct-GGUF, revision `13fb94bfda8c8cf22497dc57b78f391a9acb426a`, `qwen2.5-coder-7b-instruct-q4_k_m.gguf` (4,683,073,536 bytes); SHA256 `509287f78cb4d4cf6b3843734733b914b2c158e43e22a7f4bf5e963800894d3c`.

Official sources: https://huggingface.co/Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF and https://huggingface.co/Qwen/Qwen2.5-Coder-7B-Instruct-GGUF. Both declare Apache-2.0. Their pinned license texts and model cards are included under `Source/ThirdParty/LocalModels`. User-selected custom GGUF files have their own compatibility and licensing requirements.
