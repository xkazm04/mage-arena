# Security policy: mage-arena

Mage Arena VR is a solo competition entry (Meta VR Start Developer Competition 2026). It runs no server and ships no
release yet, so the realistic risks are a leaked key, a compromised CI dependency and an agent slipping past its guard.

## Reporting a vulnerability

Report privately through GitHub's private vulnerability reporting: the repository's **Security** tab, then
**Report a vulnerability**. The report goes to the repository owner, who is the only maintainer. Do not open a public
issue or pull request for a vulnerability.

- Acknowledgement within 7 days, and a triage decision (fix, accept or decline, with the reason) within 30 days.
- Supported version: `master` only. There are no released builds to patch.
- The owner must keep private vulnerability reporting enabled in the repository settings (Settings, Code security);
  without it this channel does not exist.

In scope:
- A secret committed to the tree or its history (the ElevenLabs key, the Ascent conformance token).
- The GitHub workflows and their dependencies (`.github/workflows/`, `.github/dependabot.yml`).
- A way around the worker guard (`tools/agy-guard.mjs`, `.agents/hooks.json`) that lets an AI worker change git
  history, bypass a hook or read a secret.
- Tampering with the pinned combat data that `apps/vr/tools/check-pin.mjs` does not catch.

## Automated guardrails we run

| Guardrail | Where | When it fails |
| --- | --- | --- |
| Read-only `GITHUB_TOKEN` (`permissions:` set in every workflow) | `.github/workflows/*.yml` | n/a: the token cannot write |
| No secret file tracked (`secrets.neverCommit`, filenames only) | `.ai/doctor.mjs`, run by `ai-conformance.yml` on every PR | the PR goes red |
| Pinned combat data unchanged (sha256) | `apps/vr/tools/check-pin.mjs`, run by `quality.yml` on every PR | the PR goes red |
| Worker guard (no git history change, hook bypass, recursive delete or secret read; reviews are read-only) | `tools/agy-guard.mjs` (PreToolUse hook), tested in `quality.yml` | the tool call is refused |
| Actions pinned to commit SHAs (`owner/repo@<40-hex sha> # vX.Y.Z`; checkout v4.2.2, setup-node v4.4.0, both upstream tag commits) | `tools/check-action-pins.mjs`, run by `quality.yml` on every PR | the PR goes red |
| Action refs kept current | `.github/dependabot.yml` (weekly, `github-actions`) | a Dependabot PR opens |
| Commit subjects follow the house forms | `tools/check-commits.mjs`, run by `commits.yml` on every PR | the PR goes red |

## Not yet in place, and why

- **Content secret scanning.** `neverCommit` matches file names, so a key pasted into a source file would pass it.
  Enable GitHub secret scanning with push protection when the repository goes public (free there, and it blocks the
  push itself, see `docs/MONOREPO.md`). A third-party scanner action would add one more dependency to pin; if content
  scanning is wanted before then, run the gitleaks release binary checked against its published sha256, not an action.
- **SAST.** CodeQL for C++ needs a compiled build, and the hosted runners have no Unreal Engine. clang-tidy runs on
  demand only (`apps/vr/tools/lint/tidy.ps1`; `-Strict` fails on any finding in a changed line, but nothing runs it
  automatically yet).
- **SBOM, signing, provenance.** Nothing is released and no container is built. The first build that leaves the
  owner's machine is the competition APK on the "Competition" release channel (`docs/PROJECT-PLAN.md`). That APK
  ships only once all of the following hold:
  - **Signed with a release upload key, not the debug key.** `apps/vr/tools/package-android.ps1` builds a Development
    APK with the debug keystore; the submission is a Shipping build. The keystore and its passwords live outside the
    repository and are read by name at build time, like the ElevenLabs key. `.ai/guardrails.yaml` `secrets.neverCommit`
    does not list `*.keystore` or `*.jks` yet; add both before the keystore exists.
  - **Signature checked before upload.** `apksigner verify --verbose --print-certs <apk>` must report APK Signature
    Scheme v2 or v3 and a signer certificate SHA-256 equal to the fingerprint recorded here when the key is made.
    Any other certificate means the wrong key, so the APK does not go up.
  - **Integrity and inventory recorded with the build.** Commit the APK's sha256 beside a CycloneDX SBOM listing what
    went in: the UE version (5.8.x), the Meta XR v207 archives with the SHA-256 values `apps/vr/tools/install-xr.ps1`
    pins (`PinnedSha256`), and the pinned combat-data commit (`apps/vr/data/PINNED.json`).
  - **No GitHub build provenance claimed.** `actions/attest` signs artifacts built on a GitHub runner, and hosted
    runners have no Unreal Engine, so the APK is built on the owner's machine. The sha256, the SBOM and the release
    signature are the record; do not describe the APK as SLSA-attested.
- **Required checks.** Branch protection is an owner setting. The checks to require on `master` are listed in the
  header of `.github/workflows/commits.yml`.
