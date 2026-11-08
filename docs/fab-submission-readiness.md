# Fab submission preparation — 7 November 2026

This is a release preparation record, not an approval or a submitted listing.

## Technical evidence available

- Three native editor modules; local search has standalone functionality without a cloud account.
- UE 5.6.1 and UE 5.8.4 builds and automated tests; a separate UE 5.8.4 engine-plugin installation check.
- Local real-asset evaluation on a copy of the owner's project; English-oriented retrieval with weaker French results.
- Separate Windows binary ZIP and source ZIP, fixed dependency hashes and bundled third-party notices.
- The source ZIP has one plugin parent directory. Standard Config, Content, Resources and Source directories are preserved, including empty directories. The plugin has no Unreal content assets and keeps `CanContainContent=false`.
- Independent-machine acceptance kit: archive hash verification and a portable standard native test run. The kit itself has passed after extraction on the developer machine; actual independent-machine acceptance remains pending.

The developer session cannot manage local Hyper-V machines (Windows authorization denied). No fresh VM or second machine has been tested. This is an operating-system access limitation, not a test success.

## Runtime dependency inspection

Inspection of the shipped ONNX DLL with Microsoft's `dumpbin /DEPENDENTS` shows Windows API/UCRT libraries and Visual C++ runtime libraries (`MSVCP140`, `MSVCP140_1`, `VCRUNTIME140`, `VCRUNTIME140_1`), with no direct CLR/.NET import. UE 5.8 includes corresponding Visual C++ libraries in its AppLocalDependencies tree. This static observation does not replace runtime validation on a fresh Unreal installation. No Microsoft runtime installer is added to the plugin.

## Store preparation still requiring owner input or acceptance

| Item | State / next action |
|---|---|
| Publisher identity and support contact | Confirmed: Maxime ESPRIT, contact@maximeesprit.fr; descriptor author updated |
| Pricing and store license selection | Confirmed free; final store license selection remains pending |
| Product identifier and FabURL | Fill from the actual listing once assigned |
| Product copy | [Draft available](fab-listing-draft.md); review before submission |
| Third-party software | [Inventory available](native-third-party-notices.md); complete the actual publisher declaration/review |
| Gallery images | Capture the actual final UI; no screenshots have been fabricated |
| Icon / product visual identity | Owner-approved asset still needed |
| Live provider request | Optional, not run; needs an account owner and authorization for a billable request |
| Independent-machine acceptance | Run the [portable kit procedure](clean-machine-acceptance.md) on a known fresh environment |
| C++ production-project relevance | Project supplied has no game C++ directory; asset-only evidence is available |
| Publication | No upload, submission, pricing change or publication performed |

## Official documentation checked

The [Fab file format and structure guide](https://dev.epicgames.com/documentation/en-us/fab/asset-file-format-and-structure-requirements-in-fab) describes one project/plugin per ZIP, the code-plugin directory layout and builds for selected engine versions. Its media section specifies gallery images at least 1920×1080, JPEG/PNG, under 3 MB each and under 25 MB total; gallery videos use 1920×1080, MP4/MOV/WEBM, at most 300 MB. Recheck these at upload time.

The [full technical requirements](https://www.fab.com/o/technical-requirements) currently show an update date of 21 September 2026. The public page's expandable details were not exposed by the text reader in this session, so this record does not claim a complete clause-by-clause audit. Do not apply sections specifically written for UEFN source projects to this Unreal editor code plugin without checking their scope.
