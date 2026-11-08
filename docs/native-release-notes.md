# Game Production AI Assistant 0.2.0 — Windows beta

- Native local indexing, bundled CPU embeddings, semantic code/asset-metadata search and SQLite persistence.
- Automatic refresh at project startup, before searches/questions and every minute while idle.
- Open assets in Unreal and source references in the configured source editor.
- Optional streamed cloud chat using a personal provider key and explicit context-sharing consent.
- Local prompt and search history, contextual cancellation, native view selector and bottom-aligned responses.
- Personal-key actions beside the masked key field; native JSON profile import/export dialogs.
- HTTP cleanup before engine shutdown, covered by isolated pending-request shutdown tests.

Release targets: Windows x64 editor, UE 5.6 and UE 5.8 (validated with 5.6.1 and 5.8.4). macOS support is deferred. API usage is separate from the free plugin. See the user guide for setup, privacy and limitations.
