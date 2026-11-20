# Releases and isolated Unreal builds

The source repository remains `espritm/game-production-ai-assistant`. The persistent Windows runner belongs only to the **private** `espritm/gpaa-unreal-builds` repository. Public PR jobs use GitHub-hosted machines. This separates runner access from public contributions; it does not turn the Windows PC into an ephemeral VM. Only trusted main commits may run there.

## One-time secrets

Create two separate **fine-grained** GitHub personal access tokens at https://github.com/settings/personal-access-tokens/new. Use selected repositories and an expiry date. Do not paste tokens into chat or commit them. Workflow/administration write permissions are not required.

1. **Dispatch token:** select only `gpaa-unreal-builds`; repository permission **Actions: Read and write** (Metadata read is automatic). In the source repository Settings > Secrets and variables > Actions, save it as **BUILD_DISPATCH_TOKEN**.
2. **Release token:** select only `game-production-ai-assistant`; repository permission **Contents: Read and write**. In the private build repository Settings > Secrets and variables > Actions, save it as **SOURCE_RELEASE_TOKEN**. This permits reading the private source during preparation and uploading release assets later.

The dispatch token cannot modify the private build workflow. The release token is not installed in public PR jobs. Checkout does not persist tokens in Git configuration. Rotate these tokens before expiry; a failed dispatch/build does not publish partial downloads.

## Create a release

1. Merge reviewed changes through `dev` into `main`. Set `VersionName` in the plugin descriptor to the intended version and increment its integer `Version` before merging. Update release notes and Fab documents/media. No new features belong in the release preparation step.
2. From an up-to-date checkout of `main`:

   ```powershell
   git checkout main
   git pull --ff-only origin main
   git tag -a v0.3.0 -m "Game Production AI Assistant 0.3.0"
   git push origin v0.3.0
   ```

   Use a new version/tag for each subsequent release; do not retarget a published tag.
3. The hosted release request job validates semantic version, exact commit, main ancestry and descriptor version. It dispatches the trusted workflow on the private build repository's `main`.
4. The private workflow independently checks the tag/commit/main relationship **before** assigning the runner. It builds UE 5.6 and UE 5.8, tests the exact archives, checks shutdown/background indexing/distribution rules, and prepares a Fab dossier.
5. Only after all checks pass does it create a draft, upload the two Win64 ZIPs, two FabSource ZIPs, the Fab dossier and `SHA256SUMS.json`, and publish the complete release. A published version is never overwritten by a rerun. Uploads use GitHub Release assets rather than Actions artifact storage.

Downloads are on the source repository's Releases page. They remain accessible only to authorized users while that repository is private. Opening the repository is a separate owner decision. This pipeline does **not** submit to or publish on Fab. Review applicable Unreal distribution terms before distributing editor binaries outside Fab.

## Operations

Build progress is in the private build repository's Actions page; the source job reports dispatch, not build completion. Failed builds publish no release. Failed uploads leave a draft for diagnosis/rerun. See local runner logs for detailed build failures; personal machine logs are not automatically published.

The runner's Network Service account is retained. Do not grant it interactive-user credentials. Before approving new main code, review workflow/script changes with the same care as executable code. Keep the private build repository private and restrict its writers. If public untrusted native builds become necessary, provision a separate disposable build machine rather than reconnecting this runner to public PRs.

For a maintainer acceptance run before the first real tag, manually dispatch the private workflow with the reviewed main SHA, the matching version tag name and `dry_run=true`. The workflow creates a tag only inside its disposable source checkout; it does not push a GitHub tag or publish downloads. The single private runner serializes native builds without dropping pending versions through a shared workflow concurrency group.
