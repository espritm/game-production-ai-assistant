# Third-party declaration — 0.3.0

Publisher: Maxime ESPRIT. Free Windows x64 editor plugin. This inventory supports the publisher's live declaration; it is not an automatic approval.

| Component | Version / origin | License | Distribution / use |
|---|---|---|---|
| ONNX Runtime CPU | Microsoft/ONNX, 1.20.1; github.com/microsoft/onnxruntime | MIT, retained upstream third-party notices | Header and renamed gpaa_onnxruntime.dll for embeddings |
| all-MiniLM-L6-v2 | sentence-transformers, revision 1110a243fdf4706b3f48f1d95db1a4f5529b4d41 | Apache-2.0 | Bundled ONNX search model, vocabulary, license and model card |
| llama.cpp / ggml CPU | ggml-org/llama.cpp b12284 | MIT | Pinned source-built static native inference libraries and notices; no server or separate executable |
| Qwen2.5-Coder 1.5B Instruct GGUF | Official Qwen Hugging Face repository, revision f86cb2c1fa58255f8052cc32aeede1b7482d4361 | Apache-2.0 | Optional user-requested Light download; weights not bundled, license/card retained |
| Qwen2.5-Coder 7B Instruct GGUF | Official Qwen Hugging Face repository, revision 13fb94bfda8c8cf22497dc57b78f391a9acb426a | Apache-2.0 | Optional user-requested Larger download; weights not bundled, license/card retained |
| SQLiteCore, ICU, Slate/editor/platform modules | Installed supported Unreal Engine | Engine-supplied licenses/notices | Build/runtime dependencies supplied through Unreal |
| Credential Manager, native dialogs and HTTPS | Windows / Unreal platform APIs | System/engine functionality | No separate installer; key storage, file selection and model transport |
| Optional cloud AI endpoint | User-selected OpenAI Responses or compatible Chat Completions | Provider service terms | No entitlement, key or credits supplied; selected context sent only in Cloud mode |

Original code retains MIT. Pinning and hashes are documented in scripts/native-dependencies.lock.json and native-third-party-notices.md. Retain full upstream notices in repackaged archives. Unknown user-selected GGUF files have their own licenses and compatibility requirements.

The product was developed with AI assistance and contains pretrained search weights plus optional downloaded chat models and cloud generation. Branding/gallery layouts are procedural, not generative image output. The UI preview uses the compiled Slate widgets and an explicitly seeded example conversation; it does not pretend to be a fresh model evaluation. The screenshot includes no credential or real private project source. No demo project or third-party scene/animation assets are distributed.

Answer the current portal's disclosure questions according to their exact wording. Applicable Unreal distribution restrictions are separate from the MIT license on original code.
