# Fab listing draft — native edition 0.2.0

Working copy for publisher review. This document has not been submitted to Fab. Store-specific fields and requirements must be checked again at submission time.

## Product name

Game Production AI Assistant

## Short description

Search your Unreal project locally and ask a cloud AI assistant questions grounded in your source files and asset metadata.

## Description

Game Production AI Assistant brings project search and source-aware chat into the Unreal Editor. Install the Windows plugin, enable it, and index your project. The embedding model ships with the plugin, so local search works without a companion application or a separate model installation.

Search source files and Asset Registry metadata, follow references back to your project, and refresh the index as files change. The plugin stores its index and conversation history in the project's Saved directory.

For generated answers, connect your own OpenAI API account or a compatible Chat Completions endpoint. Cloud generation requires a personal API key and explicit permission to send retrieved project excerpts. Provider charges are billed separately by your provider. Local search does not require a cloud API key.

Features:

- Bundled CPU embedding model and local semantic search.
- Incremental indexing with SQLite persistence.
- Source references for code and asset metadata.
- Streamed cloud responses, cancellation and local conversation history.
- Personal API keys stored with Windows Credential Manager.
- No .NET service, Ollama installation or companion application required by the plugin.

## Compatibility and limits

- Windows x64, Unreal Editor only. The plugin does not provide a packaged-game runtime assistant.
- Locally compiled and tested with UE 5.6.1 and UE 5.8.4. Choose the archive matching your engine version. Only advertise the versions actually included in the submitted product.
- Cloud chat requires internet access and a separate provider account. API-compatible providers can differ in supported fields and streaming behavior.
- The bundled embedding model is English-oriented. French search quality has not yet been validated on representative projects.
- Asset search uses metadata; it does not inspect every Blueprint graph or infer behavior from binary assets.
- Fully local answer generation is outside this edition's implemented scope.
- Current indexing bounds: 10,000 scanned files, 512 KiB per source file and 100,000 passages. Exact vector ranking is intended to be evaluated on representative project sizes before publishing scale claims.

## Data handling summary

Local indexing and search run inside Unreal. The index and history live under `<Project>/Saved/GPAA`. When cloud context sharing is enabled and the user asks for an answer, the question and selected source excerpts are sent to the configured provider. Users should review that provider's data handling terms. Profile exports exclude API keys.

## Publisher inputs still needed

- Final publisher identity, support contact and product/support URLs.
- Store-assigned product identifier and the corresponding plugin `FabURL`.
- Pricing and the final store license selection.
- Product icon, gallery images and a short demonstration captured from the final UI.
- Third-party software declarations reviewed against [the bundled inventory](native-third-party-notices.md), including the model and inference runtime.

## Demonstration capture plan

1. Enable the plugin in a fresh project and open the assistant panel.
2. Index a representative project; show completion and a local query with navigable source references.
3. Show provider configuration with a blank key field and explicit cloud consent.
4. Capture an optional cloud answer only after the account owner authorizes a billable request; show source references and streaming cancellation.
5. Show history and explain that local search works without a cloud account.

Never include a real API key, private project excerpts or account details in store media.

## Evidence and outstanding acceptance

See [native delivery status](native-delivery-a-status.md). Automated project-plugin tests pass on UE 5.6.1 and UE 5.8.4; UE 5.8.4 also passes the complete suite when installed as an engine plugin. Clean-machine acceptance, representative real-project retrieval quality, manual UI acceptance and an optional user-authorized live provider request remain outstanding. This draft makes no claim of Fab approval.
