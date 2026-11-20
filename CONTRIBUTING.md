# Contributing

The maintained product is the native Windows Unreal Editor plugin in `plugin/`. The C# projects are the retained reference implementation; changes there should not introduce a mandatory backend into the native plugin.

Open focused pull requests with a concrete behavior or maintenance goal. Avoid adding features during release hardening. Preserve provider privacy boundaries, cooperative cancellation and shutdown order. Do not commit generated dependencies, models, compiled engine/plugin binaries, credentials, project data or screenshots containing secrets.

Use `.clang-format` for touched C++ code and `.editorconfig` for other files. Prefer small widgets and explicit application services over large UI classes. Add abstractions at actual replacement/test boundaries; avoid architectural layers with no present responsibility.

Run the .NET CI commands in README when changing the reference backend. For native changes, build the package for the engines affected and run `scripts/test-native-plugin.ps1 -TestFilter GPAA` against the exact ZIP. Real-model and screenshot acceptance are explicit opt-in checks. See `docs/releasing.md` for tag validation and the private native build path.

Never put actual API keys into tests. Describe which checks ran and their limitations in the PR. External PRs receive hosted source/reference checks; native validation happens after maintainer review in an isolated trusted build process. See `SECURITY.md` for private security reports.
