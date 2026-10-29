# Image-based 2.5D route for Mage Arena VR: research report

Date: 2026-10-28 (written for the 2026-10-29 owner ruling, DECISIONS.md "research and spike a 2.5D image-based route").
Scope: read-only web research plus the ai-registry, with arithmetic for the stereo and memory questions. No engine or
ComfyUI run backs any claim here. **[unverified]** marks claims I could not confirm from a primary source.
**[calc]** marks my own arithmetic, with its inputs stated so it can be checked.

Fixed inputs used throughout: Quest 3 panel 2064x2208 per eye, Meta spec about 25 PPD
([meta.com compare](https://www.meta.com/gb/quest/compare/)). The default eye buffer is 1680x1760 at render scale 1.0
([Meta render scale doc](https://developers.meta.com/horizon/essentials/render-scale);
[UploadVR, 2023-09-27](https://uploadvr.com/quest-3-higher-default-rendering-resolution)), so about 20 PPD effective at
the centre **[calc: 25 x 1680/2064]**. App memory limit is 5.75 GiB PSS
([Meta memory doc](https://developers.meta.com/horizon/documentation/unreal/po-memory-ram/)). IPD is taken as 63 mm, the
opponent stands 16 m away, and head translation is at most ±0.15 m (a seated lean).

---

## (a) Executive summary

**Verdict: GO for a hybrid.** Make the static world and the distant opponent image-based. Keep everything within arm's
reach in 3D. The seated, fixed-seat layout suits image-based rendering unusually well:

- Most of the scene is either planar (the sand and the slate floor) or 20 m or more away (the wall and the stands).
- At those distances a ±0.15 m head lean produces at most about 2 px of disocclusion **[calc, §1]**.
- At 16 m a flat card is stereoscopically indistinguishable from a 3D body: 0.14-0.25 px of relative disparity across
  the body's depth **[calc, §3]**.

The owner already ruled the arena down to "one 2D background" (DECISIONS 2026-10-28 late). This report says how to do it
properly, and how far the same idea extends.

### Ranked recommendation (do in this order)

| # | Asset class | Route | Why | Risk |
|---|---|---|---|---|
| 1 | **Arena, stands, sky, sand and slate floor** | **Image-based.** A cubemap painting generated from the exact seat eye, conditioned on engine depth and edges, and sampled by direction from the seat on simple proxy meshes (unlit, 1-3 draws). Separate layer cards only for objects nearer than ~10 m. | Planar or distant surfaces are geometrically exact under projection. 2048² per cube face (≈22 PPD at face centre) matches Flux 2's ~4 MP ceiling, so one face is one generation. Memory is trivial (~11-15 MB with ASTC 6x6). | **Low-medium.** Style consistency across faces; painted silhouettes not matching proxy edges. |
| 2 | **Opponent (Brennic) at 16 m** | **Image-based.** An animated sprite card, repainted by a video model from renders of the existing Brennic rig (pose, depth and normals from the seat camera), then matted and packed into a 4096² atlas. Keep the 3D Brennic as the fallback and as the source of the driving data. | At 16 m he is ~165 px tall per eye. Only silhouette, value design and animation read there, and a painting in the target style delivers those directly. The flat card is stereo-correct at that distance. | **Medium-high.** Identity drift, line "boil" or flicker, and matting of the cloak edge. Must be spiked. |
| 3 | **Spell VFX (fire, water, sigil, ward)** | **Hybrid.** Generated video flipbooks supply the *texture motion* (fire licks, water churn). The gameplay-shaped geometry (ward arc ribbon, sigil trail, telegraph rings) stays procedural 3D, textured with those flipbooks. Additive on dark wherever possible. | Chalk on dark slate is inherently additive-friendly. Shape must follow gameplay, which a baked video cannot do. | **Medium.** Overdraw when the player's own spell fills the view; loop seams. |
| 4 | **Player hands** | **Stay 3D.** Paint the albedo by multi-view projection (Flux 2 with depth control and S30 hand crops as reference). Do the shading live (stepped N·L plus tonal-art-map hatching, rim, spell glow, wrist fade). | At 0.3-0.6 m, 1 cm of depth is about 5 px of disparity **[calc]**. Only geometry works this close. | **Medium.** Baked lighting looks wrong as the hand turns, so keep the albedo light-neutral. |
| 5 | **Crowd, banners, life in the backdrop** | Painted into the cubemap, plus a few animated sprite strips (cheering segments) and banner flipbooks. | Stops the "dead diorama" read for a handful of draws. | Low. |
| 6 | **Creatures that circle the player** (hound, maw) | Keep 3D for now, or use 8-direction sprites later. | Their view angle changes far more than Brennic's (±14°). | Out of scope for the first spike. |

### Go/no-go risks (in order of how likely they are to kill the route)

1. **Sprite animation quality.** Video-model identity drift and flicker, and matte edge chatter. This is the biggest
   unknown, and the Brennic spike settles it (experiment E3).
2. **Painting-to-proxy registration.** Depth control is approximate. If a painted edge does not sit on a geometric
   discontinuity, the paint "slides" in stereo. Mitigation: the painting defines the silhouettes and the engine defines
   each layer's depth (§1).
3. **Cross-face style consistency** in the panorama (seams, stroke direction changes at cube edges). Experiment E2.
4. **Licences.**
   - **MiniMax H3's licence lists the US, UK, EU and South Korea as "excluded territories"**: users there must apply to
     MiniMax for a licence ([The Batch, 2026-08-14](https://www.deeplearning.ai/the-batch/minimaxs-state-of-the-art-video-model-is-only-minimally-open)).
     If the studio is in the EU, do not ship H3 outputs until that application is granted.
   - FLUX.2 [dev] outputs are usable commercially per its model card, but running the model commercially needs a BFL
     licence; verify the output clause ([HF model card](https://huggingface.co/black-forest-labs/FLUX.2-dev);
     [licence](https://bfl.ai/legal/non-commercial-license-terms)).
   - The LTX-2.x Community Licence is free below $10M annual revenue ([ltx.io licence](https://ltx.io/model/license)).
   - Any paid 3D or world service (Marble, Meshy, Rodin) needs the owner's written approval (CLAUDE.md).
5. **Schedule.** The contest deadline is 2026-11-18. Pipelines 1 and 3 fit inside it. Pipeline 2 fits only if E3 passes
   within about 3 days.
6. **No headset test in this phase** (DECISIONS 2026-10-28). Stereo claims can only be checked by computed stereo-pair
   captures until a headset or Link session is allowed (E4).

**What not to do:** Gaussian splats on Quest (Meta advises ≤150k splats; UE support on Quest is third-party only; it buys
nothing for a fixed seat). Waiting for 3D generators to reach "2020s fidelity" (the gap the owner sees is texture,
shading and animation, not polygon count).

---

## (b) Findings per question

### 1. Camera-projected, depth-matched paintings in real time and VR

**Prior art.**

- **Google Seurat (2017-2018)** is the closest shipped precedent for mobile VR. It renders a complex scene from a
  bounded "headbox" and bakes it into simplified textured geometry that holds up for any eye position inside the box.
  It was used on *Blade Runner: Revelations* (Daydream / Mirage Solo); Google reported one scene cut from 46.6 M to
  ~307k triangles ([Google OSS blog, 2018-05](https://opensource.googleblog.com/2018/05/open-sourcing-seurat.html);
  paper "View-Region Optimized Image-Based Scene Simplification", ACM TOG 2018,
  [pdf](https://cdn.pharr.org/seurat.pdf)). This is old (2018), but the principle is exactly ours: a small viewing
  volume makes image-based rendering valid.
- **Google DeepView / Immersive Light Field Video (SIGGRAPH 2020)** uses ~16 layered RGBA+depth spherical-shell meshes to
  hold disocclusions inside an ~80 cm viewing volume. It quotes 10 PPD, which is low for Quest 3 today
  ([SIGGRAPH history entry](https://history.siggraph.org/learning/immersive-light-field-video-with-a-layered-mesh-representation-by-broxton-flynn-overbeck-erickson-hedman-et-al)).
  Lesson: a few depth layers are enough for a seated volume.
- **Blockade Labs Skybox AI** exports a 360 panorama as a depth-displaced GLB mesh "for visual parallax". It is
  experimental, and its depth quality varies by style
  ([Blockade support](https://support.blockadelabs.com/hc/en-us/articles/31649281677074-Export-3D-Models-from-Skybox-AI)).
  This is the "single depth-displaced sphere" variant: it shows rubber-sheet stretching at depth jumps.
- **Research on texturing scenes from panoramas.**
  - RoomTex (ECCV 2024) unwraps the scene to a panoramic depth map, generates a panorama as the global style reference,
    then paints the occluded objects view by view ([arXiv 2406.02461](https://arxiv.org/pdf/2406.02461)).
  - DreamSpace propagates a panorama texture into occluded areas by inpainting
    ([arXiv 2310.13119](https://arxiv.org/pdf/2310.13119)).
  - RoomPainter reports fewer seams than per-view inpainting ([arXiv 2412.16778](https://arxiv.org/html/2412.16778v2)).

  These are the academic version of our pipeline.
- **Stylised Quest game: *Across the Valley*** ([Meta dev blog, 2024-02-28](https://developers.meta.com/horizon/blog/across-the-valley-graphics-design-challenges/)).
  It uses hand-painted textures, flat toon shading and animated sketch shadows. Computed outlines doubled the cost (a
  second pass per object), so the team painted outlines into the textures for Quest. Lesson: bake line work into the
  painting and avoid hull passes.
- No shipped VR game using AI-generated camera-projected matte paintings was found. **[gap]**

**How well it holds in stereo with small head motion [calc].**

- With projection onto proxy geometry, each eye sees the painting on surfaces at their true depth. Stereo is therefore
  exactly right wherever the proxy matches the painted form.
- The error is the proxy mismatch. A painted detail that really sits Δ metres off the proxy at distance d shows an
  angular error of about IPD·Δ/d² in stereo, and t·Δ/d² under a head lean t.
  - Stands relief of 1 m at 30 m: 0.063·1/900 = 7e-5 rad ≈ 0.004° ≈ 0.1 px. A painted cylinder is enough for the stands.
  - Footprints and scuffs on the sand: Δ ≈ 0, exact.
- **Disocclusion** (background revealed behind a nearer edge by a lean t) is θ ≈ t·(1/d_near − 1/d_far):
  - Wall top at 20 m in front of stands at 25 m, t = 0.15 m: 0.0015 rad ≈ 0.09° ≈ **2 px**. Dilate the far layer
    by 4 px; no inpaint needed.
  - A brazier at 5 m in front of the wall at 20 m: 0.15·(0.2 − 0.05) = 0.0225 rad ≈ 1.3° ≈ **32 px**. This needs its own
    card plus an inpainted background.
  - **Rule:** anything nearer than ~10 m that overlaps a farther painted surface becomes its own layer.
- **Yaw ±100°** causes no disocclusion by itself; the neck pivot adds ~8-10 cm of eye translation, already covered by t.
  It does set the coverage: ±100° yaw plus about half of the ~110° horizontal FOV ⇒ ±155°. The wrap seam can sit at
  180° behind the player, where it is never seen.
- Dynamic and occluding things (opponent, spells, rings, hands) must **never be baked** into the painting.

**Resolution needed for VR [calc].**

- The eye buffer gives about 20 PPD at render scale 1.0 and about 25 at the panel's native size. The texture should
  supply at least 20-25 texels per degree where the gaze dwells.
- Full coverage of 310° x ~115° (from 75° down to 40° up) at 22 PPD is ≈ 6800 x 2500 = 17 MP. A cubemap with 2048²
  faces gives 22.7 PPD at face centre and up to about 2x at face edges (cube faces oversample toward their corners).
- Memory: 6 faces x 4.2 MP = 25 MP. ASTC 6x6 (3.56 bpp) is ≈ 11 MB, about 15 MB with mips. ASTC 4x4 (8 bpp) is about
  33 MB with mips. **Memory is not the constraint; generator resolution and consistency are.**
- An unlit, 1-3 draw backdrop frees GPU time. Test render scale 1.2-1.3 (≈ panel-native), which is where a painting
  starts to look crisp. **[unverified on device]**

**Projection mechanics in UE (recommended).**

- **Sample by direction:** `normalize(AbsoluteWorldPosition − SeatEye)` from a TextureCube, with SeatEye in a Material
  Parameter Collection. Do not compute equirect UVs with atan2.
  - The hardware handles the cube mapping and Vulkan filters cube seams seamlessly.
  - It avoids the mobile half-precision trap: an 11-bit mantissa cannot address an 8k-wide equirect without visible
    stepping. If you do compute UVs, set the material to full precision.
- No camera-projection node exists in UE for this; a forum search found nothing native
  ([OTOY forum thread](https://render.otoy.com/forum/viewtopic.php?p=415259)). The MPC plus TextureCube material is a few
  nodes.
- Baking per-vertex UVs works too, but only on a dense mesh (~1° vertex spacing), because the projection is non-linear
  across large triangles.

**Seams to watch.**
- Cube-face boundaries: generate with overlap, then feather.
- Mip and page boundaries if you use 2D pages: use 4-8 px duplicated borders.
- Proxy depth jumps without a matching painted edge: these "tear" under head motion. The painted silhouette must
  coincide with the proxy silhouette.

**Keeping lighting coherent with dynamic elements.**
- Make the painting unlit, with its key light chosen once (direction and colour written into every prompt).
- Paint the dynamic sprites with the same key (same reference and prompt block).
- Spell light on the painted sand is a fake: a sum of 2-4 radial glow terms in the backdrop material (MPC positions and
  colours), or additive ground glow cards under each projectile.
- Shadows of dynamic objects: multiply-blend cards (the sprite's own silhouette sheared onto the floor reads as a chalk
  smudge).
- Global mood changes: exposure and tint parameters on the backdrop material, not real lights.

**Generation strategies compared.**

| Strategy | Stereo correctness | Effort | Notes |
|---|---|---|---|
| (a) Depth/edge-conditioned generation per cube face onto hand-built proxies | Exact on planes and far shells | Medium | **Recommended.** Proxies come from the greybox we already have. |
| (b) img2img of a lit engine render | Same as (a) | Low | Keeps layout tightly, but imports the engine's mid-90s shading unless denoise is high (≥0.75), at which point it is (a). Useful for the floor, where layout must match gameplay rings. |
| (c) Generate free, then estimate depth (Depth Anything / MoGe) and displace a sphere | Approximate; rubber sheets at jumps | Lowest | Blockade-style fallback; fine beyond ~20 m. |
| (d) Image-to-world service (World Labs Marble: splats PLY/SPZ plus GLB mesh export) | Good | Paid (needs approval) | Could produce proxies from S30 directly ([Marble mesh export docs](https://docs.worldlabs.ai/marble/export/mesh)); experiment only. |

### 2. Flux 2 dev conditioning in ComfyUI

**Model facts.**
- FLUX.2 was released 2025-11-25; [klein] followed 2026-01-15.
- [dev] has 32B parameters and outputs up to ~4 MP. BFL's spec page lists 8-10 multi-reference images for [dev] and 6 for
  [klein]; other pages say 10 ([BFL help, specs](https://help.bfl.ai/articles/6944273991-what-are-the-flux-2-technical-specifications)).
- It supports hex colour codes in prompts and structured (JSON) prompts
  ([BFL docs](https://docs.eu.bfl.ai/flux_2/flux2_overview)).
- ComfyUI files: `flux2_dev_fp8mixed.safetensors`, `mistral_3_small_flux2_bf16.safetensors` (text encoder) and
  `flux2-vae.safetensors` ([Comfy docs: Flux 2 dev](https://docs.comfy.org/tutorials/flux/flux-2-dev)).
- On a 64 GB unified-memory box, the 32B fp8 model (~32 GB) plus a bf16 24B text encoder (~48 GB) cannot both stay
  resident. Use an fp8 text encoder if one is available, or let ComfyUI unload the encoder after conditioning.
  **[unverified: exact file names and timings on this machine]**

**Multi-reference (style) wiring.**
- Each reference goes LoadImage → VAEEncode → **ReferenceLatent**, chained onto the conditioning (the native pattern,
  per the Comfy template and the
  [RunComfy write-up](https://www.runcomfy.com/comfyui-workflows/flux-2-dev-in-comfyui-high-fidelity-visual-generation)).
- Native ReferenceLatent has no per-image strength. The community
  [Multi Reference Latent](https://comfy.icu/extension/lynclee__comfyui_multi_referencelatent) and
  [ReferenceLatentPlus](https://github.com/shootthesound/comfyui-ReferenceLatentPlus) packs add strength control.
- Each reference costs context; downscale references to ~1 MP.
- New native nodes `Flux2Scheduler` and `EmptyFlux2LatentImage` exist **[names unverified; check in ComfyUI 0.42]**.
- **Pitfall:** Flux references carry content as well as style. S30 contains labels ("ARENA", "CROWD x100+"), a specific
  layout, Brennic and hands.
  - Feed text-free crops (a stands crop, a sand crop, a slate-border crop).
  - Say "image 1 is a style reference only: chalk and pastel on dark slate".
  - Put a negative block on lettering.

**Depth, normal and structure control.**
- **alibaba-pai/FLUX.2-dev-Fun-Controlnet-Union** covers pose, canny, HED, depth and gray. Use the newer
  `FLUX.2-dev-Fun-Controlnet-Union-2602.safetensors` (better data, CFG-distilled)
  ([model card mirror](https://s9gkls5--c7029cb47f3111f1aaa91607ee4eb77e.web.val.run/alibaba-pai/FLUX.2-dev-Fun-Controlnet-Union)).
- In ComfyUI it runs through **JLC Flux2 ControlNet v1.0.0** (MIT, 2026-07-19,
  [repo](https://github.com/Damkohler/JLC-Flux2-ControlNet),
  [comfyui-wiki](https://comfyui-wiki.com/en/news/2026-07-19-jlc-flux2-controlnet-v1-0-0)). Its nodes:
  - `JLC Flux2 ControlNet Loader`
  - `JLC Flux2 ControlNet Apply` / `Apply Advanced`
  - `JLC Flux2 ControlNet Orchestrator` (1-4 branches)
  - `JLC Flux2 Reference Image Orchestrator` (≤10 refs)
  - `JLC Flux2 ControlNet Latents Cache`
  - `JLC Flux2 In/Out-Paint Adapter` (experimental)
- The pack has no preprocessors. Use comfyui_controlnet_aux (Depth Anything V2, DWPose, lineart), or better, feed the
  engine's own depth and edge renders.
- No normal-map control exists. Feed normals as a "gray" or HED-like hint, or use depth plus canny of the greybox.
  **[unverified effectiveness]**
- Alternative pack: [comfyui-flux2fun-controlnet](https://comfy.icu/extension/bryanmcguire__comfyui-flux2fun-controlnet).

**Panoramas, tiling and outpainting.**
- No seamless-360 LoRA for FLUX.2 [dev] was found.
- There is one for **[klein] 9B**: [nomadoor 360 ERP outpaint LoRA](https://huggingface.co/nomadoor/flux-2-klein-9B-360-erp-outpaint-lora),
  2:1 and edge-matched, with a ComfyUI "Panorama Stickers" UI. Its output is only ~2048x1024 equirect (≈5.7 PPD), so use
  it for **layout and blocking**, then refine per cube face with [dev] at 2048².
- [Spherope](https://github.com/orhir/spherope) lists a FLUX.2-dev ERP pipeline at 1024x2048 (~40 GB VRAM, ~16 GB with
  offload).
- For consistency across faces: generate the front face first. Render each neighbouring face at 100-110° FOV so it
  overlaps the finished one, reproject the finished overlap into it as known pixels, and complete with the JLC
  In/Out-Paint adapter under the same depth control and references.

**Chalk/pastel LoRA.**
- No public chalk-on-slate LoRA for FLUX.2 was found. **[gap]**
- Train one with ostris **ai-toolkit**, which supports FLUX.2 [dev] and [klein]
  ([RunComfy AI-Toolkit FLUX.2 guide](https://www.runcomfy.com/trainer/ai-toolkit/flux-2-dev-lora-training);
  [fal blog](https://blog.fal.ai/training-flux-2-loras)).
  - Dataset: 20-40 curated images (the S-series studies, S30 crops, studio stills), captioned with a trigger word.
- FLUX.1, FLUX.2 and klein LoRAs are not interchangeable
  ([RunComfy](https://www.runcomfy.com/comfyui-workflows/flux2-dev-ai-toolkit-lora-inference-in-comfyui-training-matched-results)).
- Training a 32B [dev] LoRA in 64 GB unified memory is plausible with quantisation but slow **[unverified]**. A [klein] 9B
  LoRA is the cheap first try.

### 3. 2D sprite or impostor characters in stereo VR

**Disparity arithmetic [calc].**
- The absolute disparity of a point at d relative to infinity is IPD/d. At 16 m that is 3.94 mrad = 0.226° ≈ 5.6 px at
  25 PPD. The card sits at the right depth, so this is correct.
- The flatness error is the relative disparity across the body's depth Δ, about IPD·Δ/d²:
  - Δ = 0.4 m (torso): 9.8e-5 rad = 0.0056° = 20 arcsec ≈ **0.14 px**.
  - Δ = 0.7 m (casting arm thrust forward): ≈ **0.25 px**.

  Best-case human stereoacuity is ~20-40 arcsec, but one display pixel is ~144 arcsec. **At 16 m a flat card is
  stereo-indistinguishable from the 3D body.**
- **Motion parallax** from a 0.15 m lean, across Δ = 0.5 m at 16 m, is ≈ 0.017° ≈ 0.4 px. The view angle to him changes by
  only 0.54°. No view-dependent sprites are needed for head motion.
- **When it breaks:** Δ = 0.5 m reaches 1 px at d ≈ √(0.063·0.5/6.98e-4) ≈ **6.7 m**, and 0.5 px at about 9.5 m. Below
  ~7 m the cardboard look can appear; at 3 m it is ~5 px and obvious. S30 is annotated "8m TO MAGE". At 8 m the error is
  ~0.7 px: borderline, and needs a depth sprite.
- **Yaw ±100°** does not change the view of the opponent; it only puts him at the edge of the FOV.
  - The trap is billboarding toward the **view direction** or the **per-eye camera position**. Under multiview, UE's
    `CameraPosition` differs per eye, so a camera-facing card turns differently in each eye, producing rivalry and
    "floating".
  - Face the card toward the **seat point** (or the HMD centre passed through an MPC), yaw only.
  - UE has a long history of billboard-in-one-eye and instanced-stereo issues
    ([UE-37611](https://issues.unrealengine.com/issue/UE-37611);
    [forum](https://forums.unrealengine.com/t/material-billboard-component-and-instanced-stereo/430762)), so use a plain
    static mesh card rotated from C++.
- **Lateral movement of the opponent:** a ±4 m strafe at 16 m is a ±14° view change. Painting from a single front facing
  is acceptable (2D-fighter convention). Three facings (−15°, 0, +15°) cost 3x the frames.

**Depth sprites and impostors.**
- A **depth sprite** is a card subdivided into a ~32x32 grid and displaced along the view axis by a per-frame depth map
  (from the rig's depth pass, or Video Depth Anything / DepthCrafter). It gives true stereo and parallax down to a few
  metres. Silhouettes rubber-sheet, so clamp displacement near alpha edges.
- **Pixel depth offset** on Android has a history of not working on mobile
  ([UE answers](https://answers.unrealengine.com/questions/668000/view.html)). Treat it as **[unverified on UE 5.8 mobile
  multiview]** and prefer vertex displacement.
- **Stereoscopic impostors** decouple binocular disparity from motion parallax. Waldow et al. (IEEE VR 2024) found that
  this cue inconsistency matters perceptually
  ([pdf](https://downloads.hci.informatik.uni-wuerzburg.de/2024-ieeevr-incoherent-depth-cues.pdf);
  [TH Köln impostor work](https://cg.web.th-koeln.de/impostor-based-rendering-acceleration-for-virtual-augmented-and-mixed-reality/)).
  It is irrelevant at 16 m and relevant for close creatures.
- **Octahedral impostors** are for static objects seen from many angles. They are not needed for Brennic (view angle
  < ±15°) and are wrong for animated characters.

**Examples in VR.**
- QuestZDoom and other Doom VR ports run flat sprite enemies in stereo; players accept them as a retro look
  ([Brian Koponen](https://www.briankoponen.com/how-to-run-doom-virtual-reality/)).
- *Pop-Up Pilgrims* (PSVR) bends a 2D stage around the player in depth layers
  ([Push Square, 2018-02](https://www.pushsquare.com/news/2018/02/interview_dakko_dakko_on_bringing_a_ps_vita_classic_to_psvr)).
- Academic: billboarding plus parallax plus shape interpolation for 2D characters in VR
  ([Bournemouth](https://eprints.bournemouth.ac.uk/29573/)).
- No 2024-2026 commercial VR title with painted or AI-generated sprite opponents was found. **[gap]** Treat our case as
  unproven in market, but sound by the numbers.

**Lighting and shadows for sprites.**
- Paint the key light in, matching the backdrop.
- Add dynamic spell light through a per-frame normal map: render the rig's normal pass for the same pose (aligned by
  construction), or use NormalCrafter for painted detail
  ([Comfy workflow](https://comfy.org/workflows/utility-normal_crafter-video-9cca5e2b3dde/)).
- Use half-Lambert plus rim in an unlit custom material.
- Shadow: a sheared, darkened copy of the same frame on the floor, plus a blob for contact.
- Edges: Masked with alpha-to-coverage under 4x MSAA. This gives soft chalk edges, writes depth and needs no sorting
  **[unverified on mobile multiview; test]**.

### 4. Generative video to game sprites and animation (2025-2026)

**Local models.**
- **LTX-2.5**: Lightricks, 22B, open weights released 2026-08-11
  ([comfyui-wiki](https://comfyui-wiki.com/ko/news/2026-08-11-ltx-2-5-open-weights-release);
  [Comfy docs](https://docs.comfy.org/tutorials/video/ltx/ltx-2-5)).
  - Distilled transformer (reported fixed 8 steps, CFG 1), Gemma-4-12B text encoder, video and audio VAEs.
  - Native T2V, I2V and **FLF2V** workflows (first image to `image`, last to `last_frame`). The community also wires
    `LTXVAddGuide` keyframes (frame index −1 for the last frame).
  - Output up to 4K HDR and 50 fps; "auto duration" from the prompt.
  - IC-LoRA control: LTX-2.3 has a **Union Control** adapter (depth, canny, pose)
    ([LTX docs](https://docs.ltx.video/open-source-model/integration-tools/ic-lo-ra-adapters)). For 2.5, separate canny,
    depth and pose IC-LoRAs are reported but not confirmed in the official file list **[unverified]**.
  - Character LoRAs are trainable with the LTX trainer
    ([LTX blog](https://ltx.io/blog/how-to-maintain-character-consistency-in-ai-video)). Lightricks notes that an IC-LoRA
    learns the *control*, not fine identity.
- **MiniMax H3**: 33.1B, open weights 2026-08-03, native ComfyUI (PR #15224)
  ([comfyui-wiki model page](https://comfyui-wiki.com/en/models/minimax/minimax-h3)).
  - **FL2VA**: T2V, first frame, last frame, and first+last frame.
  - **Ref2VA**: up to 9 images, 3 videos and 3 audio clips (12 files maximum).
  - 768 px short edge by default, 24 fps, ≤15 s, int8 weights ~19.5 GB. The 2K regenerator and Context-IR are not in
    the open release.
  - **Licence territory exclusion** (see the go/no-go risks). Ref2VA with multiple identity images is its main strength
    for consistency. It has no structure control (pose or depth) **[as far as documented]**.

**Comparison (hosted or other).**
- **Wan 2.2 Animate** (14B, Apache-2.0, native ComfyUI) has a "Move" mode: it drives a reference-image character with the
  motion of an input video ([Comfy docs](https://docs.comfy.org/tutorials/video/wan/wan2-2-animate)). This is the
  cleanest open "pose-driven from our rig render" path. It is not installed and needs a download.
- **Kling 3.0** and **Seedance 2.5** lead on reference-held identity in vendor comparisons; Seedance 2.5 was announced
  2026-07-31 with up to 50 reference inputs, per secondary sources
  ([WaveSpeed comparison](https://wavespeed.ai/blog/ai-comparisons/kling-4-0-vs-veo-3-1-vs-seedance-2-5/)).
- **Veo 3.1** produces fixed 4/6/8 s shots. No reliable Veo 4 or Wan 3.0 primary sources were found. All of these are
  paid and need approval.

**Practitioner pipeline and pitfalls.**
- The vendor-described flow is image → video → frame extract → background removal → atlas
  ([Sorceress, vendor](https://sorceress.games/blog/how-to-make-a-sprite-sheet-in-2-minutes-with-ai-in-2026)).
- Independent warnings ([pixie.haus](https://pixie.haus/resources/creating-movement-engineering-pixel-art-animation-ai)):
  - Temporal models morph rigid structure.
  - Camera moves break sprites: lock the camera.
  - Start from a transparent or plain background.
- **Identity drift** comes mainly from small faces and mismatched anchors. Keep first and last frames from one
  generation (the registry's "cut from one cloth" rule).
- **FLF pacing pitfall:** a clip pinned at both ends often holds still, then snaps to the last frame. Brief the motion
  explicitly and bridge short gaps (registry, `media-generation/.../generated-shot-sourcing`).
- **Loops:** avoid "starts" or "finishes" wording. Review the loop as a loop.

**Mitigations, strongest first.**
1. **Structure from our rig.** Render Brennic's existing clips from the seat camera as pose, depth and normal videos and
   drive the generation with them (Wan Animate Move, or LTX pose/depth IC-LoRA). Timing then equals the gameplay clip,
   and hand sockets are known per frame (MAGEVR_RIVAL_HAND already exists).
2. One clip per action; never chain clips for one action.
3. A Flux 2 identity master as the first frame, with Ref2VA or a character LoRA if drift remains.
4. Post-processing:
   - Pivot registration from DWPose feet keypoints.
   - Per-frame colour match to the master in Lab on the character mask (registry palette discipline).
   - Low-frequency deflicker.
   - Optionally **animate on twos** (12 fps from 24). This reads as hand-drawn, halves memory and hides boil.

**Matting.**
- Per-frame segmenters flicker at edges. The usual per-frame options are BiRefNet, RMBG-2.0 and **BEN2**, which
  self-reports strong hair matting ([BEN2 repo](https://github.com/PramaLLC/BEN2/)).
- **MatAnyone 2** (arXiv 2512.11782, Dec 2025) is a temporal video matter. It propagates from a first-frame mask, handles
  motion blur and fine hair, and reports Grad and Conn errors −27% and −22% versus MatAnyone 1
  ([paper](https://arxiv.org/pdf/2512.11782)).
- No independent head-to-head of BiRefNet, RMBG and BEN2 on hair or blur was found. **[gap]**
- **Do not generate fire inside the character clip.** Spawn the fire flipbook at the hand socket instead, so the character
  never needs a fire matte.
- **Native RGBA video:** Wan-Alpha (Wan 2.1 T2V 14B plus an RGBA LoRA, ComfyUI template) does transparency, smoke and
  glow ([Comfy docs](https://docs.comfy.org/tutorials/video/wan/wan-alpha)). It is text-to-video only; an RGBA
  image-to-video variant exists only as 2026 research ([arXiv 2608.09355](https://arxiv.org/pdf/2608.09355)).

**Frame interpolation.**
- RIFE and FILM (ComfyUI-Frame-Interpolation) suit smooth body motion. They ghost on fire and make chalk grain swim.
- Prefer generating at 24-25 fps natively and decimating, or engine-side motion-vector blending for VFX.

**Frame counts and atlas [calc].**
- Brennic at 16 m is ~6.6° tall ≈ 165 px per eye at 25 PPD. Use **256² frames** (character ≈ 200 px tall, 1.2x
  oversample), or 384² if the 8 m distance from S30 ever returns.
- A set of about 224 frames at 12 fps:
  - idle loop 32
  - 3 casts x 20
  - hit 12
  - ward/block 16
  - death 36
  - victory 24
  - strafe L/R 2 x 16
  - stagger 12
- That fits **one 4096² page** (256 slots): ASTC 6x6 RGBA ≈ 7.5 MB, about 10 MB with one mip chain. An optional normal
  page doubles it.
- Gutters: extrude 2-4 px per region, and more if mips go deep (registry
  `sprite-and-atlas-production/atlas-packing-and-bleed-margins`).

### 5. Video-generated VFX flipbooks

- **Generation:** a seed still from Flux 2 (chalk-pastel flame or water stroke on pure black, centred, no ground), then
  LTX-2.5 I2V (distilled) at 4-6 s with a static camera. Wan-Alpha is the RGBA alternative.
- **Loops:**
  - Find the best-matching frame pair at least 1.5 s apart (perceptual distance), cut there, and close the gap with an
    FLF2V bridge (first = the cut's last frame, last = its first frame).
  - Or crossfade 8-12 frames.
  - Identical first and last frames tend to give a static clip (the registry pitfall above).
- **Alpha from black:**
  - For additive VFX (fire, glow, chalk sparks) you need no alpha: RGB on black *is* the emission. Crush the noise floor
    with `rgb' = max(rgb − b0, 0)/(1 − b0)`, b0 ≈ 0.02-0.05, or the quad prints as a grey rectangle.
  - For darkening or occluding VFX (smoke, a water body), estimate alpha = clamp(k·max(r,g,b)), store premultiplied, and
    use UE's AlphaComposite (premultiplied) blend.
  - Chalk-on-slate water is mostly bright strokes, so additive works there too.
- **Motion-vector blending:**
  - Compute optical flow between frames, either offline with RAFT, or with
    [henlo Flipbook Tool](https://henlo.gumroad.com/l/flipbooktool) (image sequence → optical-flow flipbook) or TFlow
    (UE 4.26-5.0 listed **[5.8 unverified]**).
  - Encode flow in RG at half resolution ("half of the original flipbook resolution is way enough", per TFlow's note in
    [realtimevfx](https://realtimevfx.com/t/motion-vectors-for-animated-textures-how-do-you-generate-them/575/11)).
  - In the material, do 2 samples per frame plus a flow offset. This lets 16-36 frames at 12-15 fps look like 60, so it
    is Quest-friendly. The extra texture fetch is cheaper than more frames or more particles
    ([UE answers](https://answers.unrealengine.com/questions/437785/view.html), old).
- **Sizes:** fire at hand 6x6 or 8x8 frames of 256² on a 2048² page (ASTC 6x6 ≈ 1.9 MB); ward water 512² frames, 4x4 or
  6x6 on 2048²; sigil and sparks 128-256².
- **Overdraw on Quest:** fill cost is coverage x layers x shader cost. Blending defeats the tiler's hidden-surface
  removal.
  - Cap blended layers per pixel and cost each effect at its *nearest legal distance* (registry
    `standalone-headset-frame-budgets/overdraw-and-transparency-ceiling`).
  - The player's own fire and the ward at 0.3-1.5 m are the worst case.
  - Use Niagara's sub-UV **cutout geometry** (8-vertex bounding shapes) to trim empty quad area.
  - Keep the ward a single continuous ribbon mesh; this repo already found that overlapping cards double the
    translucency (HANDOFF r9).
  - Meta's Showdown port disabled Mobile HDR to cut per-pixel cost
    ([Meta blog](https://developers.meta.com/horizon/blog/showdown-on-quest-part-2-how-we-optimized-the-pc-vr-demo-for-meta-quest-2/)).

### 6. Quest 3 constraints for this route [calc unless linked]

- **Memory:** a 5.75 GiB PSS limit ([Meta](https://developers.meta.com/horizon/documentation/unreal/po-memory-ram/)).
  The whole image-based set budgets as follows:

  | Item | Size (ASTC 6x6 unless noted) |
  |---|---|
  | Cubemap backdrop | 15 MB |
  | 2 near-layer cards | 4 MB |
  | Brennic colour and normal pages | 20 MB |
  | 4-6 VFX pages | 10 MB |
  | Crowd strips | 5 MB |
  | Hands, 2048² colour and normal (4x4) | ~11 MB |

  The total is **under 70 MB**, against a texture budget the registry says to declare up front (`render-submission-economy`).
- **ASTC rates:** 4x4 = 8 bpp, 5x5 = 5.12, 6x6 = 3.56, 8x8 = 2.
  - Chalk grain hides block artefacts, but thin bright chalk lines on dark slate are the flat-art case where block
    compression halos edges (registry `lossy-compression-acceptance-for-flat-art`).
  - Measure 4x4 against 6x6 on real crops (E7).
- **Texture size:** use 4096² pages and 2048² cube faces. The Adreno 740 hardware limit (likely 16384) and UE's mobile
  LOD-group caps are **[unverified]**.
- **Draws:** backdrop 1-3, near cards 2, Brennic 2 (card plus shadow), VFX 5-15, hands 2-4. That is far under the
  ~150 budget, compared with the current D-G4 chalk median of ~107.
- **Triangles:** proxies of a few thousand triangles, against BasePass 167k today.
- **Transparency:** opaque backdrop drawn first; sprites masked with alpha-to-coverage (depth-writing, no sort);
  additive VFX unsorted; alpha-blended VFX sorted back to front, kept few and small.
- **Sharpness:** a painting looks sharp when its texel density at the eye reaches ~20 px/° (render scale 1.0) to ~25 px/°
  (panel-native). For angular size A degrees you need ~22·A texels across. The 2048² 90° cube face meets this at centre.
  Brennic's 165 px needs a ≥200 px frame.

### 7. Hands

- **Shipped practice.**
  - Meta's hand-representation guidance: fade fill and outline at the wrist (wrist angle tracks poorly); fade the hands
    on low confidence or when one hand occludes the other
    ([Meta design: hand representation](https://developers.meta.com/horizon/design/hand-representation/)).
  - Custom skeletal hand meshes in UE through Meta's sample
    ([unreal-sample-hand](https://developers.meta.com/horizon/documentation/unreal/unreal-sample-hand)).
  - Stylised Quest games paint the detail (and even outlines) into textures and keep the shading simple (*Across the
    Valley*, above).
  - No postmortem specific to painterly hands was found. **[gap]**
- **Recommended look recipe** (mobile, cheap):
  - A light-neutral painted albedo carrying the creases, knuckles, veins and pastel colour variation.
  - Shading live: stepped half-Lambert from the backdrop's key direction, plus **tonal art map** hatching (Praun et al.,
    SIGGRAPH 2001: 3-6 hatch tones in one texture, blended by tone, so stroke density follows light without swimming).
  - Fresnel rim, the same spell-glow MPC term as the floor, and a wrist fade into the cuff.
  - Baked shading is wrong the moment the palm turns; that is why the albedo must stay light-neutral.
- **Generating the painted texture.**
  - Multi-view projection: render 6-8 views of the posed hand, generate each with Flux 2 (depth plus canny control, S30
    hand crops as reference), then project and blend into UV.
  - Tools:
    - **StableGen** (Blender add-on; sequential mode inpaints view after view for consistency; FLUX.1-dev and
      Qwen-Image-Edit tested late 2025; [repo](https://github.com/Zaws77/StableGen))
    - **StableProjectorz** (standalone; multi-view projection; SD and Flux)
    - ComfyUI **"Projection MultiView Texturing"** ([node](https://comfy.icu/node/VisualBrunoToolsProjectionMultiViewTexturing))

    Whether these support FLUX.2 is **[unverified]**. The fallback is to generate the views in ComfyUI and project them
    with StableGen or Blender camera projection.
  - **Hunyuan3D-Paint 2.1** (open, ~2B, ~21 GB VRAM; mesh-conditioned multi-view PBR on *your* mesh;
    [arXiv 2506.15442](https://arxiv.org/html/2506.15442v1)) is a one-click alternative. It tends to photoreal PBR, so
    expect to restyle its output.
- **Texture size:** a hand at 0.35 m spans ~30° ≈ 750 px. Use 2048² for hand plus forearm, mirrored for both hands.

### 8. Other risks and alternatives

- **Gaussian splatting on Quest.**
  - Meta's Spatial SDK advises staying under 150k splats
    ([Meta splats doc](https://developers.meta.com/horizon/documentation/spatial-sdk/spatial-sdk-splats/)).
  - One developer reported poor standalone performance even at 50k
    ([Meta forum](https://communityforums.atmeta.com/discussions/Questions_Discussions/issues-with-gaussian-splatting-integration-in-meta-spatial-sdk-v0-9-2-%E2%80%93-standalo/1358674)).
  - Splats are sorted, blended overdraw; Quest support in UE is third-party; stylising them is open research.
  - **No** for this project.
- **NeRF:** not viable on Quest.
- **3D generators (late 2026).**
  - TRELLIS.2: local, 4B, MIT, PBR GLB, 512³-1536³; needs retopology for real time
    ([Comfy docs](https://docs.comfy.org/tutorials/3d/trellis2)).
  - Hunyuan3D 3.0/3.1: hosted only; reportedly not open-sourced.
  - Rodin Gen-2.5 (May 2026): strongest raw geometry, per guides.
  - Meshy 6 and Tripo 3.1: a Meshy-authored test found no single winner
    ([HackerNoon](https://hackernoon.com/how-i-stress-tested-3-ai-3d-generators-on-the-same-inputs-what-the-numbers-actually-show?source=rss)).

  None publishes evidence of *stylised painterly* texture quality at 2020s level. **[gap]** The owner's verdict on the
  TRELLIS round is that the deficit is surface and animation, which these tools do not fix for an animated character at
  16 m.
- **World generators.**
  - World Labs Marble: image to world, splats plus GLB mesh export; HQ mesh on the Pro plan. Paid, needs approval.
  - HunyuanWorld 1.0: open, panorama plus layered mesh, 2025; 2026 state **[unverified]**.

  Both are worth one experiment as proxy and layer generators, not as final art.
- **Keep 3D but improve the animation** (registry `generated-video-motion-extraction`): generate a performance video,
  solve it with markerless mocap onto the 3D Brennic. This fixes animation quality but not surface fidelity. It is the
  fallback if E3 fails on identity but the motions look good.
- **Paint in 3D:** Quill-style VR painting (strokes as true 3D geometry) is the hand-craft route to "drawn" 3D. It is
  high quality but artist time, not generation. Mentioned for completeness.
- **The "dead diorama" risk:** a static painting reads lifeless. Budget a few animated layers: crowd strips, banners,
  dust motes and drifting chalk specks.

---

## (c) Concrete pipelines

### Pipeline A: projected arena, sand and slate (cubemap from the seat)

1. **Proxies (UE).**
   - From the greybox, keep the sand disc and slate floor (planes), the wall ring (cylinder), the stands (one stepped or
     conical shell) and a sky dome.
   - Hide Brennic, VFX, rings, hands and HUD. Record `SeatEye` (seated eye centre) in an MPC.
2. **Capture (UE).**
   - Use 6 SceneCapture2D at `SeatEye`, each at 100-110° FOV (5-10° overlap per side), 2304² (cropped to 90° = 2048²).
   - Outputs per face: SceneDepth (linear, then normalised inverse depth), world normal, and an edge or lineart pass
     (greybox edges plus material-ID boundaries).
   - The back face (beyond ±155°) can be a cheap dark slate gradient.
3. **Front face first (ComfyUI, Flux 2 dev fp8).**
   - Models: UNETLoader `flux2_dev_fp8mixed`, CLIPLoader `mistral_3_small_flux2_*`, VAELoader `flux2-vae`.
   - Latent: `EmptyFlux2LatentImage` 2048x2048.
   - Prompt: structured JSON with the palette hex codes from S30 (slate #2B2F36-like, sand ochres, chalk whites), "chalk
     and pastel on dark slate, 2020s illustration detail, matte, no text".
   - References: ReferenceLatent x 2-3 with text-free S30 crops (stands, sand, slate border) at ~1 MP.
   - Control: `JLC Flux2 ControlNet Loader` (Union-2602), then `JLC Flux2 ControlNet Orchestrator` with depth (0.6-0.8,
     end ≈ 0.65) and canny/lineart (0.3, end ≈ 0.5).
   - Sampling: ~28-40 steps through `Flux2Scheduler` **[node names to verify]**.
   - Generate 4-8 seeds and pick by a blind judge against the new yardstick.
4. **Neighbour faces.**
   - Reproject the finished face(s) into the next 110° view (a small numpy script using the depth and the two camera
     matrices). Mark the overlap as known and generate the rest with `JLC Flux2 In/Out-Paint Adapter` under the same
     controls and references.
   - Order: front, left, right, floor, sky, back.
   - Optionally do the layout pass first with the [klein] 9B 360 ERP outpaint LoRA, then use its reprojection as a low-res
     reference per face.
5. **Assemble.**
   - Reproject all views into 6 cube faces at 2048² and feather overlaps by 32-64 px.
   - Paint out any lettering and any figure that should be dynamic.
   - Dilate each far layer 4 px behind near silhouettes.
6. **Near layers.** For each object nearer than ~10 m that overlaps a far surface (braziers, wardstones if baked, gate
   pillars):
   - mask it (BiRefNet or SAM)
   - inpaint the background behind it with Flux 2
   - project the background onto the far proxy
   - put the object on its own alpha card
7. **UE material** `M_ProjectedBackdrop`, unlit, opaque:
   - `dir = normalize(AbsWorldPos − SeatEye)`, then sample the TextureCube (ASTC 4x4 front-weighted, or 6x6).
   - Multiply by exposure and tint (MPC).
   - Add Σ spell-glow(pos_i, colour_i, radius_i) for ≤4 lights (MPC).
   - No shadows, no lighting. One material on all proxies: 1-3 draws.
8. **QA.**
   - Stereo pairs at ±32 mm for head offsets (0, ±0.15 m lateral, ±0.05 m vertical) and yaw −100/0/+100: look for paint
     sliding, tears and seam visibility.
   - The mobile proxy capture.
   - A blind judge on the new yardstick, against round 11.

### Pipeline B: Brennic's sprite animation set

1. **Contract** (write it down first; registry sprite rules):
   - 256² frames (384² if distance < 10 m is ever possible), pivot at the feet centre, 24 fps generated, 12 fps shipped
     (on twos).
   - Key light matching the backdrop; palette LUT from the identity master.
   - Per-frame metadata: pivot, hand_l, hand_r, release-frame events.
2. **Identity master.** One chalk Brennic still at the seat-camera angle (eye ~1.2 m high, 16 m away, so nearly level).
   Use Flux 2 dev with the Brennic multi-view masters (DECISIONS 10-24) plus the S30 Brennic crop as references, and
   pose plus depth control from a rig render of the idle pose. Generate on a flat background in the sand mid-tone, so
   edge fringes match where he stands.
3. **Driving data (UE).**
   - Play each clip (Idle, Cast/CastLunge, Hit, Death, plus a new strafe and victory) on the existing rig in front of the
     seat camera, locked off, 1024² crop.
   - Export RGB greybox, depth, normal and mask.
   - Run DWPose (comfyui_controlnet_aux `DWPreprocessor`) on the RGB for skeleton videos and the hand and feet tracks.
4. **Keyframes.** Flux 2 with pose and depth control plus the identity reference produces the first frame of each clip
   (and the last frame for loops and returns to idle, generated as two panels of one image when possible).
5. **Video, three arms; choose in E3:**
   - **(i) LTX-2.5 distilled.** I2V or FLF2V with the pose or depth IC-LoRA driven by the skeleton or depth video. If 2.5
     lacks one, use LTX-2.3 with the Union-Control adapter.
   - **(ii) MiniMax H3.** FL2VA first plus last frame, with Ref2VA identity images. This is a licence check first.
   - **(iii) Wan 2.2 Animate "Move".** The greybox video drives the identity master (download needed).

   For all three: prompt "static locked-off camera, plain background, no fire in hands", and reject clips whose
   background optical flow is above a threshold.
6. **Post.**
   - ffmpeg frame extract.
   - Pivot-align on the DWPose feet.
   - Lab histogram match to the master on the character mask; low-frequency deflicker.
   - Matte with BEN2 or BiRefNet on frame 1, then MatAnyone 2 propagation; check edge chatter on the cloak.
   - Decimate to 12 fps.
7. **Normals (optional).** Use the rig normal pass for the same frames, or NormalCrafter on the painted frames. Pack on
   page 2.
8. **Pack.** A 4096² atlas with 4 px extruded gutters, plus a JSON with pivots, sockets and events. Import with
   ASTC 6x6, mips limited to 2.
9. **UE.**
   - A card mesh trimmed to the union silhouette bounds. The material picks the frame from a scalar parameter that the
     RivalFigure presenter drives from gameplay clip time.
   - Masked with alpha-to-coverage; yaw-only facing toward `SeatEye`.
   - A shadow card on the floor from the same frame (multiply).
   - Hit flash and death "chalk-dust erosion" in the shader.
   - Spawn spell VFX at the per-frame hand socket.
10. **Acceptance metrics:**
    - identity: DINOv3 cosine against the master per frame (DINOv3 is already in the pipeline); flag the worst frames
    - flicker: temporal SSIM of the low-passed masked character
    - pivot jitter ≤1 px
    - a loop seam check

    Then a blind judge.

### Pipeline C: fire and water flipbooks

1. **Seed still** (Flux 2): "chalk-pastel fireball / water swirl, bright strokes, pure black #000 background, centred, no
   ground, no text", 1024². Use the S30 fire and water crops as references.
2. **Video:** LTX-2.5 distilled I2V, 5 s at 24-25 fps, 768-1024², "static camera, continuous looping turbulence". For
   true RGBA smoke or water veils, try Wan-Alpha T2V.
3. **Loop:**
   - Search frame pairs ≥36 frames apart for the minimum LPIPS or L2.
   - Cut, then bridge with LTX FLF2V (first = the cut's end frame, last = the cut's start frame, 1 s), or crossfade
     8-12 frames.
   - Check by playing it 3x.
4. **Black to emission and alpha:** crush the floor (b0 0.02-0.05). Fire and sparks stay additive RGB. For water bodies
   that should occlude, use alpha = clamp(k·max(rgb)) and store premultiplied.
5. **Downsample and pack:** fire 32-36 frames of 256² on 2048² (6x6 grid); ward water 16 frames of 512² on 2048² (4x4);
   sparks 16 frames of 128².
6. **Motion vectors:** RAFT or the henlo or TFlow tools, then an RG flow page at half resolution. A UE material function
   blends 2 frames with flow offset (strength tuned per effect).
7. **UE:**
   - Niagara sprite renderer with sub-UV plus cutout texture (8 vertices).
   - Additive for fire, glow and sigil; AlphaComposite for occluding water.
   - The ward stays one procedural ribbon whose UVs scroll the water flipbook.
8. **QA:** overdraw at the worst legal coverage (player's hand fire at 0.25-0.35 m; ward at full arc) in the mobile proxy
   GPU time. Visual check of the loop seam.

### Pipeline D: painted texture for the hands

1. **Mesh:** the existing SK_PlayerHands (MPFB). Confirm non-overlapping UVs, posed relaxed and open.
2. **Views (Blender):** 8 cameras (dorsal, palmar, radial, ulnar, fingertips, wrist-up, two dorsal 3/4) at 1024-2048.
   Render depth, normal and Freestyle lineart.
3. **Generate per view** (Flux 2 dev):
   - Controls: depth 0.8 plus canny 0.4.
   - References: S30 left and right hand crops, plus the previous accepted view (sequential, as in StableGen's mode).
   - Prompt: "evenly lit, flat diffuse, no cast shadows, chalk and pastel skin: creases, knuckles, veins, warm tan,
     visible pastel strokes".
4. **Project and blend:** StableGen or StableProjectorz, or Blender camera projection plus bake. Weight by cos²(view·
   normal) with visibility tests. Bake to 2048² UV.
5. **Fill:** inpaint UV-space holes (between fingers) with Flux inpaint, or by hand. Bake cavity and curvature masks from
   the mesh.
6. **Shader** (M_Q_Hands successor):
   - albedo x stepped half-Lambert (backdrop key direction)
   - TAM hatching by tone (in UV space)
   - cavity darkening
   - fresnel rim
   - spell-glow MPC term
   - wrist fade

   No hull pass (it costs +2 draws and doubles the fill, as *Across the Valley* also found).
7. **QA:** play the recorded hand clips through the pipeline. Check stroke stability, knuckle stretching under skinning
   and the near-field read at 0.3 m. Compare against HERO-7.

---

## (d) Experiments that settle the open questions fastest

| # | Experiment | Settles | Time | Pass bar |
|---|---|---|---|---|
| E1 | One projected cube face (front) on greybox proxies; stereo-pair captures at 0/±0.15 m and yaw ±100°; mobile proxy | Q1 registration, paint sliding, sharpness, cost | 0.5-1 day | No visible sliding at ≥10 m; judge prefers it over round 11 for sand and arena |
| E2 | Front + left + right + floor faces by overlap outpainting; seam visibility; S30-crop refs only vs refs + quick [klein] LoRA | Q2 cross-face consistency | 1 day | Seams invisible at 1:1; no style drift a judge can spot |
| E3 | Brennic Cast clip (47 frames) through arms (i) LTX-2.5 + pose/depth, (ii) H3 FL2VA (licence permitting), (iii) Wan Animate Move; matting BEN2 vs MatAnyone 2 | Q4, the route's biggest risk | 2-3 days | DINOv3 sim ≥ agreed bar on every frame; no visible boil at 12 fps; cloak edge stable; timing matches the clip |
| E4 | Sprite card vs 3D Brennic side by side at 16, 8 and 4 m in computed stereo pairs (later on headset or Link when allowed) | Q3 flatness thresholds | 0.5 day | Indistinguishable at 16 m; record the distance where it breaks |
| E5 | Fire loop via LTX-2.5 + bridge + motion vectors; overdraw at hand distance in the mobile proxy | Q5 | 1 day | Seamless 3x playback; GPU cost within the effect's share |
| E6 | Hand albedo by 8-view projection + TAM shader vs HERO-7 | Q7 | 1-1.5 days | Judge prefers it; no stroke swimming in recorded clips |
| E7 | ASTC 4x4 / 5x5 / 6x6 on chalk-line crops (FLIP or PSNR + visual) | Q6 compression class | 2 h | Pick the cheapest rate with no edge halos |
| E8 | TextureCube direction sampling vs equirect atan2 at half precision in the mobile proxy | Q1/Q6 precision | 2 h | No stepping or seams |
| E9 | Licence checks: H3 territory (apply if EU), FLUX.2 dev output clause, LTX revenue terms | Go/no-go | 1 h | Written confirmation in DECISIONS |
| E10 (optional, needs approval) | S30 (labels removed) into World Labs Marble; use its GLB as proxies or layers | Q8 | 0.5 day | Better proxies than the greybox, or drop it |

Recommended order: E9 and E7/E8 (cheap, today), then E1 to E2, with E3 in parallel (the critical path), then E5 and E6.
E4 runs alongside E3.

---

## Registry notes

**Read:**
- `concept-scene-parity`: scorecard and caps; it already supports "judge per element".
- `sprite-and-atlas-production`: the four relational defects (grid, frame-to-frame palette, anchor, atlas bleed) and the
  compression class rule.
- `standalone-headset-frame-budgets/overdraw-and-transparency-ceiling`
- `media-generation/.../generated-shot-sourcing`: conditioning ladder, paired anchors, the FLF hold-then-snap pitfall.
- `generated-video-motion-extraction` (the keep-3D fallback).

**Gaps worth contributing back** (product-neutral):
1. Camera-projected or image-based scenes for a bounded seated viewing volume: the disocclusion and registration
   arithmetic of §1, cubemap-from-seat projection, the layer rule (d_near < ~10 m).
2. Stereo validity of flat or depth sprites: the IPD·Δ/d² rule, the ~7 m threshold for a 0.5 m-deep body at 25 PPD, and
   billboard-facing toward the seat instead of per-eye cameras.
3. Generated video to sprite set: rig-driven structure, matting choice, on-twos decimation, per-frame socket metadata.
4. VFX flipbooks from video models: black-to-emission levels, premultiplied alpha, loop bridging, flow pages.
5. Multi-view projection painting of a skinned mesh with light-neutral albedo and live hatching.

## Uncertainty ledger (not verified)

- Exact ComfyUI 0.42 node names for Flux 2 (`EmptyFlux2LatentImage`, `Flux2Scheduler`), fp8 text-encoder availability,
  and timings on the RTX Spark.
- An LTX-2.5 pose or depth IC-LoRA (only LTX-2.3 Union-Control is confirmed); MiniMax H3 structure control.
- Pixel depth offset and alpha-to-coverage under UE 5.8 mobile multiview on Quest; UE mobile texture-size caps.
- FLUX.2 support in StableGen and StableProjectorz.
- Effective PPD figures are my arithmetic from Meta's specs, not measurements.
- No 2024-2026 shipped VR title using AI-generated projected paintings or sprite opponents was found. The route is sound
  by the numbers, but unproven in market.
