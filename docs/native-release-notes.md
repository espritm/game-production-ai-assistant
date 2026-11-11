# Game Production AI Assistant 0.3.0 — Local AI beta

- Native CPU local chat provider, with no companion application or API key requirement.
- Curated instruction GGUF catalogue with explicit downloads, resume/cancel, pinned SHA256 verification, managed removal and existing-file selection.
- Source-grounded local streaming, token/context budgets, cooperative cancellation and shutdown cleanup.
- Deterministic no-evidence response when no project sources are supplied; no automatic cloud fallback.
- Provider/model identity in new history entries; transactional schema-3-to-4 migration preserves prior history and the index.
- Portable local CPU/context preferences; machine-specific model paths are excluded from exports.
- Existing cloud providers remain available. Local GPU acceleration and macOS are deferred.

The 0.2.0 Fab dossier/archives remain separate. See the user guide for local setup, hardware limits and cache location.

# Game Production AI Assistant 0.2.0 — Windows beta

- Native local indexing, bundled CPU embeddings, semantic code/asset-metadata search and SQLite persistence.
- Automatic refresh at project startup, before searches/questions and every minute while idle.
- Open assets in Unreal and source references in the configured source editor.
- Optional streamed cloud chat using a personal provider key and explicit context-sharing consent.
- Local prompt and search history, contextual cancellation, native view selector and bottom-aligned responses.
- Personal-key actions beside the masked key field; native JSON profile import/export dialogs.
- HTTP cleanup before engine shutdown, covered by isolated pending-request shutdown tests.

Release targets: Windows x64 editor, UE 5.6 and UE 5.8 (validated with 5.6.1 and 5.8.4). macOS support is deferred. API usage is separate from the free plugin. See the user guide for setup, privacy and limitations.
