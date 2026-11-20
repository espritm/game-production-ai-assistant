# Fab submission readiness — 0.3.0

Prepared 20 November 2026. This document describes the local dossier, not an Epic approval or submission.

## Product and scope

- Publisher: Maxime ESPRIT; contact@maximeesprit.fr; free Windows x64 Unreal Editor plugin.
- Targets: UE 5.6.1 and UE 5.8.4 only. No inferred 5.7/5.9, macOS, Linux, GPU-generation or packaged-game support.
- Local semantic search; optional local CPU GGUF generation; user-selected cloud generation; guided setup; session conversation; no separate History UI and no new search-history entries.
- No required companion process, Python service, .NET backend or Ollama installation.

## Dossier contents

Run `scripts/new-fab-submission-kit.ps1` after building the exact version for both target engines. It reads the plugin version by default and produces:

- Win64 binary and FabSource ZIPs for each selected engine.
- English user guide, release notes and native third-party inventory.
- Current listing copy, third-party declaration, MIT license and dependency lock.
- Three 1920x1080 gallery images plus product icon and provenance/size inventory.
- `SHA256SUMS.json`, including every copied payload file, and a transfer ZIP.

Source ZIPs contain one plugin directory and required source/dependencies/notices. Chat weights, credentials, project databases, personal logs and demo assets are excluded. Keep generated archives out of Git.

## Validation and evidence

The acceptance scripts test exact release archives, not just an IDE build. Required native checks cover storage and migration, code/asset retrieval, cloud transport fixtures, cancellation/shutdown, setup reset, conversation retention, minimum prompt height and Send alignment. Real local-model acceptance is an explicit additional check; its small fixture corpus is not a guarantee of semantic quality on arbitrary code.

The 0.2 owner-confirmed manual/second-machine checks are historical evidence only. The owner subsequently confirmed the 0.3 UI improvements in development. The release-preparation changes here are maintenance and packaging work; test results and exact final hashes are recorded in the generated evidence/manifests rather than invented in this document.

The gallery UI preview is rendered from compiled Slate widgets offscreen using a seeded example conversation. It is labeled as an interface demonstration, not an actual model evaluation. No private project code or credentials appear. See `fab/media/README.md`.

## Public source and CI

MIT is retained for original code. Third-party licenses and Unreal terms remain separate. The source repository remains private until an explicit visibility change; private GitHub URLs are not public customer download links.

The original runner has been unregistered from the source repository. The release design uses hosted public/source checks and a separate private build controller. Completing the Windows service transfer and recording the two limited tokens described in `releasing.md` are required before claiming the tag pipeline is operational. The private acceptance workflow supports a dry run without creating a GitHub tag or publishing a release.

## Portal actions still separate

1. Complete publisher/tax/payment onboarding if Epic still requires it.
2. Create/update the actual Fab draft; select only tested platform/engine versions and the appropriate live store-license option.
3. Upload the matching **FabSource** archives, not the entire transfer dossier. Use the included media, listing copy and dependency/AI disclosure inventory.
4. Add the real Fab URL/ID only after Epic assigns it; rebuild affected source archives and verify them again. No fake placeholder URL is shipped.
5. Preview and submit for Epic review. A GitHub release does not submit to Fab and a local test does not constitute Fab approval.

Applicable references: [Fab plugin file requirements](https://dev.epicgames.com/documentation/fab/asset-file-format-and-structure-requirements-in-fab), [publishing workflow](https://dev.epicgames.com/documentation/fab/publishing-assets-for-sale-or-free-download-in-fab), [licenses/pricing](https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab), and [Unreal EULA](https://www.unrealengine.com/eula/unreal). The publisher must complete the live portal's declarations according to their current wording.
