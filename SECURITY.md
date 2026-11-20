# Security

Do not put API keys, project excerpts or personal logs in public issues. For a suspected security vulnerability, contact **contact@maximeesprit.fr** privately with the affected version and a minimal, redacted reproduction. The same address handles support; prefix security reports with `Security`.

The current Windows native plugin stores personal provider keys in Windows Credential Manager, not project settings or exported profiles. Local inference does not send prompts/project excerpts to a provider; explicit model downloads contact the configured public model host. Cloud mode deliberately sends selected context to the user's provider. Review that provider's terms before using private project content.

Indexes and prompt records can contain project material. Their local SQLite database and backups must be handled accordingly. User-selected GGUF files are outside the curated catalogue's integrity guarantees. AI output is untrusted: verify source references and code before using it.

Public pull requests must never run on the maintainer's persistent PC. Unreal release builds are controlled from a separate private repository and accept only reviewed commits from the main branch history. This is repository-level separation, not a sandbox for arbitrary third-party code.
