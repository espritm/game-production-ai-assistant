# Native Game Production AI Assistant

Publisher: Maxime ESPRIT. Support: contact@maximeesprit.fr. The plugin is free; cloud API usage is billed separately by your provider.

This editor-only Windows plugin indexes your project and searches it locally. Its native mode does not require .NET, Ollama, Python or a companion application. The small CPU embedding model is included in the package. Chat generation uses your own cloud provider account; local chat generation is a later delivery.

## First use

1. Install the package for your validated Unreal version and enable **Game Production AI Assistant**.
2. Open **Tools > Game Production AI Assistant**, then **Settings**.
3. Choose `OpenAI` for the Responses API, or `OpenAICompatible` for a provider supporting streaming Chat Completions.
4. Set the provider base URL and a model available to your account. The OpenAI base URL is `https://api.openai.com/v1`. Avoid appending `/responses` or `/chat/completions`; the plugin supplies the route.
5. Enter your personal API key and click **Save Personal Key** in the **Chat** category of Settings. The field is cleared and a confirmation appears after the key is stored in Windows Credential Manager. Each provider URL has a separate credential entry. Use **Remove Personal Key** to delete it. Entering a key in the field alone does not save it.
6. Review **Cloud Context Consent**: enabling it allows your questions and the selected source excerpts to be sent to the configured provider. That provider bills API usage; the plugin does not supply credits. Its retention policies still apply. The OpenAI adapter requests `store=false`.
7. Use **Search** or **Send**. Local indexing starts automatically after the project's assets are registered, refreshes every minute, and refreshes changed files before each search or question. Search and indexing work without a key or internet access.

There is no manual refresh button or Project Knowledge status box. Automatic indexing continues while the assistant panel is closed. A periodic refresh waits while another assistant operation is busy; a user search or question takes priority over a background refresh. Background indexing does not clear displayed results or send a cloud request.

Responses appear progressively. **Cancel** appears beside **Send** or **Search** only while that action is cancellable. It stops the request; partial chat responses are retained in history as interrupted. Search results include source references and the existing source-opening options. Unreal asset metadata is a virtual source, not deep Blueprint graph analysis.

**History** opens a separate view containing prompts and completed searches, including the selected search scope, result count and references. Empty-result searches are included. Automatic refreshes and internal retrieval for a prompt are not added as separate searches. Search history starts with this plugin update; earlier searches were not recorded. Existing conversations are retained by the history-schema migration. Use **Back** to return to the selected search/chat tab with its content preserved.

Click **Open** on an asset result to open that object in its Unreal asset editor. Code results use your configured external source editor. If an asset has been moved or deleted, refresh knowledge and search again; an unresolved reference is reported as a failure.

## Local data

The index and recent history are stored under `<YourProject>/Saved/GPAA/native-v1.sqlite`. Models and the inference runtime are read from the installed plugin; the plugin never writes index data into its installation directory. The SQLite database uses transactions and versioned embedding identities. A changed embedding pipeline rebuilds vectors instead of mixing incompatible indexes.

The baseline embedding model is the English `sentence-transformers/all-MiniLM-L6-v2`, pinned to the revision described in `native-third-party-notices.md`. English code search is the initial validation target; French and other languages require a larger quality evaluation before equivalent quality can be claimed. It is not a model that generates chat responses.

Current bounded indexing limits are 10000 source documents, 100000 fragments and 512 KiB per source file. Generated directories, the assistant plugin itself and third-party source folders are excluded. Asset metadata capture currently inherits the existing Asset Registry bounds. Large-project quality and performance require additional validation beyond the bundled prototype corpus.

## Migration from the .NET edition

The native index is separate from the previous JSON index. It is built automatically when the project opens or before the first search/question. Do not copy embeddings between different models.

Use **Export Settings Profile** to select a destination JSON file in the native save dialog. Use **Import Settings Profile** to select an existing JSON profile. Cancelling either dialog leaves settings and files unchanged. Import resets cloud consent so you can review the selected provider before enabling it again. Profiles exclude API keys and do not import keys or history. The legacy history import is no longer exposed in Settings.

The original backend projects remain in the development repository as a reference. The native plugin has no legacy backend client, supervisor, executable or runtime toggle.

## Validation and compatibility

Build and test the native package using:

```powershell
.\scripts\package-native-plugin.ps1 -UnrealEngineRoot 'C:\Program Files\Epic Games\UE_5.6'
.\scripts\test-native-plugin.ps1 -PackagePath '.gpaa\releases\GameProductionAIAssistant-native-v0.2.0-UE5.6-Win64.zip'
```

Dependency download is a build-time activity only, pinned by revision and SHA256 in `scripts/native-dependencies.lock.json`. It is not performed on the end user's machine.

The native smoke tests cover offline embeddings, stream fragmentation, SQLite rollback and local indexing/retrieval. They use no real cloud credentials. Live provider authentication, account quota and model availability must be checked with the user's account. A successful test on one Unreal version does not validate another. Consult the delivery status document for current evidence and outstanding Fab requirements.
