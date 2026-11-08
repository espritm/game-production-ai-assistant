# Fab submission preparation — 8 November 2026

Step 4: local dossier preparation. This record is not Fab approval and no upload/submission/publication has been performed.

## Acceptance recorded

The owner explicitly confirmed that manual UI acceptance, independent Windows-machine installation, and representative real C++ project retrieval (points 1, 2 and 3 of the release checklist) are validated on 8 November 2026. These are owner-attested results. Detailed second-machine/project logs and hardware identifiers were not supplied in this conversation; no such details are invented here. Earlier records describing those gates as pending are superseded by this confirmation.

The owner separately validated real animation search/opening, cloud answers grounded in an asset path, and persistent history during development. macOS support is explicitly deferred.

## Local release deliverables

- Version 0.2.0 (beta), free Windows x64 Unreal Editor plugin.
- UE 5.6 and UE 5.8 binary installation ZIPs and source submission ZIPs. Each source ZIP has one plugin parent directory, source modules, Config/Content/Resources, bundled dependencies and licenses; no build outputs, user keys, index or history files.
- Bundled English user guide as Markdown and plain text, release notes and third-party notices.
- Copy-ready listing description, technical information, FAQ, support contact and explicit limits: fab-listing-draft.md.
- Copy-ready dependency/AI disclosure inventory: fab/third-party-declaration.md.
- Original Icon128.png in the plugin and SVG/512 px branding masters in fab/media.
- Three checked 1920 x 1080 PNG gallery images, each below 3 MB and together below 25 MB. The second embeds a genuine current plugin capture. Raw screenshots are not placed in the upload dossier.
- Reproducible local dossier builder: scripts/new-fab-submission-kit.ps1. Its output includes the four archives, documentation, listing copy, media and SHA256SUMS.json.

## Compatibility and dependency checks

Only Windows x64 and UE 5.6 / 5.8 are advertised. Validation uses installed UE 5.6.1 / 5.8.4. No inferred support for 5.7, macOS, Linux or packaged-game runtime is advertised. Fab rebuilds source plugins against selected versions; its build/review remains separate from our checks.

ONNX Runtime 1.20.1 CPU is supplied as a DLL with the MIT license and upstream transitive notices. The MiniLM model, vocabulary, model card and Apache-2.0 license are pinned by hash. SQLiteCore and ICU are engine-provided. Runtime dependency inspection previously found Windows/UCRT and Visual C++ libraries, with no direct CLR/.NET import. The independent-machine acceptance is now owner-confirmed.

Gallery graphics use an original procedural icon and rasterized system-font text; no font files, engine logos or demo assets are added to the downloadable plugin. No generative image model was used for the media. The software was developed with AI assistance and includes pretrained local embeddings and optional cloud AI functionality; answer the actual portal disclosure questions accurately.

## Fields completed in step 5 (actual listing creation)

| Field | Prepared value / action |
|---|---|
| Publisher / support | Maxime ESPRIT / contact@maximeesprit.fr |
| Price | Free; provider API usage billed separately |
| License | Owner selects the store license in the live form; retain repository MIT and upstream notices |
| Product ID / FabURL | Use the actual ID assigned by Fab, then rebuild the source archive with that URL; do not invent one |
| Documentation/download hosting | Bundled local files ready. Repository is private: its URLs are not public customer/download links; choose owner-approved hosting if requested |
| Media | Three ready PNG files, proposed cover/icon ready for owner preview |
| Third-party/AI declarations | Copy inventory into the actual portal fields according to their scope |
| Review / publication | No portal action performed. Preview before submission; publication is a separate step |

## Official requirements consulted

Checked on 8 November 2026:

- [Fab file format and structure guide](https://dev.epicgames.com/documentation/en-us/fab/asset-file-format-and-structure-requirements-in-fab): one plugin per ZIP, source plugin layout, source builds against selected versions, media limits and thorough documentation for tools/plugins.
- [Publishing workflow](https://dev.epicgames.com/documentation/en-us/fab/publishing-assets-for-sale-or-free-download-in-fab): draft creation, files/media, preview, review submission and activation are distinct steps.
- [Full technical requirements](https://www.fab.com/o/technical-requirements): public page indicates last update 21 September 2026. Expandable clauses were not fully exposed to the text reader. This dossier does not claim a full clause-by-clause Epic compliance audit; review the live portal and current rules at submission.

The public summary specifies gallery images at least 1920 x 1080, JPEG/PNG, under 3 MB each and under 25 MB total. Source archives are not password-protected. No rule specific to UEFN source projects has been applied to this Unreal Editor code plugin without checking scope.

## Exact local validation evidence

Final packaged archives pass 24 standard tests on UE 5.8.4 (native-smoke/5cfcd947ca464170bc64451c51f9d5d4) and 24 on UE 5.6.1 (native-smoke/af024479bcb549779763596cb8c729dd). Package policy checks and negative cases (unexpected executable and corrupted dependency) pass. No live provider request was used for these tests. UE 5.6 compilation includes existing engine-header deprecation warnings; the build succeeds.

| Archive | SHA256 |
|---|---|
| GameProductionAIAssistant-native-v0.2.0-UE5.6-FabSource.zip | 830768707F3B977A9832C8BEBFF065B3BB5FC7BD74E8E66590BBD9C83126514B |
| GameProductionAIAssistant-native-v0.2.0-UE5.6-Win64.zip | 847A12DDCF94E96D65359AB3FE2FCEC60E49230615EDC8A66E788D9D670A7361 |
| GameProductionAIAssistant-native-v0.2.0-UE5.8-FabSource.zip | BEEA6F0F76441CD73EAFD765F2A62B536312CB0D2D340359BE4062D2BB68F8E8 |
| GameProductionAIAssistant-native-v0.2.0-UE5.8-Win64.zip | A9D791C0D5A195241FFAC1F68C3C9E552327F2C38CEE4B488EFF2E42C1EF243A |
