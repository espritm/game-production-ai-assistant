# Fab listing — copy-ready Windows release 0.2.0

Prepared 8 November 2026. Local submission material; no listing has been created or submitted by this task.

## Product fields

- Name: Game Production AI Assistant
- Publisher: Maxime ESPRIT
- Product type: Tools & Plugins / Unreal Engine code plugin (choose the matching live portal category)
- Price: Free
- Support: contact@maximeesprit.fr
- Platform: Windows x64, Editor only
- Engine versions to select: 5.6 and 5.8 (validated with 5.6.1 and 5.8.4)
- Suggested tags: AI, Search, Editor, Productivity, C++, Assets, Assistant
- Store license: select in the publisher portal; free pricing does not select a license. The repository's MIT license is retained in all packages.

## Short description

Search your Unreal project locally and ask a cloud AI assistant questions grounded in your source files and asset metadata.

## Description — paste into the listing

Find relevant project context without leaving Unreal Editor.

Game Production AI Assistant adds local semantic search and optional source-aware cloud chat to your editor. Install the Windows plugin and enable it: the CPU search model ships with the plugin, with no companion application or separate model installation.

Search C++ source and Asset Registry metadata, filter results by Asset, Code, or both, then open the referenced asset in Unreal or navigate to the source in your configured editor. Indexing refreshes automatically when the project opens, before each search/question, and every minute while idle.

For generated answers, connect your own OpenAI API account or a compatible streaming Chat Completions endpoint. Cloud chat uses your personal API key and requires explicit consent to send your question and selected project excerpts. Answers stream into the panel, with cancellation and local history for both prompts and searches.

Features:
- Local semantic search with a bundled CPU embedding model.
- Incremental indexing and SQLite persistence inside your project Saved folder.
- Code and asset-metadata scopes, with navigable references.
- Optional cloud chat with streamed responses and contextual cancellation.
- Personal keys stored in Windows Credential Manager.
- Local prompt/search history and portable JSON settings profiles that exclude keys.
- No .NET service, Ollama installation or companion application required.

The plugin is free. Cloud API charges, model access and account requirements are separate and determined by your provider. Local search works without a cloud account or internet connection.

## Technical information — paste into Included Files

- Modules: GPAACore (Editor), GPAAInference (Editor), GPAAssistant (Editor).
- Development/target platform: Windows x64. Editor utility only; no packaged-game runtime feature.
- Supported engine release targets: UE 5.6 and UE 5.8; validation used 5.6.1 and 5.8.4. Other versions, including 5.7, are not claimed. macOS support is deferred.
- Installation: Fab / Epic Games Launcher engine installation, or the matching precompiled manual package. Do not keep duplicate active project/engine copies.
- Documentation: bundled English Docs/native-user-guide.txt and Docs/native-user-guide.md.
- Dependencies bundled: ONNX Runtime 1.20.1 CPU (MIT); all-MiniLM-L6-v2 model and WordPiece vocabulary at pinned revision (Apache-2.0), with notices.
- Engine dependencies: SQLiteCore and ICU supplied by Unreal Engine.
- External services: optional user-selected OpenAI Responses or compatible Chat Completions endpoint. Personal account, key and internet connection required for cloud chat.
- Network: local search/indexing runs on the PC; cloud chat sends the question and retrieved excerpts to the configured provider after consent.
- Local data: <Project>/Saved/GPAA/native-v1.sqlite for index/history; Windows Credential Manager for provider keys.
- Index bounds: 10,000 documents, 100,000 passages, 512 KiB per source file. No advertised large-project throughput guarantee.
- Model: English-oriented embeddings; equivalent retrieval quality in other languages is not claimed.
- Asset analysis: Asset Registry metadata, not exhaustive Blueprint graph inspection or binary-asset behavior analysis.
- Generated answers can be incomplete or incorrect. Verify the cited sources.
- Fully local chat generation: not included.
- Example project/content: none bundled. Screenshots show a separate test project; its animation assets and scene are not included.
- Support: contact@maximeesprit.fr.

## FAQ — copy-ready

**Do I need an API key?** Only for cloud answers. Local project search does not need a key.

**Does the plugin include API credits?** No. The plugin is free; your provider bills cloud API usage separately.

**Does it run a local chat model?** No. The included model creates local search embeddings. Answer generation uses your configured cloud provider.

**Where is my key stored?** In Windows Credential Manager for your Windows account and provider URL. It is excluded from Unreal configuration and profile exports.

**Is my whole project uploaded?** Local indexing stays on your PC. A cloud question sends the question and selected retrieved excerpts to your chosen provider after you enable consent.

**Does it understand every Blueprint?** It searches asset metadata and source excerpts. It does not inspect every Blueprint graph or guarantee complete project understanding.

**Does it work on Mac?** This release supports Windows x64 only.

## Media upload order

1. fab/media/01-cover.png — cover/thumbnail candidate, 1920 x 1080.
2. fab/media/02-in-editor.png — actual current plugin capture, 1920 x 1080.
3. fab/media/03-local-and-cloud.png — data-flow feature graphic, 1920 x 1080.

All three files are PNG, each below 3 MB and together below 25 MB. Original procedural branding is included as SVG and PNG. The real capture shows an existing response; no additional billable request was made for the media. No API key appears in these files. The screenshots are illustrative; project assets are not included. No promotional video is required by the summarized Tools & Plugins section used for this preparation, and none is supplied.

## Fields completed when creating the actual listing

- Fab-assigned product ID and matching FabURL (do not invent one).
- Store license selection in the live form. Do not replace existing MIT/third-party notices.
- Any required public download/documentation URL. The GitHub repository is private; do not use its URLs as public customer links or make it public without owner authorization. The dossier supplies local source ZIPs and bundled documentation for upload/hosting.
- Actual third-party software and AI disclosure fields, using fab/third-party-declaration.md. AI-assisted development and the included pretrained model must be described accurately if the portal asks; no generative image model was used for these media.
- Preview/owner review before submission. Submission belongs to step 5, outside this preparation task.
