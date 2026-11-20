# Fab listing — Windows release 0.3.0

Prepared 20 November 2026. This is upload material, not a submitted or approved listing.

## Product fields

- Name: Game Production AI Assistant
- Publisher: Maxime ESPRIT
- Price: Free
- Support: contact@maximeesprit.fr
- Product: Unreal Engine editor code plugin / Tools & Plugins
- Platform: Windows x64
- Engine versions: 5.6 and 5.8 only (validation targets 5.6.1 and 5.8.4)
- Suggested tags: AI, Search, Editor, Productivity, C++, Assets, Assistant
- Store license: select the appropriate live Fab option; MIT and third-party notices remain in the code packages.

## Short description

Search Unreal code and asset metadata locally, then ask a local or cloud AI for answers grounded in project excerpts.

## Description

Find project context without leaving Unreal Editor.

Game Production AI Assistant combines local semantic search with a choice of native local AI generation or your own cloud provider. Install and enable the Windows plugin; no companion application, Python backend or Ollama installation is required.

Search code and Asset Registry metadata by Asset, Code, or both. Open asset results in Unreal and source results in your configured editor. The project index refreshes automatically on startup, before questions/searches, and every minute while idle. Search does not require an API key, and search results are not recorded in history.

For local answers, Quick Start selects the Light Qwen model, downloads and verifies it, then runs a local test prompt. The first Light download is about 1.04 GiB; compatible existing GGUF files can also be selected. The native CPU backend requires AVX2 and additional available memory alongside Unreal. GPU acceleration is not included.

For cloud answers, select Cloud AI Provider and configure your personal OpenAI or compatible Chat Completions account. Selecting Cloud authorizes sending your question and relevant project excerpts to that provider. Any API charges are billed by the provider you configure and select, according to its pricing. Cloud processing does not guarantee project confidentiality: review the provider's data-handling terms. Local generation does not send prompts or excerpts to an AI provider.

Answers stream into a conversation with user messages on the right. Enter sends, Shift+Enter inserts a new line, and cancellation retains the partial response. The current panel thread is session-based; there is no separate History view. Settings profiles exclude API keys and machine-specific model paths.

Answer quality depends directly on the selected model and available project context. Light favors a small download; a more capable model is recommended for stronger results if hardware/provider budget permits. Leading cloud models can outperform lightweight local models; advanced local installations can use more capable models. Always verify references and generated code.

## Technical information

- Modules: GPAACore, GPAAInference, GPAAssistant; Editor only, Windows x64.
- Bundled: ONNX Runtime 1.20.1 CPU (MIT), pinned MiniLM embeddings/vocabulary (Apache-2.0), native llama.cpp b12284 / ggml static CPU runtime (MIT), upstream notices.
- Optional downloads: Qwen2.5-Coder 1.5B Light and 7B Larger instruction GGUF Q4_K_M models, pinned revisions and SHA256; Apache-2.0. Chat weights are not bundled. Larger 7B throughput is not guaranteed by Light tests.
- Engine-provided: SQLiteCore, ICU and editor/platform modules, governed by engine distribution terms.
- Keys: Windows Credential Manager, scoped to the configured provider URL; excluded from project config/profile exports.
- Local data: <Project>/Saved/GPAA/native-v1.sqlite for index and retained prompt records; no new search history. Local models use a separate user cache or selected file path.
- Index bounds: 10,000 documents, 100,000 passages, 512 KiB per source file.
- Retrieval model is English-oriented. Equivalent search quality across languages or every project size is not claimed.
- Assets: registry metadata, not exhaustive graph/behavior analysis.
- No demo scene, animation assets, API credits, cloud entitlement or model service subscription bundled.
- macOS, Linux, GPU generation, packaged-game runtime and untested engine versions are not advertised.
- English Markdown and plain-text user guides are bundled.

## FAQ

**Do I need a key?** Only for cloud generation. Search and local generation do not need one.

**Is the model included?** Search embeddings are included. Local chat weights are downloaded only on request; Quick Start handles the Light download.

**Is the whole project uploaded?** Local mode uploads no prompts/project excerpts. Cloud questions send selected context to the configured provider after the Cloud choice.

**Can answers be wrong?** Yes. This is an assistant, not a correctness guarantee. Check the cited files/assets.

**Are search results saved?** New searches are not historized. The conversation view retains its turns for the current panel session.

## Media and submission

Use 01-cover.png, 02-in-editor.png and 03-local-and-cloud.png. See media/README.md for provenance and the distinction between an interface example and actual model acceptance.

Upload the FabSource archive for each selected engine version; Win64 ZIPs are manual installation downloads. Complete the live third-party and AI disclosures using fab/third-party-declaration.md. After a real Fab listing ID is assigned, add the real URL to the descriptor; no placeholder URL is shipped.
