# Media provenance and reproduction

`render-fab-media.py` renders original GPAA branding and 1920x1080 gallery layouts with Pillow. Python and Pillow are development tools, not end-user prerequisites. No font file or demo scene is distributed.

01 and 03 are procedural feature layouts describing the 0.3 local/cloud choice. 02 embeds an offscreen render of the actual compiled Slate panel, captured by the opt-in `GPAA.Acceptance.ReleaseMedia.CurrentSlatePanel` automation fixture. Its seeded example prompt/answer is identified as an interface preview, not a fresh AI/retrieval evaluation. It contains no credential or private project source. The scene/animation assets named in the example are not part of the downloadable plugin.

To refresh the preview, build the plugin and run `scripts/test-native-plugin.ps1 -ReleaseMediaPath .gpaa/release-media/current-panel.png` with a supported Unreal engine/package. This renders to a texture without controlling a desktop window. The first GPU run can compile engine shaders; the development cache is under .gpaa/release-media/ddc. Copy the reviewed output to captures/prompt-v0.3.png, then run `python scripts/render-fab-media.py`. The fixture rejects empty renders.

Raw captures are retained for provenance but excluded from the upload dossier; only gallery images, branding masters and media-manifest.json are copied there. media-manifest.json records image dimensions and sizes. The proposed original icon is not an Epic/OpenAI logo. Generative image output is not used.
