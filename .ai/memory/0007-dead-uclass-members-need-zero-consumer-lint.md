---
id: 0007
kind: gotcha
scope: path:apps/vr/Game/Source
date: 2026-10-05
supersedes: null
refs: [0008]
---

The compiler will not report a dead private member of a UCLASS. Clang's `-Wunused-private-field` (and clang-tidy's
`clang-diagnostic-unused-private-field`) only fires when every member of the class is defined in one translation unit,
and UHT defines part of each UCLASS in its `.gen.cpp`; MSVC has no equivalent at all. `UHandPresentationComponent::Wards`
survived until the arena-greybox sweep found it by a zero-consumer grep (commit de92c3c). That grep is now the `lint`
capability, `node apps/vr/tools/lint/unused-members.mjs`, run on every pull request; its `--self-test` puts `Wards` back
into the real component and must flag it. The tidy claim is reasoned from clang's documented rule, not yet observed:
running `apps/vr/tools/lint/tidy.ps1` on `HandPresentationComponent.cpp` at `de92c3c^` would confirm it.
