# Bejeweled Blitz Live — GPU draw bug: forensic state (2026-10-04)

## Verdict so far
Game side is 100% proven good. The invisible GFx art/text is a draw-submission/
translation bug in the Vulkan layer — the same failure upstream Xenia Canary
reports for this title (game-compatibility #563, title 58410A57,
"nearly all the graphics are missing", label gpu-drawing-corrupt).

## Proven facts
- File layer fixed and verified (SKIP-FIX + XFALLBACK in generated code;
  test #6: 102/102 VFS misses recovered, zero failed movie preloads).
- Assets byte-perfect (offline pak decode; in-RAM index verified).
- Guest textures contain correct data (decoded from live RAM):
  - font/string atlas 1024x1024 k_8 @ phys 0x1EA2E000 — crisp rasterized
    text ("Loading", "press t...", outlined display font).
  - 512x512 DXT title strip — proper anti-aliased art.
  - 2048x1024 DXT @ 0x1E2EE000 — blue nebula background art.
  - 1024x512 DXT @ 0x1E1EE000 — the circuit-board frame (this one IS
    visible on screen, as are solid fills and the green PLAY wordmark).
- ~250 DXT textures upload during boot; all "Loaded" activity is in the
  first ~20 s. Only six 512x512 (title strips) ever get drawn-bound.
- Resolves go ONLY to the two 1280x720 swap textures (0x1ECD3000 /
  0x1F06B000, +0x1F403000 during boot). No offscreen movie render targets:
  GFx draws straight into the main EDRAM framebuffer.
- The presented frame is NOT recoverable from the arena: swap-texture
  guest regions read 0.00% nonzero even mid-run (presenter is host-side).

## Arena forensics (reusable)
- Arena file: /dev/shm/xenia_memory_* (world-readable; survives exit).
- RAM phys P -> file offset 0x100000000 + P. System/XEX VA (0x80000000+)
  -> offset == VA. Guest heap VAs (0x4xxxxxxx) are NOT identity mapped.
- Tools: project/tools/framegrab/framegrab.py (textures -> PNG, Xenos
  detile ported from SDK texture_util::GetTiledOffset2D) and dxtdump.py
  (DXT3 decode, big-endian endpoints).

## Next frontiers (pick up here)
1. Guest PM4 command stream: find the CP ring buffer, decode DRAW_INDX
   packets + register state around GFx batches (blend, alpha test,
   texture fetch constants) — what does the guest actually ask for?
2. Guest D3D probe: the XDK D3D is statically linked; intercept the
   HAL's draw calls to count image-shape draws and their state.
3. SPIR-V cache audit: find the GFx bitmap pixel shader among the boot
   pipelines (hashes in trace "Creating graphics pipeline state" lines)
   and check its texture fetch translation.
4. Bisect by state: which visible elements share state with invisible
   ones? (circuit frame texture visible; title strips + text invisible.)

## Shader-cache recon (2026-10-04 pm)
- Cache lives at ~/.local/share/bejeweled/cache/shaders/shareable/
  (58410A57.vkspv = "XSPV" container; SPIR-V modules carvable by the
  0x07230203 magic; spirv-dis is installed on this box).
- Muse solo runs populate /home/muse/.local/share/bejeweled (only boot
  VS modules so far - 10 s runs end too early). NEXT STEP: one ~90 s
  solo run (warn Jason: window pops on his display) to reach the menu
  and populate the cache with the full GFx shader set, then carve and
  disassemble the GFx bitmap pixel shader and audit its texture fetch.

## Shader harvest + audit (2026-10-04 pm, solo run approved by Jason)
- Harvest run (100 s) grew muse cache vkspv 18 KB -> 131 KB. 12 modules
  carved and hash-mapped: presenter pair (VS 1E6883FC / PS A4A965C1),
  solid-fill pair (VS 103B6AAE / PS 3A92D78F, 0 samples - the VISIBLE
  fills), and three textured pairs: D5E12A97/A0B5089C (6 samples),
  D19182BE/E5529982 (2 samples, 3 interpolators), 2D12E1F6/F7919E8E
  (2 samples). All VS/PS interpolator interfaces match within pairs.
- The textured PSs contain a runtime OpSwitch on the fetch-constant sign:
  case 3 (kGamma) is a piecewise decode that crushes normal colors to
  ~0-1.5%. BUT: it is standard translator furniture (25 of Splosion
  Man's 48 shaders have it, textures fine there) AND the guest texture
  descriptors in RAM prove it never engages here: census of the harvest
  arena found 550 DXT2_3 descriptors + 188 k_8, ALL sign (0,0,0,0) =
  kUnsigned, pitch/size sane. SIGN/GAMMA THEORY DEAD.
- Descriptor layout (xenos.h xe_gpu_texture_fetch_t): dword0 signs at
  bits 2-9 (x,y,z,w; 0=Unsigned 1=Signed 2=UnsignedBiased 3=Gamma),
  dword1 format bits 0-5 (DXT2_3=19) + base>>12 at bits 12-31, dword2
  w/h minus-1 in 13-bit fields. Guest descriptors stored BE in RAM;
  arena scan signature works (numpy, fmt+base+size filter).
- Remaining suspect space: the GFx batches' draw-call state itself
  (shader float constants e.g. texture matrix, blend state, primitive/
  index handling). Hook point located: VulkanCommandProcessor::IssueDraw
  (src/graphics/vulkan/command_processor.cpp:3594). PLAN: diagnostic
  SDK build logging per-draw (texture base, shader hashes, index count,
  blend/alpha state) for a boot-to-menu run; find the strip draws and
  diff their state against the visible circuit-frame draw.

## Addendum 2026-10-04 pm2 — draw-submission probes v1–v13 (IssueDraw instrumentation)

All probes live in VulkanCommandProcessor::IssueDraw (SDK src/graphics/vulkan/command_processor.cpp),
staged GPU lib only; game exe unchanged (a1b8355e). Solo runs reach the menu and idle there.

VERIFIED SANE (each was a live suspect, each cleared with data):
- Draw census by pipeline (100 s run): bitmap pair VS D19182BE0951A742 / PS E5529982266D5A38 =
  22,516 draws; fills 103B6AAE/3A92D78F ~332 (idx up to ~21k, tessellated); text pair
  2D12E1F6/F7919E8E ~281 (idx=60, NON-indexed — index_buffer_info is nullptr); no-PS VS D3207A36
  ~5,153 (idx=6, stencil mask passes); present 1E6883FC/A4A965C1 idx=3.
- Bitmap draws: prim=kTriangleList, idx=30, indices local [0..7] = two quads (8 verts).
  Vertex fetch = slot 95 ONLY, type 3, size 24 words (96 B), addr immediately before the IB
  in the dynamic ring (0x1F7Bxxxx–0x1FCxxxxx). Vertex format 12 B: [s16 x|s16 y][4×u8][s16 u|s16 v]
  with endian mode 2 (full byte reverse before extraction). Decoded positions form clean
  rectangles: circuit ±(14445, 28870), strip ±(23583, 23508) — two nested quads (image + inner).
- VS float constants: bitmap VS reads packed guest c0,c1 (view matrix) and c4,c5 (UV matrix:
  u = x*2.1e-5 + 0.5 etc. — sane). c2/c3 (identity) are NOT used by this VS (packing!).
  PS reads packed guest c258 (cxform multiply) + c259 (add = 0). Menu-time c258: circuit
  (1,1,1,1); sampled strip (0.05, 0.80, 1.0, 0.30) = k/256-quantized, plausible GFx cxform.
  SQ_PS_CONST=0x000FF100 (base 256), SQ_VS_CONST=0x000FF000 (base 0).
- Blend (RB_BLENDCONTROL0): 0x07060706 = (SrcAlpha, OneMinusSrcAlpha) both channels;
  0x01060106 = (SrcAlpha, One) premultiplied. Both appear on visible and invisible draws.
- Stencil: only two depthctl/refmask states all run — 00700770/ref 0 (nebula only) and
  00700270/ref 1 (everything else; stencilfunc=Equal but stencil_enable=0). Mask pass
  (D3207A36) writes stencil: func=Always, zpass=Replace, ref=1, depthctl=00708771.
  Non-discriminating: visible circuit + invisible strips share state #2 at menu time.
- Scissor fullscreen (TL=0, BR=0x20002000) for all draws; RT state constant all run
  (surf=14000500 colorinfo=0 depthinfo=000102D0). Draw order per frame is normal painter's
  order: nebula+circuit first … fills/text … present last. ~70 draws/frame at menu.
  Long runs of 128×128/64×64 draws with c258a≈0.05–0.20 = menu particle sparkles (meant subtle).
- Textures: all DXT are LINEAR, 1 mip level, mip_address=0 (guest made them single-level).
  DXT3 alpha verified in RAM: logo strip 5.8% opaque + 2.8% mid (rest transparent) ✓;
  particle 128² healthy; nebula 100% opaque. NOTE: second 1024×512 texture @0x1E26E000 has
  alpha ≈ 0 (92% zero) yet draws with premult blend every frame — likely a glow/spec layer
  whose alpha is data; the visible circuit frame is 0x1E1EE000 (blend 07060706).
- PS E5529982 fully read: sample = OpImageSampleExplicitLod w/ Grad (DPdx/DPdy × 2^lod_bias),
  sign-switch on texture sign (=0 unsigned here), cxform c258/c259 applied, out.a = a × interp2.a.
  No OpKill. VS/PS interfaces match. Vulkan VALIDATION LAYER: zero VUIDs on a 45 s run.
- Guest resolve targets (0x1ECD3000 etc.) read 0.00% nonzero — final frames are host-side only;
  no frame readback path exists guest-side. Mesa screenshot layer loads but its capture config
  was not cracked (VK_LAYER_MESA_SCREENSHOT_CONFIG takes an option string, frames=/output_dir=
  syntax unconfirmed); Xvfb window captures black (RADV WSI doesn't land in the X pixmap).

REMAINING FORK (bisect running): llvmpipe (LVP ICD) run staged for Jason's eyes.
- Strips appear under llvmpipe → RADV-specific execution fault (cf. Condemned 2 unclipped-draw).
- Still blank → translator draw-submission logic (UpdateBindings / descriptor path / EDRAM).

## Addendum 2026-10-04 evening — probes v14–v61, PARKED by Jason

### Final reframing (supersedes earlier framing)
- The flat azure backdrop (0,106,193) is NOT a broken texture sample. Ownership paints (per-shader constant overrides in UpdateBindings) proved it is GFx solid shape fills: 54AAA688 (out = c0 constant; its c0 IS the azure) + A4A965C1 (out = vertex color). Guest-authored solid background — CORRECT rendering.
- The bug is confined to the two bitmap shaders' draws layered over the fills: E5529982 (main bitmap: out = lerp(vcol, tex, r2.z)*c2 + c3) and F7919E8E (vertex-color RGB, texture alpha only). Their art/text never appears: samples return a constant per texture, and E55's output alpha has the r2.w wedge (vertex dword2 = 0x00FFFFFF even / 0xFFFFFFFF odd verts → interp2.w alternates 0/1).

### Proven good (each individually verified)
- Guest side: textures valid in guest RAM; file layer solved (SKIP-FIX + XFALLBACK keepers).
- Guest microcode (via `--dump_shaders`): bitmap VS exports o0=UV (dp4 with c4/c5), o1=color (FMT_8_8_8_8 @+1), o2=dword2 (@+2); bitmap PS instruction 0 is `tfetch2D r0, r0.xy, tf0` — no hidden coord math. LOD bias from fetch dword4 = SExtract(bits 12..21)*1/32 = −0.5 (normal).
- Executed UVs: perfect full-range gradients on every GFx quad (SPIR-V store-redirect viz). Interp2 ≈ (1,1,1,~) → lerp factor z≈1.
- Texture images ON THE GPU (v44/v56 readbacks of the actual bound images): atlas 7,424 unique BC3 blocks, circuit 3,208, glow 8,736, nebula 26,166 — real art in all four. Uploads/conversion exonerated.
- Constants: packed float constants ({2,3} → slots 0,1) match shader access chains [0][0]/[0][1]; re-upload happens EVERY E55 draw (v55); c3 paint via register file floods the screen (v57) → constants reach the GPU. Earlier v34 "c2 pin no effect" was a phase/ownership artifact.
- Descriptors/binds/layouts: executor binds exactly the record-side sets (v45); views are correct BC formats, identity swizzle, correct subresource ranges; sampler create-info textbook (unnormalized never set); texture signs all-unsigned (v49), signed view null-by-design and irrelevant; pipeline layouts canonical (v54).
- Driver: llvmpipe (LLVM, software) reproduces the identical azure → NOT RADV/ACO. Two independent Vulkan implementations agree.

### The stubborn core
- Forcing E55's sample result to (1,1,1,1) or (0,0,0,0) via SPIR-V cache patch changes the output exactly as the formula predicts → the sample value propagates. But the natural sample is a per-texture CONSTANT (≈ the texture's average/corner) on every GFx draw, invariant to bound view swaps (v46/v51), coord forces, gradient zeroing, and forced reloads (v42).
- Cache-patching gotcha: the .vkspv module ORDER changes when the cache is rebuilt — re-scan module offsets before patching (bitmap PS twins: 2-sample modules; E55 ≈ 13.6 KB module, F7919E ≈ 10.5 KB, 2 interpolators/2 registers).

### Decision state
- Jason asked about a custom renderer (2026-10-04). Assessment: full renderer = reimplementing PM4/shader translation/texture cache/EDRAM — rejected. Viable middle path = BYPASS SHIM: intercept only E55+F7919E draws in the command processor, re-emit with a hand-written pipeline/descriptors/SPIR-V using the proven inputs (vertex data, texture views, register constants; shader semantics fully known — 7 and 5 ucode instructions). Jason chose "run the decisive test first" → test done (above) → PARKED before the shim decision. Resume point: build the shim, OR chase the last thread (why the natural sample is constant despite every input verifying).
- PROBE STATE: Jason's staged librexgpu-xenos.so carries probes v31–v61 (blend-force v39 ACTIVE, force-reload v42 ACTIVE, v50 alias, v55 print, v56 readbacks, v57–v61 paints). All patch scripts in ~/workspace/drawprobe_patchNN.py and copies in /home/muse/. Revert ALL before any ship build; keep only SKIP-FIX + XFALLBACK + final fix. Shader cache on the box is pristine (md5-verified after last patch run).

## SDK probe cleanup inventory (recorded 2026-10-04 night, surgery pending)

The house SDK tree (/home/muse/splosion-build/sdk) carries diagnostic probes in SEVEN files (full pre-cleanup diff backed up on the box at /tmp/full_tree_backup.patch — regenerate with `git diff` before surgery; the tree diff = house patch + probes, 63 files total):

| file | hunks | content |
|---|---|---|
| src/graphics/vulkan/command_processor.cpp | 10 | the v31–v61 draw probes (prints, paints, pins, swaps) |
| src/graphics/vulkan/deferred_command_buffer.cpp | 5 | v45 executor tracking |
| include/rex/graphics/vulkan/deferred_command_buffer.h | 3 | v45 declarations |
| src/graphics/vulkan/texture_cache.cpp | 4 | v53 view log + v56 readbacks |
| src/graphics/pipeline/texture/cache.cpp | 10 | v41 watch logs, v42 fmt-19 force-reload (ACTIVE behavior), v43 |
| src/system/kernel_state.cpp | 1 | [TT-PROBE] fprintf in TerminateTitle (harmless) |
| src/kernel/xam/xam_info.cpp | 4 | [LT-PROBE] prints + **XamLoaderLaunchTitle NEUTRALIZED (behavior change from the Bejeweled file-layer hunt — must restore real behavior)** + [TERM-PROBE] |

Method: per-file, revert only probe hunks (keep house hunks); rebuild librexgpu-xenos AND librexruntime; verify with `strings` (no PROBE tags) and a Bejeweled + Real Steel smoke each. Real Steel's staged libs currently include all of the above — its verdict runs remain valid (graphics probes are Bejeweled-hash-gated; v42 is semantically neutral; the LaunchTitle neutralization only touches title-relaunch paths), but no release ships until this cleanup is done and re-verified.

CLEANUP DONE 2026-10-04 night: all probe hunks reverted (7 graphics/kernel files + exception_handler_posix.cpp fault-suppressor). Tree diff now matches patches/house-sdk.patch exactly (58 files, 4,323 lines). Clean librexgpu-xenos.so + librexruntime.so rebuilt, 0 probe strings. NOT yet restaged into any run/ folder — staging waits for the Real Steel retest verdict to keep that test single-variable.

## Addendum 2026-10-05 night — solo session (Jason asleep), LOD branch closed

- CLEAN BASELINE RESTAGED: run/ now has the probe-free SDK libs (librexgpu-xenos
  md5 752bebb7, librexruntime 8bc03763 — runtime also carries the two Real Steel
  content fixes; tree = house patch 59 files). Exe unchanged a1b8355e.
- PIXEL ORACLE CRACKED (solo): Jason's display :1 refuses muse SSH runs (X auth
  died 2026-10-04 ~23:54). Working rig: Xvfb :99 (1280x800) + llvmpipe ICD
  (VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json) + `import -window root`
  — llvmpipe's WSI lands in the X pixmap (RADV's does not). Bug reproduces
  identically under llvmpipe (known). Mesa screenshot layer: grammar is
  frames=a/b/c or start-count-interval after individuals; comms socket mode did
  not activate via VK_LOADER_LAYERS_ENABLE here — abandoned, Xvfb rig suffices.
- BASELINE ON CAMERA: circuit frame visible, center black, no nebula/title/text
  (capture llv_menu.png). Menu-phase pipelines (F7919E8E) appear ~35 s in.
- SPIR-V AUDIT of E5529982 from the fresh cache (module 6 @ vkspv offset 72772,
  13624 B): two OpImageSampleExplicitLod, Grad operands = DPdxCoarse/DPdyCoarse
  of the half-texel-offset UV, scaled by Exp2(SExtract(fetch_const[1].dword0,
  bits 12..21) * 1/32). Emitted code is correct. The fetch-constant UBO content
  on the GPU was never read back by the v-probes — it was the prime suspect.
- LOD-ZERO TEST (decisive): in-place word patch of the cached module — the
  Exp2 result (%164) replaced with OpFMul %163 x 0.0 (+OpNop), verified via
  spirv-dis --raw-id. Both Grad operands become exactly (0,0) -> lambda clamps
  to mip level 0 at natural UVs. RESULT: frame pixel-identical to baseline
  (capture llv_lodzero.png). => LOD / gradient / bias path EXONERATED,
  including the fetch-UBO-bias theory. (Earlier "gradient zeroing" probes
  zeroed coordinates, which trivially yields a constant — not discriminating;
  this test zeroed only the derivatives.)
- REMAINING SUSPECT SPACE: the executed sample does not read the verified
  image through the verified coordinates. Next experiments, in order:
  (1) replace OpImageSampleExplicitLod+Grad with OpImageSampleImplicitLod in
  the cached module (NOP-padded) — tests the ExplicitLod execution path on
  both drivers; (2) sample a DIFFERENT texture slot / hardcode a neighboring
  texel fetch to see if output tracks the binding at all; (3) the bypass shim
  (hand-written pipeline for E5529982 + F7919E8E in IssueDraw) — now the
  front-runner if (1)/(2) stay negative.
- STATE AT PARK (06:30): no game running, /dev/shm clean, muse shader cache
  restored pristine (vkspv md5 0023b362 = .orig), Jason's own cache untouched.
  Captures in /tmp/bjw_shots/ on the box and Muse's workspace/bejeweled/.

## Addendum 2026-10-05 morning — phase-locked A/B; swap test negative

- ORACLE UPGRADED: Mesa screenshot layer works in-game once the lsfg-vk
  implicit layer is disabled (VK_LOADER_LAYERS_DISABLE=VK_LAYER_LS_frame_
  generation) — that layer was silently breaking the chain in all earlier
  attempts. Frame-indexed captures (frames=300/600) are the phase lock;
  wall-clock X11 captures are NOT comparable (idle animation runs on wall
  time; uncontrolled diffs run +/-5% of pixels either direction).
- Present rate under llvmpipe ~5/s early, slower during compile stalls;
  present 600 ~= settled idle state (circuit + black center in baseline).
- OPCODE TABLE (derived empirically from the module, do not trust memory):
  OpLoad=61, OpSampledImage=86, OpImageSampleImplicitLod=87,
  OpImageSampleExplicitLod=88. The first ImplicitLod attempt wrote 86
  (=OpSampledImage) and was rightly rejected by spirv_to_nir — INVALID
  test. A corrected patcher (87 + vec2 coord %165) is staged at
  ~/workspace/bejeweled/patch_implicit.py but UNRUN.
- VIEW-SWAP TEST (phase-locked A/B at presents 300/600): swapped the image
  operands of the two OpSampledImage instructions in E5529982 (unsigned
  view %115 <-> signed view %116). RESULT: no qualitative change — center
  still black, circuit unchanged in character; diffs within animation
  noise. If execution had been wrongly taking the signed (dummy) branch,
  the swap would have put the real image on that branch and art would
  have appeared. It did not. => the wrong-branch theory is dead in its
  strong form; the executed sample returns a constant through BOTH views.
- CACHE STATE: the runtime re-serialized the container once (140,964 ->
  140,284 B; only the tail module changed; E5529982 still at offset
  72772/13624 B). Current pristine baseline snapshot = vkspv.base2
  (md5 6e9313b8); the live cache is restored to it. All patchers assert
  against base2 now.
- STRATEGIC STATE: cache-patch fishing is done. Remaining paths are
  (a) a fresh-eyes source read of the translator/texture-cache sampling
  path (PM4 frontier #1 from the original hunt was never done), or
  (b) the BYPASS SHIM. Awaiting Jason's call (asked 2026-10-05 morning).

## Addendum 2026-10-05 07:00 - ImplicitLod avenue closed (tooling wall)

Corrected ImplicitLod patch (opcode 87, vec2 coord %165) applied cleanly
and the run executed, but llvmpipe's spirv_to_nir rejected the module at
the patched sample again (pipeline creation fails; E55 draws absent -
frame is a flat clear, not even the circuit). Both implicit-form attempts
died in Mesa's SPIR-V front-end at the same site; the NOP padding inside
the sign-selection blocks is the likely trigger, but regardless: the
ImplicitLod question cannot be answered via cache patching on this
driver stack. AVENUE CLOSED - not evidence about the bug itself.
Cache restored to base2 (6e9313b8), box clean. Decision between the
source hunt and the bypass shim is with Jason.

## RESOLVED 2026-10-12 — SDK exp_adjust bug (same root cause as Lollipop Chainsaw "wash")

Root cause: the house SDK's SPIR-V translator read texture exp_adjust from
fetch-constant dword4 bits 13..18 (overlapping LOD bias, dword4 bits 12..21)
instead of dword3. Bejeweled's GFx textures carry LOD bias -0.5 (= -16 as a
10-bit field, 0x3F0) -> bits 13..18 = 0b111000 = -8 -> every sample scaled
by 2^-8 = near-black. That's the "per-texture constant" seen in v31-v61 (the
LOD-zero test patched Exp2(bias) but not the second read of the same bits).
Fix = SDK commit 7efddc0 (spirv_translator_fetch.cpp reads word 3).

Verified on the lc-autotest rig (stagings/bjw-base vs bjw-expfix, scenario
scenarios/bjw-title.py): title luma 0.003 -> 0.287; logo, nebula, gem,
"Press Start" and the mode menu all render.
Staged: run/librexgpu-xenos.so = LC test3b build (md5 ab64dc7f); old lib kept
as librexgpu-xenos.so.pre-expfix. Stale shader cache moved to
~/.local/share/bejeweled/cache.pre-expfix-20261012 (must invalidate cache
after this fix, or old translations keep being served).
Gameplay verified 2026-10-12 (rig run 20261012-184414-bjw-play4, scenario
scenarios/bjw-play.py: wait 45 s, START, A (Play), Y (Classic), A (One
Player), random swaps). Board, gems, score, hint panel, timer bar, Game Over
and results screen all render; swaps score (7,550). Open question for a human
eye: the animated in-game background looks blocky/low-res - may be by design.

UPDATE 2026-10-12 evening - staged lib changed. The LC test3b lib (whole LC SDK
tree, newer than this exe/runtime) caused intermittent guest fault loops in boot
(threads F80001B8 @0x8227xxxx, F8000164 @0x82889718 movie reader) on Jason's
real pulse device: 4/6 boots vs 0/3 with the old lib. run/librexgpu-xenos.so is
now the OLD Oct 5 lib + one byte (offset 0xf547a 0x0D->0x16: exp_adjust
SExtract reads dword4 bits 22..27 = 0, same stopgap as LC). Rig: 4/4 clean boots,
art renders. test3b kept as *.test3b. Proper fix still = rebuild exe+runtime+gpu
from one SDK tree that includes 7efddc0.
Tip: --log_max_file_size_mb=4000 stops fault spam from rotating the boot away.

## PERF 2026-10-12 night - CPU 2.2-2.4 cores -> 0.3-0.9, fps unchanged (60)
Profile (rig, kwin display, silent private pulse, per-thread /proc + tsample):
- Main XThread ~98% always: XDK D3D fence wait sub_82418128 spinning on
  sub_82422E60 (cctpl / 32x db16cyc / cctpm + poll). Fix: project/src/perf_hooks.cpp
  overrides the weak sub_82422E60 -> original, then sleep 100 us while it
  returns 1 (still waiting).
- Audio Worker ~78%: SDK WaitMultiple trylock spin, fixed by SDK 2433346.
Rebuilt exe against SDK HEAD (codegen untouched; stale SDK PCHs deleted after a
system header update). run/ now = exe aaf482a1 + runtime ce87ebee + gpu
ab64dc7f (consistent set; proper exp_adjust fix). Previous set kept as
*.pre-perf / *.bytefix. Results (title/menu/gameplay): CPU 216/225/241% ->
31/43/90%; GPU busy 13-20% unchanged; Mesa overlay fps 60.0 avg in all phases
before and after.

VERIFIED by Jason 2026-10-12: rebuilt set (exe aaf482a1 / runtime ce87ebee / gpu ab64dc7f) - "seems to be perfect".

## PACKAGE Test1 (2026-10-12) - BejeweledBlitzLive-Linux-Test1.zip (md5 83d1ee0f)
- AppImage project/appimage/ReJeweled-x86_64.AppImage (md5 f75411ce): Re-Jeweled
  Launcher (house TP launcher, TP_GAME=bejeweled; copy of the LC tree at
  project/tp-launcher, build dir build-bejeweled) + run/ set + xctd.
- Stock data is XCTD-compressed -> launcher import makes a NEW copy
  (<dest>/BejeweledBlitzLive/gamedata) and decodes it with the bundled xctd
  (src/xctdprep.cpp: magic scan, in place, idempotent, adds default.xex).
  Verified: stock /mnt/sdb1/Games/Bejewled -> prepared copy identical to
  run/gamedata; AppImage binaries (.text == run set) played full rounds on
  the rig from that copy; launcher PLAY -> correct args/env, 0 faults.
- Not exercised: STFS LIVE-package import (no Bejeweled package on the box;
  same StfsPackage code as Maw/Splosion + the same prepare step).
- Rebuild: tp-launcher `cmake -S . -B build-bejeweled -G Ninja -DTP_GAME=bejeweled`,
  then `env -u LD_PRELOAD project/appimage/build-appimage.sh`.

### PACKAGE Test1 — import-flow revision (2026-10-12)
"Extracted folder..." now uses the chosen folder IN PLACE (no destination
prompt, per Jason). Already prepared -> registered silently; still
XCTD-compressed -> one prompt "Decompress here / Make a copy... / Cancel";
read-only -> copy only. Package import unchanged (new folder). New helper
findXctdFiles() in xctdprep. Verified from the rebuilt AppImage on Xvfb:
in-place decode of a stock copy is byte-exact vs run/gamedata, re-import is
silent. AppImage md5 8736c097, zip md5 779c4479.
