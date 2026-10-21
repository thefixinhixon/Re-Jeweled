// Stub implementations for Xbox 360 imports not provided by the SDK.
// Bejeweled Blitz Live links the USB camera API (XUsbcam*) - the Xbox 360
// Vision camera. No-ops: the game works without camera support.
// (Same family The Maw needed; this title uses 4 more entry points.)

#include <rex/ppc.h>

extern "C" void __imp__XUsbcamCreate(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamDestroy(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamReset(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamGetState(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamReadFrame(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamSnapshot(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamGetView(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamSetView(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamSetCaptureMode(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamGetConfig(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
extern "C" void __imp__XUsbcamSetConfig(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = 0; }
