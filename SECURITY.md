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
| Action refs kept current | `.github/dependabot.yml` (weekly, `github-actions`) | a Dependabot PR opens |
| Commit subjects follow the house forms | `tools/check-commits.mjs`, run by `commits.yml` on every PR | the PR goes red |

## Not yet in place, and why

- **Actions pinned to commit SHAs.** Six refs still float on `@v4` (`actions/checkout` x4, `actions/setup-node` x2, in
  `quality.yml` and `ai-conformance.yml`). The gate is written but not wired: `node tools/check-action-pins.mjs` fails
  until every ref is `owner/repo@<40-hex sha> # vX.Y.Z`. To close it, resolve each tag
  (`git ls-remote --tags https://github.com/actions/checkout 'v4.2.2*'`, using the peeled `^{}` sha for an annotated
  tag), rewrite the refs, and run the pin check in `quality.yml` in the same commit. `commits.yml` uses no action at all.
- **Content secret scanning.** `neverCommit` matches file names, so a key pasted into a source file would pass it.
  Enable GitHub secret scanning with push protection when the repository goes public (free there, and it blocks the
  push itself, see `docs/MONOREPO.md`). A third-party scanner action would add one more dependency to pin.
- **SAST.** CodeQL for C++ needs a compiled build, and the hosted runners have no Unreal Engine. clang-tidy runs on
  demand only (`apps/vr/tools/lint/tidy.ps1`).
- **SBOM, signing, provenance, image scanning.** Nothing is released and no container is built. Revisit with the first
  Quest build that leaves the owner's machine.
- **Required checks.** Branch protection is an owner setting. The checks to require on `master` are listed in the
  header of `.github/workflows/commits.yml`.
