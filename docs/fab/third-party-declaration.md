# Third-party software declaration — copy-ready inventory

Product: Game Production AI Assistant 0.2.0. Publisher: Maxime ESPRIT. Windows x64 editor plugin.

| Component | Version / source | License | Distributed files / use |
|---|---|---|---|
| ONNX Runtime CPU | Microsoft / ONNX contributors, 1.20.1; https://github.com/microsoft/onnxruntime/releases/tag/v1.20.1 | MIT plus retained upstream third-party notices | C API header and gpaa_onnxruntime.dll under Source/ThirdParty/OnnxRuntime; embedded search inference |
| all-MiniLM-L6-v2 | sentence-transformers; revision 1110a243fdf4706b3f48f1d95db1a4f5529b4d41; https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2 | Apache-2.0 | ONNX model and vocabulary under Source/ThirdParty/MiniLM; model card and license included |
| SQLiteCore and ICU | Supported Unreal Engine installation | Supplied under the engine distribution terms/notices | Engine dependencies, not separately redistributed as downloads |
| Windows Credential Manager and desktop dialogs | Windows / Unreal platform APIs | System/engine functionality | No installer distributed; personal credentials and file selection |
| Optional cloud AI service | User-selected OpenAI Responses or compatible Chat Completions endpoint | User's provider service terms | No service entitlement, key or credits bundled; selected excerpts sent only for cloud chat after consent |

Own code retains the repository MIT license. Upstream DLL is renamed to gpaa_onnxruntime.dll solely to avoid library basename conflicts. No third-party project assets, sample animations, fonts or executables are added to the downloadable plugin. Gallery uses rasterized system-font text, an original vector-style icon and a capture of the author's test session; test project assets are not included.

The software was developed with AI assistance and contains a pretrained model for local embeddings, plus optional cloud AI functionality. Gallery graphics were rendered procedurally; no generative AI image output was used. Answer the live portal's disclosure questions according to their actual wording; this document is not an automatic declaration to Epic.

Dependency SHA256 values are in native-dependencies.lock.json (included in the local submission dossier). The source/binary archives retain the upstream license texts and notices. See native-third-party-notices.md for more detail.
