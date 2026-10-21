// Host-side overrides that stop guest busy-waits from burning a host core.
//
// sub_82422E60 is the XDK D3D spin step used by sub_82418128 (block until the
// GPU read pointer passes a fence): a cctpl / 32x db16cyc / cctpm delay, then
// a poll of the device state with a 5000-tick hang check. On Xbox 360 the
// delay yields the core to the sibling hardware thread; recompiled it is a hot
// spin that kept the Main XThread at ~98% of a core while the emulated GPU
// (paced by vsync) caught up. It returns 1 while the caller should keep
// waiting, 0 to give up.

#include <chrono>
#include <thread>

#include <rex/ppc.h>

extern "C" void __imp__sub_82422E60(PPCContext& ctx, uint8_t* base);

extern "C" void sub_82422E60(PPCContext& ctx, uint8_t* base) {
  __imp__sub_82422E60(ctx, base);
  if (ctx.r3.u32 != 0) {
    // Still waiting for the GPU: sleep instead of spinning. 100 us is far
    // below a 16.7 ms frame, so fence latency is unaffected.
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
}
