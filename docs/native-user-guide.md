# Game Production AI Assistant — User Guide

Version 0.3.0 (beta). Publisher: Maxime ESPRIT. Support: contact@maximeesprit.fr.

A free Windows x64 editor plugin for local project search and a choice of local or cloud AI chat. The search model is included; local chat models are downloaded separately on request. No companion application, separate model download, .NET service or Ollama installation is required. Cloud API usage is charged separately by your provider.

## Compatibility and installation

Validated engine versions: Unreal Engine 5.6.1 and 5.8.4 on Windows x64. Use the package matching your engine version. macOS, Linux and other engine versions are not advertised as supported in this release. This is an editor tool, not a packaged-game runtime feature.

From Fab / Epic Games Launcher: install the plugin for the supported engine version, open your project, enable **Game Production AI Assistant** under **Edit > Plugins**, and restart if requested.

Manual binary ZIP: close Unreal, extract the matching `Win64.zip` into `<Project>/Plugins/GameProductionAIAssistant/`. The `.uplugin`, `Binaries`, `Source`, `Resources` and other folders must be directly inside that directory. Avoid an extra nested plugin folder. Reopen the project and enable the plugin. The `FabSource.zip` is the source submission archive, not the precompiled installation ZIP.

Keep a single active copy: choose either a project or an engine installation for normal use. Update only while Unreal is closed, with a backup of your project. Opening a matching precompiled package should not require compiling the plugin yourself.

Open **Tools > Game Production AI Assistant**. The panel has **Prompt IA**, **Search**, **History** and **Settings**.

## Local search

Indexing starts automatically after Unreal finishes discovering assets, refreshes before each question or search, and runs every minute while idle. The first index can take longer. The panel does not need to remain open for periodic indexing.

Select **Search**, choose **Asset**, **Code**, or **Asset and Code**, enter a query, and click **Search**. Start with an English query such as `pistol reload animation` or a question about a system in your own C++ source. Search is local and does not require a key or internet connection.

**Open** on an asset result opens that object in its Unreal editor. **Open** on a source result uses the configured source editor. Install/configure your preferred editor separately if you want source navigation. For moved or removed assets, rerun the search after saving changes; indexing refreshes automatically.

The plugin searches source files and Asset Registry metadata. It does not inspect every Blueprint graph or infer the full behavior of binary assets. The included English-oriented embedding model can produce weaker results in other languages. Inspect the results; retrieval does not guarantee finding every relevant file.

## Local chat setup

1. In Settings, select **Chat Provider: Local**.
2. Under **Local AI**, choose a **Local Model Id**. Start with `qwen2.5-coder-1.5b-q4km` (Qwen2.5 Coder 1.5B Instruct Q4_K_M, approximately 1.04 GiB). The larger 7B option is approximately 4.36 GiB and needs substantially more RAM and CPU time; its performance is not validated by the light-model benchmark.
3. Read the displayed source, license and size, then click **Download Selected Model**. This explicitly downloads public model weights from Qwen on Hugging Face. Download progress appears in Settings. **Cancel Download** retains partial data; clicking Download again resumes it. A SHA256 check must pass before the model becomes available.
4. Alternatively, **Browse...** to an existing compatible instruction GGUF file. **Use Downloaded Model** clears this override. Arbitrary model architectures/templates are not guaranteed to work.
5. Return to **Prompt IA**, enter a project question and click **Send**. No cloud key or cloud-context consent is required in Local mode. After installation, generation works without a network connection. A local error never switches to a cloud provider automatically.

The initial native backend runs on **CPU with AVX2**, using four threads by default (configurable from one to eight). It does not require a helper application, Python, Ollama or a local HTTP server. GPU acceleration is not part of this first local release. Unreal also needs memory and CPU: start with Light, keep the default 4096-token context, and increase budgets only when resources allow it. Models are loaded on demand and released after each request, so first-token latency includes loading/context processing.

Statuses distinguish model verification/loading, context processing and generation. Cancellation is cooperative. Download network operations have bounded timeouts, so stopping a stalled download can take several seconds without blocking the editor. **Remove Downloaded Model** removes only the selected managed model/partial file; it never deletes an existing GGUF chosen outside the cache. Removal is disabled while local inference or a download is active.

Downloaded weights are stored in the user cache displayed in Settings (on Windows, normally `%LOCALAPPDATA%/GPAA/Models`), outside plugin files and project source. Curated models are checked against their pinned size/hash before use. User-selected files receive GGUF/runtime/template checks but have no known catalogue hash.

A final reference check flags common generated paths that were absent from the supplied context; the answer is retained as unverified/incomplete. Local answers can still be inaccurate. The small model is a starting point for source-grounded assistance, not a guarantee of cloud-model quality or exhaustive C++ understanding. Retrieved context is bounded to leave room for the question and answer; if it cannot fit, the request fails with an actionable message rather than truncating the question. When no project evidence is supplied, the local provider returns a deterministic no-evidence response instead of inventing an implementation path.

## Cloud chat setup

1. Open **Settings** and select a Chat Provider. `OpenAI` uses Responses; `OpenAICompatible` uses streaming Chat Completions.
2. Set the provider base URL and a model available to your account. For OpenAI, the base URL is `https://api.openai.com/v1`. The plugin appends the API route.
3. Paste your personal API key into the masked field and click **Save Personal Key** beside it. Entering a key alone does not save it. On success, the input clears and a confirmation appears.
4. Review and enable **Cloud Context Accepted** if you agree to send your question and selected project excerpts to that provider. Provider charges and data policies apply.
5. Return to **Prompt IA**, enter your question in **Prompt your AI here**, and click **Send**. Responses appear progressively. Inspect referenced files and assets to verify the answer.

**Cancel** appears beside **Send** or **Search** while the action is cancellable. An interrupted response can remain in history. Cancellation cannot undo data already sent or guarantee that a provider will not charge for work already performed.

Local search is independent of cloud chat. Select Local to generate answers on the PC instead. Compatible endpoints can differ in supported parameters and streaming behavior.

## History and settings profiles

**History** shows prompts (with provider/model identity for new entries) and completed searches, including search scope and references. Empty-result searches are recorded; automatic indexing is not. **Back** returns to the selected view. History persists between editor sessions.

**Export Settings Profile** opens a native save dialog for a JSON file. **Import Settings Profile** opens a native file picker. Cancel either dialog to leave settings and files unchanged. Profiles contain provider/model, local CPU/context budgets and source-opening preferences, exclude keys, history and machine-specific GGUF paths, and reset cloud consent when imported. Files over 1 MiB or malformed JSON are rejected. There are no editable path fields or legacy-history import button.

Source Opener **Auto** uses the available source-navigation integration. A custom command supports `{file}` and `{line}` placeholders. **Project Root Override** is an advanced option: leave it empty to index the current project.

## Privacy and storage

The local index and history are stored under `<Project>/Saved/GPAA/native-v1.sqlite`. This can contain project excerpts and chat content; include it in your own data-handling and backup decisions. The bundled embedding model and inference libraries are read from the plugin installation. Local chat weights are read from the separate user cache or selected GGUF path. Local mode does not transmit prompts or project excerpts; only explicit model downloads contact the public model host.

Personal keys are stored as Windows generic credentials for the current Windows user on this PC. The entry is `GPAA/Provider/<hash of normalized provider URL>`. Projects using the same provider URL under that Windows account share the same stored key. Keys are not serialized into Unreal configuration or exported profiles. The plugin reads the key into memory to authenticate requests. Other software with access to that Windows account may also access its credentials.

Use **Remove Personal Key** to remove the credential for the currently configured provider URL. Removing the plugin folder alone does not remove that Windows credential or the local index/history. Close Unreal before manually removing `<Project>/Saved/GPAA` if you choose to clear local data. Back it up first if you want to retain it.

Cloud requests go to your configured endpoint and contain your question and selected source excerpts. These requests apply to cloud mode only. The plugin supplies no account or API credits. Review your provider's privacy and retention terms before sending project content. Do not paste secrets into prompts. OpenAI requests set `store=false`; this does not replace applicable provider policies.

## Troubleshooting

- **A key is requested despite filling the field:** click **Save Personal Key** and check the confirmation. Check the base URL; credentials are scoped to that URL.
- **Authentication, quota or model error:** check the provider account's API access, billing and model availability. Never send a real key to support.
- **No expected result:** check the scope and project root, save source/assets, then rerun the search. Prefer an English query or exact name/path. Binary asset contents are not fully analyzed.
- **An asset does not open:** it may have moved or been removed. Search again and check its object path in the Content Browser.
- **Source navigation does not open:** check the editor installation and Source Opener setting. Asset navigation uses Unreal's editors.
- **Local model missing:** select a curated model and download it, or browse to an existing compatible GGUF. A `.partial` file cannot be used for generation.
- **Local context/output error:** shorten the question, lower Max Output Tokens, or increase Local Context Tokens within available RAM. Output limits can leave a partial answer, which remains in history as incomplete.
- **Local RAM/CPU error:** choose Light, close memory-heavy applications, or use a cloud provider explicitly. This native build requires AVX2.
- **Model integrity error:** remove the managed model/partial file and download again. Unknown user-selected GGUF files are outside the curated integrity guarantee.
- **No import/export window:** use an interactive editor session. Native dialogs are unavailable in unattended/headless sessions.
- **Slow first index:** allow model loading and indexing to finish. Current bounds are 10,000 documents, 100,000 passages and 512 KiB per source file. No large-project performance guarantee is advertised.
- **A different engine asks to rebuild:** use the package for a supported engine version. A successful build on one version does not validate another.

For support, email **contact@maximeesprit.fr** with the plugin version, exact Unreal version, Windows version and reproduction steps. Redact API keys, personal data and private source from logs/screenshots before sending them.

## Included software

llama.cpp b12284 (MIT), ONNX Runtime 1.20.1 (MIT) and a pinned all-MiniLM-L6-v2 model/vocabulary (Apache-2.0) are bundled. Full attribution and upstream notices are included in `Docs/native-third-party-notices.md` and `Source/ThirdParty`. The optional Qwen2.5 Coder instruction GGUF catalogue uses Apache-2.0 model licenses; its weights are not bundled. SQLiteCore and ICU come from Unreal Engine. No demo project or animation asset is included.
