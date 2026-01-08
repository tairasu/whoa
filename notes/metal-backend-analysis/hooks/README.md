# GX Metal Hook Draft (3.3.5a)

This directory is a draft for the 32-bit hook DLL and 64-bit Metal helper that
let the 3.3.5a client render through the Metal backend. It is a design and
stub layout only; no build integration yet.

## Goals

- Intercept GX device calls in the original 3.3.5a client.
- Translate GX state + resources into a Metal command stream.
- Run Metal in a 64-bit helper process while WoW.exe remains 32-bit (Wine).

## High-level flow

1) 32-bit hook DLL is loaded into WoW.exe (proxy `d3d9.dll` or `opengl32.dll`).
2) Hook Gx device creation and install a proxy vtable.
3) Proxy methods serialize GX calls into an IPC command stream.
4) 64-bit Metal helper receives commands and renders; it owns Metal resources.
5) Present/swap is driven by `ScenePresent` from the game.

## Hook points (binary addresses)

From the Ghidra work in `notes/metal-backend-analysis/ghidra-findings.md`:

- `g_theGxDevicePtr` = `0x00c5df88`
- `GxDevCreate` = `0x00681290`
- OpenGL vtable = `PTR_FUN_00a2e198`
- D3D vtable = `PTR_FUN_00a2e718`

Minimum hooks to reach Metal:

- `DeviceCreate` (vtable +0x28)
- `DeviceDestroy` (vtable +0x2c)
- `Draw` (vtable +0xa8)
- `ScenePresent` (vtable +0x98)
- `SceneClear` (vtable +0x9c)
- `BufLock/BufUnlock/BufData` (vtable +0xd8/+0xdc/+0xe0)
- `TexCreate/TexDestroy` (vtable +0xe4/+0xe8)
- `ShaderConstantsSet` (vtable +0x118)
- `IRsSendToHw` or `GxRsSet` (render-state changes)

## Data flow and ownership

- The 32-bit DLL never calls Metal APIs.
- The 64-bit helper owns all Metal resources:
  - buffers, textures, pipeline state, command buffers
- The 32-bit DLL assigns resource IDs and sends them over IPC.

Suggested IPC transports (choose one):

- Shared memory ring buffer + event/pipe signal (low overhead).
- Named pipe socket (simpler, slightly higher overhead).

## Frame boundaries

- `ScenePresent` marks end-of-frame; it flushes the pending command list.
- `SceneClear` and `XformSetProjection/View` can be recorded as commands.

## Render-state tracking

GX state is stored by the game in an array of `EGxRenderState` values.
The proxy can mirror the same state on the 64-bit side:

- Prefer to send only deltas per frame.
- Keep a small cache so `Draw` can read current state quickly.

## Missing/optional hooks

These can be added after the first render path works:

- `TexCopy`/blit paths
- Readback (`SceneReadPixels` in OpenGL)
- Cursor/overlay rendering

## Next draft files

- `gx_hook_protocol.h`: IPC command types and packed data structs.
- `gx_hook_stub.cpp`: DLL entry point + vtable hook placeholders.
