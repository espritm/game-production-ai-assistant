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
