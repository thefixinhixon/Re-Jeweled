# ReXGlue SDK patches

Re-Jeweled builds against **rexglue-sdk commit `f5337cd`** (upstream
main) plus the house changes below. The Test build 1 binaries were built
from exactly this state.

| File | What |
|---|---|
| `rexglue-sdk-f5337cd-to-test1.patch` | Everything from `f5337cd` to the Test1 build tree in one `git apply`-able diff |
| `focused/0001-fix-gpu-read-texture-exp_adjust-from-fetch-constant-.patch` | The missing-art fix alone: `exp_adjust` is read from fetch dword 3, not the dword 4 LOD-bias bits. Suitable for upstreaming. |
| `focused/0002-perf-core-remove-CPU-spin-waits-on-Linux.patch` | Fixes the audio-worker `WaitMultiple` spin and the other Linux spin-waits |

Apply:

```sh
cd rexglue-sdk
git checkout f5337cd
git apply --binary /path/to/Re-Jeweled/patches/rexglue-sdk-f5337cd-to-test1.patch
```

The combined patch also carries the other house work in that tree,
including the Lollipop Chainsaw fixes and the audio gain and stereo
cvars.
