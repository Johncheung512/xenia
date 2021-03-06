/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/logging.h"
#include "xenia/cpu/cpu.h"
#include "xenia/emulator.h"
#include "xenia/gpu/graphics_system.h"
#include "xenia/gpu/xenos.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xboxkrnl_private.h"
#include "xenia/kernel/xboxkrnl_rtl.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {

// http://www.tweakoz.com/orkid/
// http://www.tweakoz.com/orkid/dox/d3/d52/xb360init_8cpp_source.html
// https://github.com/Free60Project/xenosfb/
// https://github.com/Free60Project/xenosfb/blob/master/src/xe.h
// https://github.com/gligli/libxemit
// http://web.archive.org/web/20090428095215/http://msdn.microsoft.com/en-us/library/bb313877.aspx
// http://web.archive.org/web/20100423054747/http://msdn.microsoft.com/en-us/library/bb313961.aspx
// http://web.archive.org/web/20100423054747/http://msdn.microsoft.com/en-us/library/bb313878.aspx
// http://web.archive.org/web/20090510235238/http://msdn.microsoft.com/en-us/library/bb313942.aspx
// http://svn.dd-wrt.com/browser/src/linux/universal/linux-3.8/drivers/gpu/drm/radeon/radeon_ring.c
// http://www.microsoft.com/en-za/download/details.aspx?id=5313 -- "Stripped
// Down Direct3D: Xbox 360 Command Buffer and Resource Management"

void VdGetCurrentDisplayGamma(lpdword_t arg0_ptr, lpfloat_t arg1_ptr) {
  *arg0_ptr = 2;
  *arg1_ptr = 2.22222233f;
}
DECLARE_XBOXKRNL_EXPORT(VdGetCurrentDisplayGamma, ExportTag::kVideo);

struct X_DISPLAY_INFO {
  xe::be<uint16_t> unk00;
  xe::be<uint16_t> unk02;
  xe::be<uint8_t> unk04;
  xe::be<uint8_t> unk05;
  xe::be<uint32_t> unk08;
  xe::be<uint32_t> unk0C;
  xe::be<uint32_t> unk10;
  xe::be<uint32_t> unk14;
  xe::be<uint32_t> unk18;
  xe::be<uint32_t> unk1C;
  xe::be<uint32_t> unk20;
  xe::be<uint32_t> unk24;
  xe::be<uint32_t> unk28;
  xe::be<uint32_t> unk2C;
  xe::be<uint32_t> unk30;
  xe::be<uint32_t> unk34;
  xe::be<uint32_t> unk38;
  xe::be<uint32_t> unk3C;
  xe::be<uint16_t> unk40;
  xe::be<uint16_t> unk42;
  xe::be<uint16_t> unk44;
  xe::be<uint16_t> unk46;
  xe::be<uint16_t> unk48;
  xe::be<uint16_t> unk4A;
  xe::be<float> unk4C;
  xe::be<uint32_t> unk50;
  xe::be<uint16_t> unk54;
  xe::be<uint16_t> unk56;
};
static_assert_size(X_DISPLAY_INFO, 88);

void VdQueryVideoMode(pointer_t<X_VIDEO_MODE> video_mode);

void VdGetCurrentDisplayInformation(pointer_t<X_DISPLAY_INFO> display_info) {
  X_VIDEO_MODE mode;
  VdQueryVideoMode(&mode);

  display_info.Zero();
  display_info->unk00 = (xe::be<uint16_t>)mode.display_width;
  display_info->unk02 = (xe::be<uint16_t>)mode.display_height;
  display_info->unk08 = 0;
  display_info->unk0C = 0;
  display_info->unk10 = mode.display_width; // backbuffer width?
  display_info->unk14 = mode.display_height; // backbuffer height?
  display_info->unk18 = mode.display_width;
  display_info->unk1C = mode.display_height;
  display_info->unk20 = 1;
  display_info->unk30 = 1;
  display_info->unk40 = 320; // display_width / 4?
  display_info->unk42 = 180; // display_height / 4?
  display_info->unk44 = 320;
  display_info->unk46 = 180;
  display_info->unk48 = (xe::be<uint16_t>)mode.display_width; // actual display size?
  display_info->unk4A = (xe::be<uint16_t>)mode.display_height; // actual display size?
  display_info->unk4C = mode.refresh_rate;
  display_info->unk56 = (xe::be<uint16_t>)mode.display_width; // display width
}
DECLARE_XBOXKRNL_EXPORT(VdGetCurrentDisplayInformation, ExportTag::kVideo);

void VdQueryVideoMode(pointer_t<X_VIDEO_MODE> video_mode) {
  // TODO: get info from actual display
  video_mode.Zero();
  video_mode->display_width = 1280;
  video_mode->display_height = 720;
  video_mode->is_interlaced = 0;
  video_mode->is_widescreen = 1;
  video_mode->is_hi_def = 1;
  video_mode->refresh_rate = 60.0f;
  video_mode->video_standard = 1;  // NTSC
  video_mode->unknown_0x8a = 0x4A;
  video_mode->unknown_0x01 = 0x01;
}
DECLARE_XBOXKRNL_EXPORT(VdQueryVideoMode, ExportTag::kVideo);

dword_result_t VdQueryVideoFlags() {
  X_VIDEO_MODE mode;
  VdQueryVideoMode(&mode);

  uint32_t flags = 0;
  flags |= mode.is_widescreen ? 1 : 0;
  flags |= mode.display_width >= 1024 ? 2 : 0;
  flags |= mode.display_width >= 1920 ? 4 : 0;

  return flags;
}
DECLARE_XBOXKRNL_EXPORT(VdQueryVideoFlags, ExportTag::kVideo);

dword_result_t VdSetDisplayMode(dword_t mode) {
  // Often 0x40000000.
  return 0;
}
DECLARE_XBOXKRNL_EXPORT(VdSetDisplayMode, ExportTag::kVideo | ExportTag::kStub);

dword_result_t VdSetDisplayModeOverride(unknown_t unk0, unknown_t unk1,
                                        double_t refresh_rate, unknown_t unk3,
                                        unknown_t unk4) {
  // refresh_rate = 0, 50, 59.9, etc.
  return 0;
}
DECLARE_XBOXKRNL_EXPORT(VdSetDisplayModeOverride,
                        ExportTag::kVideo | ExportTag::kStub);

dword_result_t VdInitializeEngines(unknown_t unk0, function_t callback,
                                   unknown_t unk1, lpunknown_t unk2_ptr,
                                   lpunknown_t unk3_ptr) {
  // r3 = 0x4F810000
  // r4 = function ptr (cleanup callback?)
  // r5 = 0
  // r6/r7 = some binary data in .data
  return 1;
}
DECLARE_XBOXKRNL_EXPORT(VdInitializeEngines,
                        ExportTag::kVideo | ExportTag::kStub);

void VdShutdownEngines() {
  // Ignored for now.
  // Games seem to call an Initialize/Shutdown pair to query info, then
  // re-initialize.
}
DECLARE_XBOXKRNL_EXPORT(VdShutdownEngines,
                        ExportTag::kVideo | ExportTag::kStub);

dword_result_t VdGetGraphicsAsicID() {
  // Games compare for < 0x10 and do VdInitializeEDRAM, else other
  // (retrain/etc).
  return 0x11;
}
DECLARE_XBOXKRNL_EXPORT(VdGetGraphicsAsicID, ExportTag::kVideo);

dword_result_t VdEnableDisableClockGating(dword_t enabled) {
  // Ignored, as it really doesn't matter.
  return 0;
}
DECLARE_XBOXKRNL_EXPORT(VdEnableDisableClockGating, ExportTag::kVideo);

void VdSetGraphicsInterruptCallback(function_t callback, lpvoid_t user_data) {
  // callback takes 2 params
  // r3 = bool 0/1 - 0 is normal interrupt, 1 is some acquire/lock mumble
  // r4 = user_data (r4 of VdSetGraphicsInterruptCallback)
  auto graphics_system = kernel_state()->emulator()->graphics_system();
  graphics_system->SetInterruptCallback(callback, user_data);
}
DECLARE_XBOXKRNL_EXPORT(VdSetGraphicsInterruptCallback, ExportTag::kVideo);

void VdInitializeRingBuffer(lpvoid_t ptr, int_t page_count) {
  // r3 = result of MmGetPhysicalAddress
  // r4 = number of pages? page size?
  //      0x8000 -> cntlzw=16 -> 0x1C - 16 = 12
  // Buffer pointers are from MmAllocatePhysicalMemory with WRITE_COMBINE.
  // Sizes could be zero? XBLA games seem to do this. Default sizes?
  // D3D does size / region_count - must be > 1024
  auto graphics_system = kernel_state()->emulator()->graphics_system();
  graphics_system->InitializeRingBuffer(ptr, page_count);
}
DECLARE_XBOXKRNL_EXPORT(VdInitializeRingBuffer, ExportTag::kVideo);

void VdEnableRingBufferRPtrWriteBack(lpvoid_t ptr, int_t block_size) {
  // r4 = 6, usually --- <=19
  auto graphics_system = kernel_state()->emulator()->graphics_system();
  graphics_system->EnableReadPointerWriteBack(ptr, block_size);
}
DECLARE_XBOXKRNL_EXPORT(VdEnableRingBufferRPtrWriteBack, ExportTag::kVideo);

void VdGetSystemCommandBuffer(lpunknown_t p0_ptr, lpunknown_t p1_ptr) {
  p0_ptr.Zero(0x94);
  xe::store_and_swap<uint32_t>(p0_ptr, 0xBEEF0000);
  xe::store_and_swap<uint32_t>(p1_ptr, 0xBEEF0001);
}
DECLARE_XBOXKRNL_EXPORT(VdGetSystemCommandBuffer,
                        ExportTag::kVideo | ExportTag::kStub);

void VdSetSystemCommandBufferGpuIdentifierAddress(lpunknown_t unk) {
  // r3 = 0x2B10(d3d?) + 8
}
DECLARE_XBOXKRNL_EXPORT(VdSetSystemCommandBufferGpuIdentifierAddress,
                        ExportTag::kVideo | ExportTag::kStub);

// VdVerifyMEInitCommand
// r3
// r4 = 19
// no op?

dword_result_t VdInitializeScalerCommandBuffer(
    unknown_t unk0,    // 0?
    unknown_t unk1,    // 0x050002d0 size of ?
    unknown_t unk2,    // 0?
    unknown_t unk3,    // 0x050002d0 size of ?
    unknown_t unk4,    // 0x050002d0 size of ?
    unknown_t unk5,    // 7?
    lpunknown_t unk6,  // 0x2004909c <-- points to zeros?
    unknown_t unk7,    // 7?
    lpvoid_t dest_ptr  // Points to the first 80000000h where the memcpy
                       // sources from.
    ) {
  // We could fake the commands here, but I'm not sure the game checks for
  // anything but success (non-zero ret).
  // For now, we just fill it with NOPs.
  uint32_t total_words = 0x1CC / 4;
  auto dest = dest_ptr.as_array<uint32_t>();
  for (size_t i = 0; i < total_words; ++i) {
    dest[i] = 0x80000000;
  }

  // returns memcpy size >> 2 for memcpy(...,...,ret << 2)
  return total_words >> 2;
}
DECLARE_XBOXKRNL_EXPORT(VdInitializeScalerCommandBuffer,
                        ExportTag::kVideo | ExportTag::kSketchy);

// We use these to shuffle data to VdSwap.
// This way it gets properly stored in the command buffer (for replay/etc).
uint32_t last_frontbuffer_width_ = 1280;
uint32_t last_frontbuffer_height_ = 720;

struct BufferScaling {
  xe::be<uint16_t> fb_width;
  xe::be<uint16_t> fb_height;
  xe::be<uint16_t> bb_width;
  xe::be<uint16_t> bb_height;
};
void AppendParam(StringBuffer& string_buffer, pointer_t<BufferScaling> param) {
  string_buffer.AppendFormat(
      "%.8X(scale %dx%d -> %dx%d))", param.guest_address(),
      uint16_t(param->bb_width), uint16_t(param->bb_height),
      uint16_t(param->fb_width), uint16_t(param->fb_height));
}

dword_result_t VdCallGraphicsNotificationRoutines(
    unknown_t unk0, pointer_t<BufferScaling> args_ptr) {
  assert_true(unk0 == 1);

  // TODO(benvanik): what does this mean, I forget:
  // callbacks get 0, r3, r4

  // For use by VdSwap.
  last_frontbuffer_width_ = args_ptr->fb_width;
  last_frontbuffer_height_ = args_ptr->fb_height;

  return 0;
}
DECLARE_XBOXKRNL_EXPORT(VdCallGraphicsNotificationRoutines,
                        ExportTag::kVideo | ExportTag::kSketchy);

dword_result_t VdIsHSIOTrainingSucceeded() {
  // Not really sure what this should be - code does weird stuff here:
  // (cntlzw    r11, r3  / extrwi    r11, r11, 1, 26)
  return 1;
}
DECLARE_XBOXKRNL_EXPORT(VdIsHSIOTrainingSucceeded,
                        ExportTag::kVideo | ExportTag::kStub);

dword_result_t VdPersistDisplay(unknown_t unk0, lpdword_t unk1_ptr) {
  // unk1_ptr needs to be populated with a pointer passed to
  // MmFreePhysicalMemory(1, *unk1_ptr).
  if (unk1_ptr) {
    auto heap = kernel_memory()->LookupHeapByType(true, 16 * 1024);
    uint32_t unk1_value;
    heap->Alloc(64, 32, kMemoryAllocationReserve | kMemoryAllocationCommit,
                kMemoryProtectNoAccess, false, &unk1_value);
    *unk1_ptr = unk1_value;
  }

  return 1;
}
DECLARE_XBOXKRNL_EXPORT(VdPersistDisplay,
                        ExportTag::kVideo | ExportTag::kSketchy);

dword_result_t VdRetrainEDRAMWorker(unknown_t unk0) { return 0; }
DECLARE_XBOXKRNL_EXPORT(VdRetrainEDRAMWorker,
                        ExportTag::kVideo | ExportTag::kStub);

dword_result_t VdRetrainEDRAM(unknown_t unk0, unknown_t unk1, unknown_t unk2,
                              unknown_t unk3, unknown_t unk4, unknown_t unk5) {
  return 0;
}
DECLARE_XBOXKRNL_EXPORT(VdRetrainEDRAM, ExportTag::kVideo | ExportTag::kStub);

void VdSwap(lpvoid_t buffer_ptr,  // ptr into primary ringbuffer
            lpvoid_t fetch_ptr,   // frontbuffer texture fetch
            unknown_t unk2,       //
            lpunknown_t unk3,     // buffer from VdGetSystemCommandBuffer
            lpunknown_t unk4,     // from VdGetSystemCommandBuffer (0xBEEF0001)
            lpdword_t frontbuffer_ptr,  // ptr to frontbuffer address
            lpdword_t color_format_ptr,
            lpdword_t color_space_ptr,
            lpunknown_t unk8,
            unknown_t unk9) {
  gpu::xenos::xe_gpu_texture_fetch_t fetch;
  xe::copy_and_swap_32_unaligned(
      reinterpret_cast<uint32_t*>(&fetch),
      reinterpret_cast<uint32_t*>(fetch_ptr.host_address()), 6);

  auto color_format = gpu::xenos::ColorFormat(color_format_ptr.value());
  auto color_space = *color_space_ptr;
  assert_true(color_format == gpu::xenos::ColorFormat::k_8_8_8_8 ||
              color_format == gpu::xenos::ColorFormat::kUnknown0x36);
  assert_true(color_space == 0);
  assert_true(*frontbuffer_ptr == fetch.address << 12);
  assert_true(last_frontbuffer_width_ == 1 + fetch.size_2d.width);
  assert_true(last_frontbuffer_height_ == 1 + fetch.size_2d.height);

  // The caller seems to reserve 64 words (256b) in the primary ringbuffer
  // for this method to do what it needs. We just zero them out and send a
  // token value. It'd be nice to figure out what this is really doing so
  // that we could simulate it, though due to TCR I bet all games need to
  // use this method.
  buffer_ptr.Zero(64 * 4);

  namespace xenos = xe::gpu::xenos;

  auto dwords = buffer_ptr.as_array<uint32_t>();
  dwords[0] = xenos::MakePacketType3<xenos::PM4_XE_SWAP, 63>();
  dwords[1] = 'SWAP';
  dwords[2] = *frontbuffer_ptr;

  // Set by VdCallGraphicsNotificationRoutines.
  dwords[3] = last_frontbuffer_width_;
  dwords[4] = last_frontbuffer_height_;
}
DECLARE_XBOXKRNL_EXPORT(VdSwap, ExportTag::kVideo | ExportTag::kImportant);

}  // namespace kernel
}  // namespace xe

void xe::kernel::xboxkrnl::RegisterVideoExports(
    xe::cpu::ExportResolver* export_resolver, KernelState* kernel_state) {
  auto memory = kernel_state->memory();

  // VdGlobalDevice (4b)
  // Pointer to a global D3D device. Games only seem to set this, so we don't
  // have to do anything. We may want to read it back later, though.
  uint32_t pVdGlobalDevice =
      memory->SystemHeapAlloc(4, 32, kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", ordinals::VdGlobalDevice,
                                      pVdGlobalDevice);
  xe::store_and_swap<uint32_t>(memory->TranslateVirtual(pVdGlobalDevice), 0);

  // VdGlobalXamDevice (4b)
  // Pointer to the XAM D3D device, which we don't have.
  uint32_t pVdGlobalXamDevice =
      memory->SystemHeapAlloc(4, 32, kSystemHeapPhysical);
  export_resolver->SetVariableMapping(
      "xboxkrnl.exe", ordinals::VdGlobalXamDevice, pVdGlobalXamDevice);
  xe::store_and_swap<uint32_t>(memory->TranslateVirtual(pVdGlobalXamDevice), 0);

  // VdGpuClockInMHz (4b)
  // GPU clock. Xenos is 500MHz. Hope nothing is relying on this timing...
  uint32_t pVdGpuClockInMHz =
      memory->SystemHeapAlloc(4, 32, kSystemHeapPhysical);
  export_resolver->SetVariableMapping("xboxkrnl.exe", ordinals::VdGpuClockInMHz,
                                      pVdGpuClockInMHz);
  xe::store_and_swap<uint32_t>(memory->TranslateVirtual(pVdGpuClockInMHz), 500);

  // VdHSIOCalibrationLock (28b)
  // CriticalSection.
  uint32_t pVdHSIOCalibrationLock =
      memory->SystemHeapAlloc(28, 32, kSystemHeapPhysical);
  export_resolver->SetVariableMapping(
      "xboxkrnl.exe", ordinals::VdHSIOCalibrationLock, pVdHSIOCalibrationLock);
  auto hsio_lock =
      memory->TranslateVirtual<X_RTL_CRITICAL_SECTION*>(pVdHSIOCalibrationLock);
  xeRtlInitializeCriticalSectionAndSpinCount(hsio_lock, pVdHSIOCalibrationLock,
                                             10000);
}
