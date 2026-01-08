#include "gx_hook_protocol.h"

// Draft-only hook stub. This is not compiled by the project.
// It documents the intended hook points and data flow.

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// Placeholder vtable layout for CGxDevice (offsets from Ghidra notes).
struct GxDeviceVtable {
  void* pad_00[10];
  void* DeviceCreate;     // +0x28
  void* DeviceDestroy;    // +0x2c
  void* pad_30[18];
  void* ScenePresent;     // +0x98
  void* SceneClear;       // +0x9c
  void* pad_a0[2];
  void* Draw;             // +0xa8
  void* pad_ac[9];
  void* BufLock;          // +0xd8
  void* BufUnlock;        // +0xdc
  void* BufData;          // +0xe0
  void* TexCreate;        // +0xe4
  void* TexDestroy;       // +0xe8
  void* pad_ec[12];
  void* ShaderConstantsSet; // +0x118
};

// The real device pointer is stored in g_theGxDevicePtr (0x00c5df88).
static GxDeviceVtable* g_real_vtable = nullptr;
static GxDeviceVtable g_hook_vtable = {};

static void InstallVtableHook(void* gx_device) {
  if (!gx_device) {
    return;
  }
  GxDeviceVtable** vtable_ptr = reinterpret_cast<GxDeviceVtable**>(gx_device);
  g_real_vtable = *vtable_ptr;
  g_hook_vtable = *g_real_vtable;

  // TODO: replace entries with hook thunks (Draw, ScenePresent, BufData, etc).
  *vtable_ptr = &g_hook_vtable;
}

// IPC send stub (replace with real transport: shared memory or named pipe).
static void SendCommand(const void* data, uint32_t size_bytes) {
  (void)data;
  (void)size_bytes;
}

}  // namespace

#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  (void)module;
  (void)reserved;
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      // TODO: locate g_theGxDevicePtr and install vtable hook.
      break;
    case DLL_PROCESS_DETACH:
      break;
    default:
      break;
  }
  return TRUE;
}
#endif
