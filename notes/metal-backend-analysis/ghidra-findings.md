# Ghidra findings (3.3.5a client)

These addresses come from the current Ghidra project. Names are inferred from behavior.

## Global device pointer

- `DAT_00c5df88` = `g_theGxDevicePtr` (global device singleton)
  - Used by wrappers and logging (e.g. `FUN_00682200`).

## Device creation path

- `FUN_00681290` = `GxDevCreate(api, windowProc, format)`
  - `api == 0` -> `FUN_0068bf20` (OpenGL device ctor)
  - `api == 1` -> `FUN_00689ef0` (D3D9 device ctor)
  - `api == 2` -> `FUN_0068c220` (D3D9Ex device ctor)
  - Calls `(*g_theGxDevicePtr->vtable[0x28/4])(windowProc, format)` (DeviceCreate)

- `FUN_0068fd50` = D3D9 device constructor
  - Sets vtable to `PTR_FUN_00a2e718` (CGxDeviceD3d vtable)

## gxApi selection and config

GX backend selection is driven by the `gxApi` cvar and a string table. The
selection logic shows that only indices 0..2 are actually supported.

- `FUN_0076a630` = `ConsoleDeviceRegisterVars()`
  - Registers `gxApi` cvar with callback `FUN_007693b0`
  - Default comes from `FUN_008c8de0()` (returns `1`)
- `FUN_0076ab80` = `ConsoleDeviceInitialize(...)`
  - Reads `gxApi` string, compares against `PTR_s_OpenGL_00ad87e4[]`
  - Accepts only indices `0,1,2` (via `FUN_006811d0`)
  - Calls `FUN_00681290(api, ...)` to create the device
- `FUN_006811d0(api)` returns `1` only for `api == 0 || 1 || 2`
- `FUN_008c8de0()` returns `1` (default API index = 1)
- `FUN_007693b0` is the `gxApi` cvar setter callback
  - If value matches a supported entry in `PTR_s_OpenGL_00ad87e4`, logs
    `"GxApi set pending gxRestart"` and returns `1` (restart required)
  - Otherwise prints `unsupported api, must be one of ...`

### gxApi string table (PTR_s_OpenGL_00ad87e4)

This is a 6-entry pointer table used by `gxApi` parsing and UI. Confirmed
entries and their pointer locations:

- `[0]` `0x00ad87e4` -> `0x00a2d560` = `"OpenGL"`
- `[1]` `0x00ad87e8` -> `0x00a2d558` = `"D3D9"`
- `[2]` `0x00ad87ec` -> `0x00a2d550` = `"D3D9Ex"`
- `[3]` `0x00ad87f0` -> `0x00a2d548` = `"D3D10"`
- `[4]` `0x00ad87f4` -> `0x00a2d540` = `"D3D11"`
- `[5]` `0x00ad87f8` -> `0x00a2d53c` = `"GLL"`

Likely supported values are indices 0..2 (OpenGL / D3D9 / D3D9Ex), but the
string at `0x00a2d558` needs confirmation in Ghidra.

## D3D library load

- `FUN_0068ed80` = `CGxDeviceD3d::ILoadD3dLib()`
  - `LoadLibraryA("d3d9.dll")` + `GetProcAddress("Direct3DCreate9")`
- `FUN_006a0aa0` = `CGxDeviceD3d9Ex::ILoadD3dLib()`

## Window class + window creation

- `FUN_0068eb20` = register `GxWindowClassD3d`
- `FUN_0068ebb0` = D3D device window creation (CreateWindowExA)
- `FUN_0068da10` = register `GxWindowClassOpenGl`
- `FUN_0068ccb0` = OpenGL device window creation (CreateWindowExA)

## CGxDeviceD3d vtable (PTR_FUN_00a2e718)

Key entries (offset -> function -> inferred meaning):

- `+0x28` -> `FUN_00690750` : `DeviceCreate`
- `+0x58` -> `FUN_00690230` : `DeviceWM` (window messages)
- `+0x98` -> `FUN_006a3450` : `ScenePresent`
- `+0x9c` -> `FUN_006a74b0` : `SceneClear`
- `+0xa0` -> `FUN_006a9b40` : `XformSetProjection`
- `+0xa4` -> `FUN_006a9e00` : `XformSetView`
- `+0xa8` -> `FUN_006a3620` : `Draw(CGxBatch*, indexed)`
- `+0xcc` -> `FUN_00685eb0` : `MasterEnableSet(EGxMasterEnables, int)`
- `+0xe4` -> `FUN_00685c60` : `TexCreate(...)`
- `+0xe8` -> `FUN_006a2bb0` : `TexDestroy(CGxTex*)`
- `+0xec` -> `FUN_006a30d0` : texture blit/copy (uses device->StretchRect-style path)
- `+0xf0` -> `FUN_006a31e0` : texture blit/copy (alternate path)

There are many more entries in this vtable; above are the ones most relevant
for a Gx-layer hook.

## Useful wrapper functions (call through g_theGxDevicePtr)

- `FUN_006813b0` -> vtable `+0x9c` (SceneClear wrapper)
- `FUN_00682a00` -> vtable `+0x98` (ScenePresent wrapper)
- `FUN_00681d90` -> vtable `+0xe4` (TexCreate wrapper + caps checks)
- `FUN_00681470` -> vtable `+0xe8` (TexDestroy wrapper)
- `FUN_00682340` -> vtable `+0xa8` (Draw wrapper that builds a CGxBatch)
- `FUN_00681430` -> `CGxDevice::RsSet` (render state set wrapper)

## GxInfo + format enumeration helpers

- `FUN_00682200` = `GxInfoDump(outBuf, maxLen)`
  - Prints `GxApi: %s` then calls vtable `+0x94`
    - OpenGL: `FUN_0068a8e0` (vendor/renderer/version)
    - D3D: `FUN_0068e570` (adapter/driver/version)
- `FUN_0054f1b0` = format enumeration/cache for the active gxApi
  - Reads `gxApi` cvar and maps to index (same logic as init)
  - Calls `FUN_00682b00(api, &outList)` and builds a unique list of
    `(colorBits, depthBits, refresh)` into `DAT_00bea748`/`DAT_00bea744`
  - Appears to back video options population

## Notes

- `GxDevCreate` is the most direct hook point if you want to replace the
  device object (swap vtable) and intercept Draw/Present/TexCreate.
- For vtable swapping, you can build a proxy CGxDevice-like struct and replace
  `DAT_00c5df88` after `FUN_00681290` or inline-hook `FUN_00681290` itself.

## GX enum order verification (binary vs source)

Binary table sizes match the open-source enum order in `src/gx/Types.hpp`:

- `EGxApi` (string table `PTR_s_OpenGL_00ad87e4`)
  - 0: OpenGL, 1: D3D9, 2: D3D9Ex, 3: D3D10, 4: D3D11, 5: GLL
  - Matches `GxApi_OpenGl..GxApi_GLL` (Metal is not present in 3.3.5a)
- `EGxBlend` (`DAT_00ad8d90`/`DAT_00ad8dc0`) has 12 entries = `GxBlends_Last`
- `EGxPrim` (`DAT_00ad8eac`) has 6 entries = `GxPrims_Last`
- `EGxTexFilter` (`DAT_00a2f1f8`/`DAT_00a2f1e0`) has 6 entries
- `EGxTexTarget` (`DAT_00a2e05c`) has 4 entries

`EGxTexFormat` has 13 values in source, but the internal format table
`DAT_00a2f210` in the binary has 18 entries, implying additional/internal
format IDs in the compiled client. Treat `DAT_00a2f210` as the source of truth
for texture internal formats when mapping GX->Metal.

## GX struct offsets (binary-derived)

These offsets are inferred from the 3.3.5a binary (Ghidra) and should be used
for hooking/marshaling. Open-source definitions are mostly consistent but may
omit padding/unknown fields.

## CGxDevice base ctor + shader constants

### Base ctor (`FUN_00688690`)

- Sets base vtable to `PTR_FUN_00a2ddc0` and zeroes a large shared state block.
- Initializes various default float values (1.0f) across common state arrays.
- Initializes two global shader constant arrays and their dirty ranges:
  - `DAT_00c5dfe0` and `DAT_00c5efe8` are filled with `0x7f7fffff` (NaN) for
    `0x400` floats each. This implies **256 float4 registers per stage**.
  - Dirty range globals are set:
    - `DAT_00c5ffe8` (max for stage 0) = 0
    - `DAT_00c5ffec` (min for stage 0) = 0xff
    - `DAT_00c5efe0` (max for stage 1) = 0
    - `DAT_00c5efe4` (min for stage 1) = 0xff

### ShaderConstantsSet (OpenGL/D3D)

Both devices share the same constant storage layout (global arrays):

- `FUN_0069e970` (OpenGL vtable +0x118) and `FUN_006833e0` (D3D vtable +0x118)
  write into the global arrays:
  - Stage 0: `DAT_00c5efe8 + (index * 4)`
  - Stage 1: `DAT_00c5dfe0 + (index * 4)`
- Each constant is a float4; `param_2` is the start register, `param_4` is the
  number of float4 registers.
- Dirty range tracking:
  - Stage 0 updates `DAT_00c5ffec` (min) and `DAT_00c5ffe8` (max).
  - Stage 1 updates `DAT_00c5efe4` (min) and `DAT_00c5efe0` (max).
- The D3D path compares values before writing (per-register diff), while the
  OpenGL path does a tight memcpy-style copy for stage 1 (vectorized loop).

## CGxDevice method signatures (binary-derived)

These signatures were verified via decompilation in the 3.3.5a binary and
match the open-source headers. Use them for hook prototypes.

### Draw (OpenGL + D3D)

- `FUN_0069e560` (OpenGL vtable +0xa8)
- `FUN_006a3620` (D3D vtable +0xa8)

Signature:

- `void __thiscall Draw(CGxBatch* batch, int indexed)`

CGxBatch layout matches `src/gx/CGxBatch.hpp`:

- `m_primType` @ +0x00
- `m_start` @ +0x04
- `m_count` @ +0x08
- `m_minIndex` @ +0x0c
- `m_maxIndex` @ +0x0e

### Buffer update path (OpenGL)

- `FUN_0068b4d0` (vtable +0xd8) = `BufLock`
- `FUN_0068bdd0` (vtable +0xdc) = `BufUnlock`
- `FUN_0068b670` (vtable +0xe0) = `BufData`

Signatures (types inferred from usage and open-source):

- `char* __thiscall BufLock(CGxBuf* buf)`
- `int32_t __thiscall BufUnlock(CGxBuf* buf, uint32_t flags)`
- `void __thiscall BufData(CGxBuf* buf, const void* data, size_t size, uintptr_t offset)`

### Texture creation (OpenGL)

- `FUN_00685c60` (vtable +0xe4) = `TexCreate`

Signature:

- `int32_t __thiscall TexCreate(EGxTexTarget target, uint32_t w, uint32_t h, uint32_t d,`
- `  EGxTexFormat format, EGxTexFormat dataFormat, CGxTexFlags flags, void* userArg,`
- `  void (*userFunc)(EGxTexCommand, uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t&, const void*&),`
- `  const char* name, CGxTex*& outTex)`

Note: the binary version does not reference `name` directly inside
`FUN_00685c60`.

### Shader constants (OpenGL + D3D)

- `FUN_0069e970` (OpenGL vtable +0x118)
- `FUN_006833e0` (D3D vtable +0x118)

Signature:

- `void ShaderConstantsSet(EGxShTarget target, uint32_t startVec4, const float* data, uint32_t vec4Count)`

## Render-state system (RsSet + IRsSendToHw)

### RsSet wrapper + dirty tracking

- `FUN_00681430` is a thin wrapper over `FUN_00685cd0(this, statePtr, value)`.
- `FUN_00685cd0` expects `statePtr` to be a **state object** with:
  - `statePtr + 0x2c` = stored value (DWORD)
  - `statePtr + 0x5c` = dirty flag (byte)
  - `statePtr + 0x5d` = secondary dirty flag (byte)
- On change, it checks the state table at `this+0x2900` for entries whose
  first field matches `statePtr`, and if found triggers `FUN_006859e0(this, idx)`.
  - The loop walks entries `idx=0x15..0x24` (offsets `0x150..0x240`, stride 0x10).

### State array layout and dirty list (`FUN_006859e0`)

- State array pointer is at `this+0x28f4`; each entry is **0x18 bytes**.
  - Entry layout (observed):
    - `+0x00..0x0c` = 4 DWORDs used as raw values (float/uint).
    - `+0x14` = dirty marker (set to `1` on change).
  - This is the same array used by both OpenGL and D3D render-state uploads.
- Dirty list bookkeeping:
  - `this+0x28` = dirty count
  - `this+0x2c` = pointer to list of dirty state indices
  - `FUN_00685180(this+0x24, count+1)` grows the list if needed.
- Shadow/cache array:
  - `this+0x2900` points to an array of **0x10 bytes per state**.
  - `FUN_006859e0` stores bitwise `~value` copies of the first 4 DWORDs from
    the state array into this shadow entry to force future updates.

### OpenGL IRsSendToHw (`FUN_0069cdc0`)

- `pfVar1 = (float *)(*(int *)(this+0x28f4) + stateIndex * 0x18)`.
- Key cached fields in device:
  - `this+0x3a90` polygon offset enable
  - `this+0x3a94` polygon offset factor
  - `this+0x3abc` blend func cache (packed)
  - `this+0x3a9c` alpha test enable
  - `this+0x3968` depth mask
  - `this+0x396c` color mask
  - `this+0x3acc` clip plane mask
  - `this+0x2758` master enable mask (filters state changes)
- Notable state indices (by behavior; numeric names TBD):
  - 0: polygon offset (GL_POLYGON_OFFSET_FILL + glPolygonOffset)
  - 4: material shininess (glMaterialf)
  - 5: GL_NORMALIZE enable
  - 6: blend enable + blend func via `DAT_00ad8d90`/`DAT_00ad8dc0`
  - 7: alpha test (glAlphaFunc/GL_ALPHA_TEST)
  - 8–10: fog start/end/color (glFog*)
  - 11: lighting enable (GL_LIGHTING)
  - 12: fog enable (GL_FOG)
  - 13: depth test (GL_DEPTH_TEST)
  - 14: depth func (`DAT_00ad8e80`)
  - 15: depth mask
  - 16: color mask
  - 17: cull face + front face
  - 18: clip planes (GL_CLIP_PLANE0..5)
  - 19: multisample toggle (GL_MULTISAMPLE, if supported)
  - 20: scissor test (GL_SCISSOR_TEST)
  - 0x15–0x24: `FUN_006923a0` (state group A)
  - 0x25–0x2c: `FUN_00693020` (state group B)
  - 0x2d–0x34: `FUN_00693730` (state group C)
  - 0x35–0x3c: `FUN_00695fd0` (state group D)
  - 0x3d/0x3e: `FUN_0069f8d0` / `FUN_0069f760` (single-state helpers)
  - 0x3f: glPointSize
  - 0x40–0x42: extension hooks via `DAT_00c6054c/50`
  - 0x43: program toggles (0x8861/0x8642) if extension present
  - 0x44: extension color (DAT_00c60518)
  - 0x45: glColorMaterial (via `DAT_00ad8e90`)

### D3D IRsSendToHw (`FUN_006a4c30`)

- Same state array (`this+0x28f4`, stride 0x18), but pushes values to
  `IDirect3DDevice9::SetRenderState` via `this+0x397c` (vtable +0xe4).
- Uses mapping tables:
  - `DAT_00a2f964` / `DAT_00a2f994` (depth func / depth write)
  - `DAT_00a2fa14` (cull mode)
  - `DAT_00a2fa24` (blend op)
- Cache fields (exact meaning pending):
  - `this+0x3bc4/0x3bc8` for depth func/write
  - `this+0x3e60` (blend enable)
  - `this+0x3e64` (alpha test enable)
  - `this+0x3e68` (alpha ref)
  - `this+0x3e6c` (fog enable)
  - `this+0x3e70` (depth write enable)
  - `this+0x3e74` (color write mask)
  - `this+0x3e88` (cull mode)
  - `this+0x3e98` (special toggle for state 0x50)
  - `this+0x3e7c` (state derived from states 1–4)

## Remaining render states (GL path)

These are the post-texture/combiner states (indices 77–85) handled by the
OpenGL backend in `FUN_0069cdc0`.

### `GxRs_VertexShader` (index 77 / 0x4d) — `FUN_0069f8d0`

- Binds a vertex program based on `this+0x2c8` (shader target/profile):
  - `6/7` -> ARB vertex program path (calls `DAT_00c605d0`).
  - `8/9` -> NV vertex program path (calls `DAT_00c605a4`).
- Uses `*(param_1 + 0x20)` as program id (likely `CGxShader::m_apiSpecific`).
- If shader not created (`param_1+0x30 == 0`) calls `FUN_0069ec10` (compile/create).
- `param_1 == 0` disables the program by calling `FUN_006918f0` with state ids
  `0x52` or `0x53` depending on profile.
- `0x8620` constant passed to GL call = `GL_VERTEX_PROGRAM_ARB`.

### `GxRs_PixelShader` (index 78 / 0x4e) — `FUN_0069f760`

- Binds a fragment/combiner program based on `this+0x2d8` (pixel profile):
  - `7` -> NV register combiner path (`FUN_0069e6e0` + `FUN_006918f0(0x4e/0x4f)`).
  - `8/9/10` -> NV texture shader paths (`FUN_0069e6e0`, `FUN_0069e890`, plus 0x4e/0x4f/0x50).
  - `12` -> ARB fragment program (`DAT_00c605d0` with `0x8804`).
- `param_1+0x50` is used as program pointer (NV/combiner path).
- `param_1 == 0` disables relevant states (0x4e/0x50/0x51 depending on profile).
- `0x8804` constant passed to GL call = `GL_FRAGMENT_PROGRAM_ARB`.

### Point/point-sprite states

These map directly to GL point/point-sprite parameters:

- `GxRs_PointScale` (79 / 0x4f): `glPointSize(*pfVar1)`
- `GxRs_PointScaleAttenuation` (80 / 0x50):
  - `DAT_00c6054c(0x8129, pfVar1)` if extension (`GL_POINT_DISTANCE_ATTENUATION`)
- `GxRs_PointScaleMin` (81 / 0x51):
  - `DAT_00c60550(0x8126, *pfVar1)` (`GL_POINT_SIZE_MIN`)
- `GxRs_PointScaleMax` (82 / 0x52):
  - `DAT_00c60550(0x8127, *pfVar1)` (`GL_POINT_SIZE_MAX`)
- `GxRs_PointSprite` (83 / 0x53):
  - Enables/disables `GL_POINT_SPRITE` (`0x8861`) and coord-replace (`0x8642`)
    when `DAT_00c604c8` is available.
- `GxRs_Unk84` (84 / 0x54):
  - Calls `DAT_00c60518(f,f,f,f)` if `DAT_00c604a0` is available.
  - Likely a point-sprite/extension parameter (exact GL enum unresolved).

### `GxRs_ColorMaterial` (85 / 0x55)

- `glColorMaterial(GL_FRONT_AND_BACK, mapped)` using `DAT_00ad8e90`.

## Unk61..Unk76 status (D3D path)

- `GxRs_Unk61..GxRs_Unk76` (indices 61–76) are **not explicitly handled** in
  the OpenGL `IRsSendToHw` switch, but are used in the D3D path.

### `GxRs_Unk61..Unk68` (indices 61–68) — texture transform mode

- Handled by `FUN_006a5aa0` (called when texture stages are updated).
- Reads the state value from:
  - `*(stateArray + (stage * 3 + 0xb7) * 8)` → index 61 + stage.
- Uploads a texture transform matrix via:
  - `SetTransform(D3DTS_TEXTURE0 + stage, matrix)` (vtable +0xb0).
- Sets texture transform flags via:
  - `FUN_006a3c40(this, stage + 0x52, flags)` → `SetTextureStageState`
    `D3DTSS_TEXTURETRANSFORMFLAGS` (flags observed: `2`, `3`, `0x103`).
- Behavior by value:
  - `0`: use per-stage texture matrix; flags usually `3` (COUNT3).
  - `1`: use combined matrix; flags `2` or `3` depending on TexGen state.
  - `2`: use combined matrix; flags `0x103` (COUNT3 | PROJECTED).

### `GxRs_Unk69..Unk76` (indices 69–76) — texture coordinate index

- Handled by `FUN_006a4ac0` → `FUN_006a4100`.
- `FUN_006a4100` sets `D3DTSS_TEXCOORDINDEX` (stage + 0x6a) based on TexGen:
  - `TexGen == 0`: use `GxRs_Unk69..Unk76` value (default 0..7).
  - `TexGen == 1/2/3`: stage | `0x20000` (camera-space position).
  - `TexGen == 4/6`: stage | `0x30000` (camera-space reflection vector).
  - `TexGen == 5`: stage | `0x10000` (camera-space normal).

### Color/alpha op tables (D3D path)

- `GxRs_ColorOpN` → `FUN_006a4190`:
  - `D3DTSS_COLOROP` = `DAT_00a2f9cc[param]`
  - `D3DTSS_COLORARG1` = `DAT_00a2f9e4[param]`
  - `D3DTSS_COLORARG2` = `DAT_00a2f9e8[param]`
- `GxRs_AlphaOpN` → `FUN_006a41f0`:
  - `D3DTSS_ALPHAOP` = `DAT_00a2f9cc[param]`
  - `D3DTSS_ALPHAARG1` = `DAT_00a2f9e4[param]`
  - `D3DTSS_ALPHAARG2` = `DAT_00a2f9e8[param]`
- These tables should be extracted to complete the fixed-function mapping.

## Sampler state mapping (D3D path, from `FUN_006a4900`)

When binding a texture (`GxRs_TextureN`), the D3D path derives sampler state
from `CGxTex->m_flags` (offset `+0x2c`):

- `flags & 0x7` → **filter preset index** (0..7):
  - `DAT_00ad8f40[ preset ]` → `D3DSAMP_MINFILTER`
  - `DAT_00ad8f44[ preset ]` → `D3DSAMP_MAGFILTER`
  - `DAT_00ad8f48[ preset ]` → `D3DSAMP_MIPFILTER`
- `(flags >> 3) & 1` → address U selector:
  - `DAT_00a2f9c4[ bit ]` → `D3DSAMP_ADDRESSU`
- `(flags >> 4) & 1` → address V selector:
  - `DAT_00a2f9c4[ bit ]` → `D3DSAMP_ADDRESSV`
- `(flags >> 9) & 0x1f` → `D3DSAMP_MAXANISOTROPY`

These are set via `FUN_006a3c40` with `param_1` offsets:

- `stage + 0x12` → `D3DSAMP_MINFILTER`
- `stage + 0x02` → `D3DSAMP_MAGFILTER`
- `stage + 0x22` → `D3DSAMP_MIPFILTER`
- `stage + 0x32` → `D3DSAMP_ADDRESSU`
- `stage + 0x42` → `D3DSAMP_ADDRESSV`
- `stage + 0x5a` → `D3DSAMP_MAXANISOTROPY`

### `DAT_00ad8f40` (filter presets, 24 DWORDs)

Raw DWORDs (as extracted):

```
{
  0x00000001, 0x00000001, 0x00000000,
  0x00000002, 0x00000002, 0x00000000,
  0x00000001, 0x00000001, 0x00000001,
  0x00000002, 0x00000002, 0x00000001,
  0x00000002, 0x00000002, 0x00000002,
  0x00000002, 0x00000002, 0x00000002,
  0x00000005, 0x00000000, 0x00000001,
  0x00000002, 0x00000003, 0x00000004
}
```

Grouped by preset (min, mag, mip):

```
0: [1, 1, 0]
1: [2, 2, 0]
2: [1, 1, 1]
3: [2, 2, 1]
4: [2, 2, 2]
5: [2, 2, 2]
6: [5, 0, 1]
7: [2, 3, 4]
```

### `DAT_00a2f9c4` (wrap modes, 2 DWORDs)

```
{ 0x00000003, 0x00000001 }
```

### `DAT_00a2f9cc` (color/alpha op table, 6 DWORDs)

```
{ 0x00000004, 0x00000005, 0x00000007, 0x00000003, 0x00000010, 0x0000000c }
```

### `DAT_00a2f9e4` / `DAT_00a2f9e8` (color/alpha arg tables, 12 DWORDs)

Raw DWORDs (as extracted, interleaved arg1/arg2 pairs):

```
{
  0x00000002, 0x00000001,
  0x00000002, 0x00000001,
  0x00000002, 0x00000001,
  0x00000002, 0x00000001,
  0x00000001, 0x00000002,
  0x00000002, 0x00000001
}
```

Interpreted as pairs per op index:

```
0: [2, 1]
1: [2, 1]
2: [2, 1]
3: [2, 1]
4: [1, 2]
5: [2, 1]
```
### State group A (`FUN_006923a0`) — texture unit binding + texgen/arrays

- `param_1` is the **texture stage index** (0..15) passed from
  `GxRs_Texture0..GxRs_Texture15`.
  - Call site: `FUN_0069cdc0` dispatches `GxRs_TextureN` (0x15..0x24) to
    `FUN_006923a0(this, which - 0x15, texPtr)`.
- `param_2` is a `CGxTex*`.
  - It reads `param_2[8]` (offset +0x20) as `EGxTexTarget`.
  - Uses `ARRAY_00a2e05c` to map target -> GL texture target.
- Texture unit handling:
  - `this+0x214` = number of texture units.
  - `this+0x3970` = current active unit; if changed, calls `DAT_00c60600` and
    `DAT_00c605fc` (likely `glActiveTexture` + `glClientActiveTexture`).
- Cache slot for this group: `this + 0x3968 + (param_1 + 3) * 4`.
- `param_2 == NULL` path disables the GL state for this sub-state (glDisable,
  glDisableClientState, glTexEnvi resets, etc.).
- `param_2 != NULL` path:
  - Enables/disables GL texture target, texgen, client arrays.
  - Updates texture env combine via `glTexEnvi/glTexEnvf/glTexEnvfv`.
  - Uses extension flags: `DAT_00c604e0` (combine), `DAT_00c6048c/88/84/e8/ec`.
  - Calls `FUN_0069d5d0(this, param_2, param_1)` and
    `FUN_0069de60(this, param_2, param_1)` (bind + upload/update texture).

### State group B (`FUN_00693020`) — additional texenv/array slots

- `param_1` is the **texture stage index** (0..7) passed from
  `GxRs_ColorOp0..GxRs_ColorOp7`.
  - Call site: `FUN_0069cdc0` dispatches `GxRs_ColorOpN` (0x25..0x2c) to
    `FUN_00693020(this, which - 0x25, value)`.
- Same unit switching logic as group A (`this+0x214` / `this+0x3970`).
- Cache slot: `this + 0x3968 + (param_1 + 0x2b) * 4`.
- Primarily manipulates `glTexEnv*`, `glTexGeni`, and client state for a second
  bank of per-stage parameters. Uses the same extension flags as group A.

### State group C (`FUN_00693730`) — additional texenv/array slots

- `param_1` is the **texture stage index** (0..7) passed from
  `GxRs_AlphaOp0..GxRs_AlphaOp7`.
  - Call site: `FUN_0069cdc0` dispatches `GxRs_AlphaOpN` (0x2d..0x34) to
    `FUN_00693730(this, which - 0x2d, value)`.
- Same unit switching logic as group A.
- Cache slot: `this + 0x3968 + (param_1 + 0x33) * 4`.
- Similar responsibilities to group B (texenv/texgen/client arrays) but for a
  third bank of per-stage parameters.

### State group D (`FUN_00695fd0`) — combiner/const-color updates

- `param_1` is the **texture stage index** (0..7) passed from
  `GxRs_TexGen0..GxRs_TexGen7`.
  - Call site: `FUN_0069cdc0` dispatches `GxRs_TexGenN` (0x35..0x3c) to
    `FUN_00695fd0(this, which - 0x35, value)`.
- Multi-mode based on `param_2` (value of GxRs_TexGenN):
  - `param_2 == 0`: disable/reset the sub-state.
  - `param_2 == 1`: enable/apply the sub-state.
  - `param_2 == 2`: triggers combiner update path.
- Uses multiple per-stage value arrays:
  - `this+0x3994`, `this+0x39d4`, `this+0x39f4` (values).
  - Cache slots in `this+0x3968` with offsets `+0xb`, `+0x13`, `+0x1b`, `+0x23`.
- Calls the same GL state setters as groups A–C for enables/texenv/texgen.
- Combiner update path (`param_2 == 2`):
  - Operates on block at `this + 0x1c10 + param_1 * 0x118`.
  - Calls `FUN_0069e410` and `FUN_00407f80` (likely rebuilds ARB/combiner state).
  - `param_2 != 2` falls back to `FUN_0057c340` (reset/clear).

### `CGxTex` (observed offsets)

- `+0x14` width
- `+0x18` height
- `+0x1c` depth
- `+0x20` target (`EGxTexTarget`)
- `+0x24` format (`EGxTexFormat`)
- `+0x28` dataFormat (`EGxTexFormat`)
- `+0x2c` flags (`CGxTexFlags` bitfield)
- `+0x30` userArg
- `+0x34` userFunc (callback)
- `+0x38` apiSpecificData (GL texture id)
- `+0x3c` apiSpecificData2 / aux pointer
- `+0x5a`/`+0x5b`/`+0x5c` dirty/needsUpdate flags (byte fields)

### `CGxBuf` (observed offsets)

- `+0x08` pool pointer (`CGxPool*`)
- `+0x0c` itemSize
- `+0x10` itemCount
- `+0x14` size (bytes)
- `+0x18` index/offset into pool storage
- `+0x1c`/`+0x1d` flag bytes (`mapped` / dirty markers)

### `CGxPool` (observed offsets)

- `+0x08` target (`EGxPoolTarget`)
- `+0x0c` usage (`EGxPoolUsage`)
- `+0x10` size
- `+0x14` apiSpecific (VBO/IBO object)
- `+0x18` mem pointer (CPU staging)

### `CGxBatch` (observed offsets)

- `+0x00` prim type (`EGxPrim`)
- `+0x04` start
- `+0x08` count
- `+0x0c` minIndex/maxIndex packed (`uint16_t` each)

## CGxDeviceD3d vtable full map (PTR_FUN_00a2e718)

Offsets are from `PTR_FUN_00a2e718` (0x00a2e718). Function names are inferred
from behavior where possible; some are still unknown.

- `+0x00` 0x00a2e718 -> `FUN_006a3070` : texture mark/update path (CGxTex fields 0x5a/0x5b/0xe/0xf)
- `+0x04` 0x00a2e71c -> `FUN_006a4c30` : render-state upload to D3D device (big switch)
- `+0x08` 0x00a2e720 -> `FUN_0068e900` : cursor surface/texture setup
- `+0x0c` 0x00a2e724 -> `FUN_006a00c0` : cursor surface/texture teardown
- `+0x10` 0x00a2e728 -> `FUN_0068e810` : per-frame device update (scene begin-ish)
- `+0x14` 0x00a2e72c -> `FUN_006843b0` : invoke callback list @0x18c
- `+0x18` 0x00a2e730 -> `FUN_006843e0` : invoke callback list @0x19c
- `+0x1c` 0x00a2e734 -> `FUN_00684410` : invoke callback list @0x1ac
- `+0x20` 0x00a2e738 -> `FUN_0068fe80` : D3D device dtor
- `+0x24` 0x00a2e73c -> `FUN_00690830` : DeviceCreate (from existing HWND)
- `+0x28` 0x00a2e740 -> `FUN_00690750` : DeviceCreate (creates window class + hwnd)
- `+0x2c` 0x00a2e744 -> `FUN_006905f0` : DeviceDestroy/cleanup
- `+0x30` 0x00a2e748 -> `FUN_0068e450` : unknown (no function body)
- `+0x30` 0x00a2e748 -> `FUN_0068e450` : `EvictManagedResources()` wrapper + error check
- `+0x34` 0x00a2e74c -> `FUN_006904d0` : DeviceSetFormat (logs + recreate window)
- `+0x38` 0x00a2e750 -> `FUN_0068e4a0` : unknown (calls FUN_006a2aa0 + FUN_00682d00)
- `+0x3c` 0x00a2e754 -> `FUN_0069fe80` : gamma/viewport update helper (uses 0x398c)
- `+0x40` 0x00a2e758 -> `FUN_0068e4c0` : gamma/viewport update helper (uses 0x398c)
- `+0x44` 0x00a2e75c -> `FUN_0069fed0` : unknown (no function body)
- `+0x44` 0x00a2e75c -> `FUN_0069fed0` : `DeviceWindow()` getter (returns HWND @0x3968)
- `+0x48` 0x00a2e760 -> `FUN_00682d30` : unknown (no function body)
- `+0x48` 0x00a2e760 -> `FUN_00682d30` : mark device dirty flag (@0x2934)
- `+0x4c` 0x00a2e764 -> `FUN_00684260` : get window rect/size (returns fields @0x2938)
- `+0x50` 0x00a2e768 -> `FUN_0068fed0` : readback surface to CPU buffer (format swizzle)
- `+0x54` 0x00a2e76c -> `FUN_006a1950` : resize/clear CPU buffer backing store
- `+0x58` 0x00a2e770 -> `FUN_00690230` : DeviceWM (window messages)
- `+0x5c` 0x00a2e774 -> `FUN_0068f770` : set texture stage / bind + set viewport
- `+0x60` 0x00a2e778 -> `FUN_0068e510` : unknown (no function body)
- `+0x60` 0x00a2e778 -> `FUN_0068e510` : `GetRenderTarget` into `DAT_00c6033c` (AddRef)
- `+0x64` 0x00a2e77c -> `FUN_0068e540` : unknown (no function body)
- `+0x64` 0x00a2e77c -> `FUN_0068e540` : `SetRenderTarget` from `DAT_00c6033c`
- `+0x68` 0x00a2e780 -> `FUN_0068f900` : bind texture to D3D device
- `+0x6c` 0x00a2e784 -> `FUN_0068f950` : blit between textures (StretchRect-like)
- `+0x70` 0x00a2e788 -> `FUN_0069ff40` : render-state/flag update (param_1==0/7)
- `+0x74` 0x00a2e78c -> `FUN_006853b0` : invoke callback list @0x18c (alt)
- `+0x78` 0x00a2e790 -> `FUN_006853d0` : remove from callback list @0x18c
- `+0x7c` 0x00a2e794 -> `FUN_00685460` : invoke callback list @0x194
- `+0x80` 0x00a2e798 -> `FUN_00685480` : remove from callback list @0x194
- `+0x84` 0x00a2e79c -> `FUN_00685510` : invoke callback list @0x1a4
- `+0x88` 0x00a2e7a0 -> `FUN_00685530` : remove from callback list @0x1a4
- `+0x8c` 0x00a2e7a4 -> `FUN_006a5a00` : get rect (calls FUN_00682d70)
- `+0x90` 0x00a2e7a8 -> `FUN_006a9920` : get window rect (screen coords if windowed)
- `+0x94` 0x00a2e7ac -> `FUN_0068e570` : adapter info logging (Driver/Version/Desc)
- `+0x98` 0x00a2e7b0 -> `FUN_006a3450` : ScenePresent
- `+0x9c` 0x00a2e7b4 -> `FUN_006a74b0` : SceneClear
- `+0xa0` 0x00a2e7b8 -> `FUN_006a9b40` : XformSetProjection
- `+0xa4` 0x00a2e7bc -> `FUN_006a9e00` : XformSetView
- `+0xa8` 0x00a2e7c0 -> `FUN_006a3620` : Draw(CGxBatch*, indexed)
- `+0xac` 0x00a2e7c4 -> `FUN_006855c0` : reset dynamic draw buffer state
- `+0xb0` 0x00a2e7c8 -> `FUN_006845b0` : submit dynamic geometry (calls GxDrawLockedElements path)
- `+0xb4` 0x00a2e7cc -> `FUN_00685640` : push dynamic vertex data (marks dirty)
- `+0xb8` 0x00a2e7d0 -> `FUN_00682fa0` : set 2-float state + mark dirty bit
- `+0xbc` 0x00a2e7d4 -> `FUN_00682f70` : set 3-float state + mark dirty bit
- `+0xc0` 0x00a2e7d8 -> `FUN_00684590` : set scalar state + mark dirty bit
- `+0xc4` 0x00a2e7dc -> `FUN_00632050` : no-op
- `+0xc8` 0x00a2e7e0 -> `FUN_00632050` : no-op
- `+0xcc` 0x00a2e7e4 -> `FUN_00685eb0` : MasterEnableSet(EGxMasterEnables)
- `+0xd0` 0x00a2e7e8 -> `FUN_0069ff80` : texture state update (uses CGxTex, calls FUN_0069fb00/0068e180)
- `+0xd4` 0x00a2e7ec -> `FUN_0068e720` : texture object setup (calls FUN_0068e1f0/FUN_00688340)
- `+0xd8` 0x00a2e7f0 -> `FUN_0068fce0` : buffer lock/prepare (pairs with 0xdc/0xe0)
- `+0xdc` 0x00a2e7f4 -> `FUN_0068fae0` : buffer unlock/commit
- `+0xe0` 0x00a2e7f8 -> `FUN_0068fd00` : buffer data upload (memcpy into mapped buffer)
- `+0xe4` 0x00a2e7fc -> `FUN_00685c60` : TexCreate
- `+0xe8` 0x00a2e800 -> `FUN_006a2bb0` : TexDestroy
- `+0xec` 0x00a2e804 -> `FUN_006a30d0` : texture blit/copy (StretchRect path)
- `+0xf0` 0x00a2e808 -> `FUN_006a31e0` : texture blit/copy (alt path)
- `+0xf4` 0x00a2e80c -> `FUN_00632050` : no-op
- `+0xf8` 0x00a2e810 -> `FUN_0068e9c0` : create resource from type table (likely shader/buffer)
- `+0xfc` 0x00a2e814 -> `FUN_006a0190` : release resource from entry (pairs with 0xf8)
- `+0x100` 0x00a2e818 -> `FUN_0068ea10` : create/lock D3D resource if missing
- `+0x104` 0x00a2e81c -> `FUN_006a0240` : unlock/status for resource (returns bool)
- `+0x108` 0x00a2e820 -> `FUN_0068ea90` : resource query (priority/lock status)
- `+0x10c` 0x00a2e824 -> `FUN_006a0310` : resource query loop (GetData until done)
- `+0x110` 0x00a2e828 -> `FUN_006aa130` : texture upload path (calls FUN_006897c0)
- `+0x114` 0x00a2e82c -> `FUN_006aa190` : texture upload teardown
- `+0x118` 0x00a2e830 -> `FUN_006833e0` : ShaderConstantsSet (writes constant arrays + dirty range)
- `+0x11c` 0x00a2e834 -> `FUN_006a5d50` : texture create/upload helper (calls FUN_006aa0d0/006aa070)
- `+0x120` 0x00a2e838 -> `FUN_006a5e10` : texture upload apply helper
- `+0x124` 0x00a2e83c -> `FUN_0068e750` : cursor update (screen->client + SetCursor)
- `+0x128` 0x00a2e840 -> `FUN_00683650` : unknown (no function body)
- `+0x128` 0x00a2e840 -> `FUN_00683650` : returns pointer to state block @`this+0x2960`
- `+0x12c` 0x00a2e844 -> `FUN_0068e7e0` : mark device dirty (sets 0x3b4c)
- `+0x130` 0x00a2e848 -> `FUN_0068e980` : set pending param (0x3ac4)
- `+0x134` 0x00a2e84c -> `FUN_006a0110` : unknown (no function body)
- `+0x134` 0x00a2e84c -> `FUN_006a0110` : returns float @`this+0x3ac4`
- `+0x138` 0x00a2e850 -> `FUN_0068e9a0` : set pending param (0x3ac8)
- `+0x13c` 0x00a2e854 -> `FUN_006a0120` : unknown (no function body)
- `+0x13c` 0x00a2e854 -> `FUN_006a0120` : returns float @`this+0x3ac8`

## Key mapping tables seen in D3D render-state path

These show up in `FUN_006a4c30` (render-state upload):

- `DAT_00a2f964` / `DAT_00a2f994` : depth func / depth write mapping
- `DAT_00a2fa14` : cull mode mapping
- `DAT_00a2fa24` : blend mode mapping
- `DAT_00a2f850` : cube map face mapping (used in texture bind/blit paths)

### `DAT_00a2f964` contents

Extracted as 12 DWORDs (little endian):

```
{
  0x00000002, 0x00000002, 0x00000005, 0x00000005,
  0x00000009, 0x00000009, 0x00000009, 0x00000006,
  0x00000006, 0x00000005, 0x00000002, 0x0000000e
}
```

### `DAT_00a2f994` contents

Extracted as 12 DWORDs (little endian):

```
{
  0x00000001, 0x00000001, 0x00000006, 0x00000002,
  0x00000001, 0x00000003, 0x00000002, 0x00000002,
  0x00000001, 0x00000001, 0x00000002, 0x0000000f
}
```

### `DAT_00a2fa14` contents

Extracted as 4 DWORDs (little endian):

```
{
  0x00000004, 0x00000003, 0x00000007, 0x00000002
}
```

### `DAT_00a2fa24` contents

Extracted as 2 DWORDs (little endian):

```
{
  0x00000001, 0x00000002
}
```

### `DAT_00a2f850` contents

Extracted as 6 DWORDs (little endian):

```
{
  0x00000000, 0x00000001, 0x00000002,
  0x00000003, 0x00000004, 0x00000005
}
```

## CGxDeviceOpenGL vtable (PTR_FUN_00a2e198)

Offsets are from `PTR_FUN_00a2e198` (0x00a2e198). Names inferred from behavior
and matched to D3D vtable positions where possible.

- `+0x00` 0x00a2e198 -> `FUN_0069e120` : ITexMarkAsUpdated (calls FUN_0069de60)
- `+0x04` 0x00a2e19c -> `FUN_0069cdc0` : IRsSendToHw (render-state upload; uses `gl*`)
- `+0x08` 0x00a2e1a0 -> `FUN_00684ad0` : cursor texture setup (creates 0x20x20 texture)
- `+0x0c` 0x00a2e1a4 -> `FUN_006835e0` : cursor texture destroy
- `+0x10` 0x00a2e1a8 -> `FUN_00687a90` : overlay/cursor draw path (large GL state + quad draw)
- `+0x14` 0x00a2e1ac -> `FUN_006843b0` : invoke callback list @0x18c
- `+0x18` 0x00a2e1b0 -> `FUN_006843e0` : invoke callback list @0x19c
- `+0x1c` 0x00a2e1b4 -> `FUN_00684410` : invoke callback list @0x1ac
- `+0x20` 0x00a2e1b8 -> `FUN_0068bf50` : OpenGL device dtor
- `+0x24` 0x00a2e1bc -> `FUN_0068d610` : DeviceCreate (from existing window/context)
- `+0x28` 0x00a2e1c0 -> `FUN_0068daa0` : DeviceCreate (creates window/class)
- `+0x2c` 0x00a2e1c4 -> `FUN_0068cb40` : DeviceDestroy/cleanup
- `+0x30` 0x00a2e1c8 -> `FUN_005eeb70` : stub/assert (unused)
- `+0x34` 0x00a2e1cc -> `FUN_0068d6d0` : DeviceSetFormat (recreate window/context)
- `+0x38` 0x00a2e1d0 -> `FUN_0068cc00` : format/viewport helper (calls FUN_0069d820 + FUN_00682d00)
- `+0x3c` 0x00a2e1d4 -> `FUN_0068cc60` : gamma update (SetDeviceGammaRamp)
- `+0x40` 0x00a2e1d8 -> `FUN_0068cc20` : gamma update (SetDeviceGammaRamp)
- `+0x44` 0x00a2e1dc -> `FUN_0068cca0` : returns dword @`this+0x3aec` (likely HWND)
- `+0x48` 0x00a2e1e0 -> `FUN_00682d30` : mark dirty flag (@0x2934)
- `+0x4c` 0x00a2e1e4 -> `FUN_00684260` : get window rect/size
- `+0x50` 0x00a2e1e8 -> `FUN_0068bf80` : readback (glReadPixels + vertical flip)
- `+0x54` 0x00a2e1ec -> `FUN_0068c070` : readback (glReadPixels, buffer swap)
- `+0x58` 0x00a2e1f0 -> `FUN_0068d460` : DeviceWM (window messages)
- `+0x5c` 0x00a2e1f4 -> `FUN_0068bb90` : mode switch dispatch (0x3adc select)
- `+0x60` 0x00a2e1f8 -> `FUN_005eeb70` : stub/assert (unused)
- `+0x64` 0x00a2e1fc -> `FUN_005eeb70` : stub/assert (unused)
- `+0x68` 0x00a2e200 -> `FUN_00632050` : no-op
- `+0x6c` 0x00a2e204 -> `FUN_006b1b80` : no-op (empty)
- `+0x70` 0x00a2e208 -> `FUN_0068a7f0` : misc state update (sets flags @0x2d8, 0x3b68, etc.)
- `+0x74` 0x00a2e20c -> `FUN_006853b0` : invoke callback list @0x18c (alt)
- `+0x78` 0x00a2e210 -> `FUN_006853d0` : remove from callback list @0x18c
- `+0x7c` 0x00a2e214 -> `FUN_00685460` : invoke callback list @0x194
- `+0x80` 0x00a2e218 -> `FUN_00685480` : remove from callback list @0x194
- `+0x84` 0x00a2e21c -> `FUN_00685510` : invoke callback list @0x1a4
- `+0x88` 0x00a2e220 -> `FUN_00685530` : remove from callback list @0x1a4
- `+0x8c` 0x00a2e224 -> `FUN_006a5a00` : get rect (calls FUN_00682d70)
- `+0x90` 0x00a2e228 -> `FUN_0068d950` : get window rect (screen coords if windowed)
- `+0x94` 0x00a2e22c -> `FUN_0068a8e0` : log GL vendor/renderer/version
- `+0x98` 0x00a2e230 -> `FUN_0069e220` : ScenePresent (glFinish or SwapBuffers)
- `+0x9c` 0x00a2e234 -> `FUN_0069e150` : SceneClear
- `+0xa0` 0x00a2e238 -> `FUN_0069e510` : XformSetProjection
- `+0xa4` 0x00a2e23c -> `FUN_0069e540` : XformSetView
- `+0xa8` 0x00a2e240 -> `FUN_0069e560` : Draw(CGxBatch*, indexed)
- `+0xac` 0x00a2e244 -> `FUN_006855c0` : reset dynamic draw buffer state
- `+0xb0` 0x00a2e248 -> `FUN_006845b0` : submit dynamic geometry
- `+0xb4` 0x00a2e24c -> `FUN_00685640` : push dynamic vertex data (marks dirty)
- `+0xb8` 0x00a2e250 -> `FUN_00682fa0` : set 2-float state + mark dirty bit
- `+0xbc` 0x00a2e254 -> `FUN_00682f70` : set 3-float state + mark dirty bit
- `+0xc0` 0x00a2e258 -> `FUN_00684590` : set scalar state + mark dirty bit
- `+0xc4` 0x00a2e25c -> `FUN_0069e620` : glPointSize
- `+0xc8` 0x00a2e260 -> `FUN_0069e640` : glLineWidth
- `+0xcc` 0x00a2e264 -> `FUN_00685eb0` : MasterEnableSet(EGxMasterEnables)
- `+0xd0` 0x00a2e268 -> `FUN_0068b410` : buffer alloc/resize helper (GL/VBO path)
- `+0xd4` 0x00a2e26c -> `FUN_0068b030` : buffer release helper (calls FUN_0068af40)
- `+0xd8` 0x00a2e270 -> `FUN_0068b4d0` : BufLock
- `+0xdc` 0x00a2e274 -> `FUN_0068bdd0` : BufUnlock
- `+0xe0` 0x00a2e278 -> `FUN_0068b670` : BufData
- `+0xe4` 0x00a2e27c -> `FUN_00685c60` : TexCreate
- `+0xe8` 0x00a2e280 -> `FUN_0069d720` : TexDestroy
- `+0xec` 0x00a2e284 -> `FUN_0069d750` : TexCopy/Blit (glCopyTexSubImage2D)
- `+0xf0` 0x00a2e288 -> `00936900` : unknown data pointer (needs investigation)
- `+0xf4` 0x00a2e28c -> `FUN_00632050` : no-op
- `+0xf8` 0x00a2e290 -> `FUN_0068b050` : shader/resource create
- `+0xfc` 0x00a2e294 -> `FUN_0068b090` : shader/resource destroy
- `+0x100` 0x00a2e298 -> `FUN_0068b0d0` : shader/resource bind
- `+0x104` 0x00a2e29c -> `FUN_0068b140` : shader/resource unbind
- `+0x108` 0x00a2e2a0 -> `FUN_0068b180` : shader/resource query
- `+0x10c` 0x00a2e2a4 -> `FUN_0068b230` : shader/resource query
- `+0x110` 0x00a2e2a8 -> `FUN_0069f2d0` : texture upload path (calls FUN_006897c0)
- `+0x114` 0x00a2e2ac -> `FUN_0069f370` : texture upload teardown
- `+0x118` 0x00a2e2b0 -> `FUN_0069e970` : ShaderConstantsSet
- `+0x11c` 0x00a2e2b4 -> `FUN_0069f6a0` : texture create/upload helper
- `+0x120` 0x00a2e2b8 -> `FUN_0069f730` : texture upload apply helper
- `+0x124` 0x00a2e2bc -> `FUN_00683640` : set cursor handle/id
- `+0x128` 0x00a2e2c0 -> `FUN_00683650` : returns pointer @`this+0x2960`
- `+0x12c` 0x00a2e2c4 -> `FUN_00684b50` : set cursor hotspot + update cursor texture
- `+0x130` 0x00a2e2c8 -> `FUN_00632050` : no-op
- `+0x134` 0x00a2e2cc -> `FUN_004d5ff0` : returns `1.0f` (float10)
- `+0x138` 0x00a2e2d0 -> `FUN_00632050` : no-op
- `+0x13c` 0x00a2e2d4 -> `FUN_004d5ee0` : returns 0
- `+0x140` 0x00a2e2d8 -> `FUN_00427a90` : returns 0
- `+0x144` 0x00a2e2dc -> `FUN_00653a10` : stub (unknown)
- `+0x148` 0x00a2e2e0 -> `FUN_005eeb70` : stub/assert
- `+0x14c` 0x00a2e2e4 -> `FUN_00653a10` : stub (unknown)

## OpenGL vtable unknowns (need manual function creation)

These entries require additional Ghidra work to resolve:

- `0x00936900` (vtable +0xf0) appears as data, not a defined function
## Formerly undefined vtable slots (now resolved)

These were created manually in Ghidra and are now decompiled:

- `0x0068e450` : Evict managed resources + error check
- `0x0068e510` : GetRenderTarget (stores in `DAT_00c6033c`)
- `0x0068e540` : SetRenderTarget (from `DAT_00c6033c`)
- `0x0069fed0` : return HWND (device window)
- `0x00682d30` : set dirty flag @`this+0x2934`
- `0x00683650` : returns pointer @`this+0x2960`
- `0x006a0110` : return float @`this+0x3ac4`
- `0x006a0120` : return float @`this+0x3ac8`
- `0x006a0130` : return bool (`this+0x3ab8 == 1`)
- `0x0068cca0` : return dword @`this+0x3aec` (likely HWND)
- `0x006b1b80` : no-op (empty)
- `0x004d5ff0` : return `1.0f`

## OpenGL device creation + format change (CGxDeviceOpenGL)

- `FUN_0068daa0` : `DeviceCreate`
  - Captures system gamma ramp into `this+0x354` and `this+0x954`
  - Registers `GxWindowClassOpenGl` (`FUN_0068da10`) and stores atom at `this+0x3af4`
  - Calls `FUN_00682cb0(this, windowProc, format)` to create/attach context
  - On failure, calls vtable `+0x2c` (DeviceDestroy) and returns `0`
- `FUN_0068d6d0` : `DeviceSetFormat`
  - Logs `CGxDeviceOpenGl::DeviceSetFormat():`
  - Destroys current HWND at `this+0x3aec`
  - Validates new format via `FUN_0068c9b0`
  - Creates new HWND via `FUN_0068ccb0` and updates `this+0x3aec`
  - Calls `FUN_0068c860` (pixel format setup) and `FUN_0068cf60` (context create)
  - Calls `FUN_006840f0` (apply format) then vtable `+0x3c` (gamma update)

## OpenGL render-state upload (IRsSendToHw)

- `FUN_0069cdc0` = `IRsSendToHw(stateIndex)`
  - Uses state array at `this+0x28f4` with stride `0x18`
  - Caches GL state in fields like `this+0x3a90`, `this+0x3abc`, `this+0x3acc`
  - Key mapping tables:
    - `DAT_00ad8d90` / `DAT_00ad8dc0` : blend factor mapping (`glBlendFunc`)
    - `DAT_00ad8e80` : depth func mapping (`glDepthFunc`)
    - `DAT_00ad8e90` : `glColorMaterial` mapping
  - Notable state handling:
    - polygon offset (`GL_POLYGON_OFFSET_FILL`) via `glPolygonOffset`
    - alpha test (`GL_ALPHA_TEST`) via `glAlphaFunc`
    - fog params (`glFogf`/`glFogfv`)
    - depth test (`GL_DEPTH_TEST`) / depth mask
    - color mask (`glColorMask`)
    - cull face (`GL_CULL_FACE`) + `glFrontFace`
    - clip planes (`GL_CLIP_PLANE0..5`) via `glEnable(0x3000 + i)`
    - scissor test (`GL_SCISSOR_TEST`)
  - Ranges of states dispatch to helper functions:
    - `FUN_006923a0` (states 0x15..0x24)
    - `FUN_00693020` (states 0x25..0x2c)
    - `FUN_00693730` (states 0x2d..0x34)
    - `FUN_00695fd0` (states 0x35..0x3c)
    - `FUN_0069f8d0` / `FUN_0069f760` (single states in higher range)

### `DAT_00ad8d90` contents

Extracted as 12 DWORDs (little endian):

```
{
  0x00000001, 0x00000001, 0x00000302, 0x00000302,
  0x00000306, 0x00000306, 0x00000306, 0x00000303,
  0x00000303, 0x00000302, 0x00000001, 0x00008003
}
```

### `DAT_00ad8dc0` contents

Extracted as 12 DWORDs (little endian):

```
{
  0x00000000, 0x00000000, 0x00000303, 0x00000001,
  0x00000000, 0x00000300, 0x00000001, 0x00000001,
  0x00000000, 0x00000000, 0x00000001, 0x00008004
}
```

### `DAT_00ad8e80` contents

Extracted as 4 DWORDs (little endian):

```
{
  0x00000203, 0x00000202, 0x00000206, 0x00000201
}
```

### `DAT_00ad8e90` contents

Extracted as 3 DWORDs (little endian):

```
{
  0x00001602, 0x00001202, 0x00001600
}
```

### `DAT_00ad8eac` contents

Extracted as 6 DWORDs (little endian):

```
{
  0x00000000, 0x00000001, 0x00000003,
  0x00000004, 0x00000005, 0x00000006
}
```

## OpenGL draw path

- `FUN_0069e560` = `Draw(CGxBatch*, indexed)`
  - Ensures GL state is up to date via `FUN_0069d550`
  - Uses `glDrawElements` with `GL_UNSIGNED_SHORT` (`0x1403`) and index base
    `this+0x3b54` when `indexed != 0`
  - Uses `glDrawArrays` when non-indexed
  - Primitive type is mapped by `DAT_00ad8eac[*param_1]`

## OpenGL frame + matrix helpers

- `FUN_0069e220` = `ScenePresent`
  - Calls `(**vtable+0x10)()` then either `glFinish()` or
    `wglSwapLayerBuffers(HDC, 1)` depending on flag `this+0x9d7 & 0x40`
  - Uses HDC stored at `this+0xebe`
- `FUN_0069e150` = `SceneClear(flags, color)`
  - Calls `glClearColor` with ARGB packed `color` and `1/255` scale
  - Builds mask: `0x4000` (color) + `0x100` (depth) based on `flags`
  - Temporarily overrides depth/color mask via `FUN_006918f0` and restores
- `FUN_0069e510` = `XformSetProjection`
  - Copies matrix to `this+0xf88` then calls `FUN_0069e280`
- `FUN_0069e540` = `XformSetView`
  - Calls `FUN_00689050` then marks `this+0x3b58 = 1` (dirty)

## OpenGL state helper paths (texture/texgen/array)

These are invoked by `IRsSendToHw` for ranges of render-state IDs.

- `FUN_0069d550` = state flush before draw
  - ORs dirty bits into `this+0x3b58`, then calls a sequence of state
    upload helpers (`FUN_0069f610`, `FUN_00685b50`, `FUN_00695640`, etc.)
- `FUN_006923a0` / `FUN_00693020` / `FUN_00693730` / `FUN_00695fd0`
  - Large switch-based handlers for grouped state ranges
  - Use `(*DAT_00c60600)` / `(*DAT_00c605fc)` (active texture/client texture)
  - Heavy use of `glTexEnvi`, `glTexEnvf`, `glTexEnvfv`, `glTexGeni`
  - Toggle client arrays via `glEnableClientState`/`glDisableClientState`
    or `DAT_00c605ac` / `DAT_00c605b0` (vertex attrib array extensions)
  - Reset per-stage caches stored in `this+0x3968..`
- `FUN_0069f8d0`
  - Uses extension call `DAT_00c605d0` or `DAT_00c605a4` with `0x8620`
  - Toggles render states `0x52` / `0x53` via `FUN_006918f0`
- `FUN_0069f760`
  - Uses extension call `DAT_00c605d0` with `0x8804`
  - Toggles render states `0x4e` / `0x4f` / `0x50` / `0x51`

## OpenGL vertex/array setup and dynamic geometry

- `FUN_00693e40` : vertex array binding/update
  - Handles client arrays for position/normal/color/texcoord
  - Calls `glVertexPointer`, `glNormalPointer`, `glColorPointer`,
    `glTexCoordPointer`
  - Uses VBO paths when `DAT_00c604e4` is set; binds buffers via
    `DAT_00c60588` (target `0x8892`/`0x8893`) and/or buffer object vtbl
  - Tracks enabled state in `this+0x3a54..0x3a68` and texture-unit states
  - Uses generic attribute path if `this+0x3ab0 != 0` via
    `DAT_00c605b0`/`DAT_00c605b4` (vertex attrib extensions)
- `FUN_00691fc0` : index buffer binding
  - Uses `DAT_00c60588(0x8893, buffer)` to bind `GL_ELEMENT_ARRAY_BUFFER`
  - Updates `this+0x3b54` with index base pointer
- `FUN_00692240` : clip plane updates (`glClipPlane`)
- `FUN_006922c0` : scissor update (`glScissor`)
- `FUN_006920e0` : viewport update (`glViewport` + `glDepthRange`)
- `FUN_006921d0` : polygon mode update (`glPolygonMode`)
- `FUN_00695960` : material/vertex color updates (`glMaterialfv`/`glColor4fv`)
- `FUN_00695ac0` : texture matrix updates per tex unit (`glMatrixMode`,
  `glLoadMatrixf`)
- `FUN_00695640` : fixed-function lighting setup (`glLight*`, `glLightModel*`)

Dynamic geometry submission:

- `FUN_006855c0` : reset dynamic draw buffers
  - Clears per-stream buffers and internal arrays
- `FUN_00685640` : push dynamic vertex data
  - Appends vertices + stream data into internal arrays
  - Tracks batch counts and triggers flush when size limit reached
- `FUN_006845b0` : submit dynamic geometry
  - Calls `FUN_006828c0` (pack streams), then `FUN_006823a0` (draw)

Related tables (extracted):

- `DAT_00ad8b4c` / `DAT_00ad8b64` : dynamic buffer sizing / flush thresholds

### `DAT_00ad8b4c` contents

Extracted as 6 DWORDs (little endian):

```
{
  0x00000001, 0x00000002, 0x00000001,
  0x00000003, 0x00000001, 0x00000001
}
```

### `DAT_00ad8b64` contents

Extracted as 6 DWORDs (little endian):

```
{
  0x00000000, 0x00000000, 0x00000001,
  0x00000000, 0x00000002, 0x00000002
}
```

## OpenGL buffer upload path (CGxBuf)

- `FUN_0068b410` : buffer alloc/resize
  - If VBO path enabled (`DAT_00c604e4`) and per-stream flags set,
    calls `DAT_00c6057c` (buffer data) with target from `DAT_00a2e000`
  - Otherwise allocates CPU backing store via `FUN_0076e540`
- `FUN_0068af40` : buffer free/release
  - Frees VBO object via `DAT_00c60584` or destroys CPU backing store
  - Unbinds the buffer if it is currently bound via `FUN_006918f0`
- `FUN_0068b4d0` : `BufLock`
  - Ensures buffer exists (allocates if needed)
  - Uses `DAT_00c60574` to map VBO (likely `glMapBuffer`)
  - Falls back to staging/CPU memory when VBO path unavailable
- `FUN_0068bdd0` : `BufUnlock`
  - Uses `DAT_00c60570` to unmap VBO (likely `glUnmapBuffer`)
  - Uses `DAT_00c60578` to upload subranges (likely `glBufferSubData`)
  - Updates dirty flags and clears `buf->mapped` pointer
- `FUN_0068b670` : `BufData`
  - If VBO path enabled, uses `DAT_00c6057c` (buffer data) or
    `DAT_00c60578` (subdata) depending on size/usage
  - Otherwise copies into CPU backing store

Related lookup tables:

- `DAT_00a2e000` : GL buffer targets (e.g., array vs index buffer)
- `DAT_00a2e008` : render-state IDs used for binding the buffer
- `DAT_00a2e010` : buffer usage mapping (passed to `DAT_00c6057c`)

### `DAT_00a2e000` contents

Extracted as 2 DWORDs (little endian):

```
{
  0x00008892, 0x00008893
}
```

### `DAT_00a2e008` contents

Extracted as 2 DWORDs (little endian):

```
{
  0x00000056, 0x00000057
}
```

### `DAT_00a2e010` contents

Extracted as 3 DWORDs (little endian):

```
{
  0x000088e4, 0x000088e8, 0x000088e0
}
```

## OpenGL texture upload path (CGxTex)

- `FUN_006897c0` : texture allocation + lookup by name (hash table)
  - Caches textures by `name` or `name:idx` for array usage
  - Calls `FUN_00684970` to allocate/upload texture data
  - Increments refcount at `tex->0x1c`
- `FUN_0069f2d0` : bulk texture upload dispatcher
  - Calls `FUN_006897c0` then per-texture handlers:
    - `param_2 == 0` -> `FUN_0069ed50`
    - `param_2 == 4` -> `FUN_0069f270`
  - For single textures, calls `FUN_0069ec10` or `FUN_0069eb10`
- `FUN_0069f370` : texture upload teardown
  - Calls `FUN_0069e8c0` (type 0) or `FUN_0069e930` (type 4)
  - Always calls `FUN_00687820` (release/refcount)
- `FUN_0069f6a0` : texture create/upload helper
  - Calls `FUN_00684970` then `FUN_0069ec10`/`FUN_0069eb10`
- `FUN_0069f730` : texture apply helper (bind/setup)
  - Calls `FUN_0069ec10` for type 0 or `FUN_0069eb10` for type 4
- `FUN_0069d720` : `TexDestroy` (glDeleteTextures + cleanup)
- `FUN_0069d750` : `TexCopy` / blit
  - Uses `DAT_00a2e05c` for target mapping
  - Uses `DAT_00a2f2e0` for cube-face mapping
  - Uses `DAT_00a2f210` for internal format mapping
  - Calls `glCopyTexImage2D` or `glCopyTexSubImage2D`

Texture state helpers:

- `FUN_0069ec10` / `FUN_0069eb10` : set shader/combiner state for textures
  - Use extension function tables (`DAT_00c605b8`, `DAT_00c605d0`, `DAT_00c605d4`)
- `FUN_0069e8c0` / `FUN_0069e930` : release GL texture object for type 0/4

Additional texture upload helpers:

- `FUN_0069de60` : main texture upload path
  - Creates GL texture object (if missing) and binds it
  - Computes mip level count and sets `GL_TEXTURE_MAX_LEVEL` (0x813d)
  - Uses `piVar2[0xd]` callback to fetch per‑mip data; calls `FUN_0069d920`
    to issue `glTexImage2D`, `glTexSubImage2D`, or compressed variants
  - Respects flags in `tex->0x2c` and `tex->0x5b/0x5c/0x5a` dirty flags
- `FUN_0069d620` : sampler state setup
  - Uses `DAT_00a2f1f8` (MAG) and `DAT_00a2f1e0` (MIN) filter tables
  - Wrap S/T derived from bits in `tex->0x2c`
  - Optional anisotropy (`0x84fe`) and depth compare (`0x8191`)
- `FUN_0069d920` : per‑mip upload (format/type/target)
  - `ARRAY_00a2f210` : internal format mapping
  - `DAT_00a2f278` : pixel format mapping (GL format)
  - `DAT_00a2f2ac` : pixel type mapping (GL type)
  - `DAT_00a2f2e0` : cube face mapping
  - Uses `DAT_00c605f4` / `DAT_00c605f8` for compressed uploads
- `FUN_00684970` : shader bytecode preprocessor for `.bls` files
  - Builds path `"%s\\%s\\%s.bls"` and scans for `PARAM c[...]` lines
  - Rewrites `program.local` / `program.env` ranges in ARB assembly
- `FUN_0069ed50` / `FUN_0069f270`
  - Patch ARB program strings (local/env) for VS/PS variants
  - Triggered by texture program load for specific shader types
- `FUN_00687820` : texture release (refcount dec + destroy)
- `FUN_00687870` / `FUN_00689760` : texture lookup/insert in hash table

Related tables (extracted):

- `DAT_00a2f1f8` / `DAT_00a2f1e0` : sampler filter mappings
- `DAT_00a2f278` / `DAT_00a2f2ac` : pixel format/type mappings
- `DAT_00a2daf8` / `DAT_00a2db60` / `DAT_00a2db0c` : bytes‑per‑pixel/block tables

### `DAT_00a2f1f8` contents

Extracted as 6 DWORDs (little endian):

```
{
  0x00002600, 0x00002601, 0x00002601,
  0x00002601, 0x00002601, 0x00002601
}
```

### `DAT_00a2f1e0` contents

Extracted as 6 DWORDs (little endian):

```
{
  0x00002600, 0x00002601, 0x00002700,
  0x00002701, 0x00002703, 0x00002703
}
```

### `DAT_00a2f278` contents

Extracted as 5 DWORDs (little endian):

```
{
  0x00000000, 0x00001908, 0x000080e1,
  0x000080e1, 0x000080e1
}
```

### `DAT_00a2f2ac` contents

Extracted as 5 DWORDs (little endian):

```
{
  0x00000000, 0x00008367, 0x00008367,
  0x00008365, 0x00008366
}
```

### `DAT_00a2daf8` contents

Extracted as 5 DWORDs (little endian):

```
{
  0x00000000, 0x00000020, 0x00000020,
  0x00000010, 0x00000010
}
```

### `DAT_00a2db60` contents

Extracted as 14 DWORDs (little endian):

```
{
  0x00000000, 0x00000004, 0x00000004, 0x00000002,
  0x00000002, 0x00000002, 0x00000008, 0x00000010,
  0x00000010, 0x00000002, 0x00000004, 0x00000004,
  0x00000004, 0x00200000
}
```

### `DAT_00a2db0c` contents

Extracted as 8 DWORDs (little endian):

```
{
  0x00000010, 0x00000004, 0x00000008, 0x00000008,
  0x00000010, 0x00000020, 0x00000020, 0x00000020
}
```

### `DAT_00a2e05c` contents

Extracted as 4 DWORDs (little endian):

```
{
  0x00000de1, 0x00008513, 0x000084f5, 0x00000de1
}
```

### `DAT_00a2f2e0` contents

Extracted as 6 DWORDs (little endian):

```
{
  0x00008515, 0x00008516, 0x00008517,
  0x00008518, 0x00008519, 0x0000851a
}
```

### `DAT_00a2f210` contents

Extracted as 13 DWORDs (little endian):

```
{
  0x00000000, 0x00008058, 0x00008058, 0x00008056,
  0x00008057, 0x00008050, 0x000083f1, 0x000083f2,
  0x000083f3, 0x00008709, 0x0000881b, 0x00000000,
  0x000081a6
}
```

### `DAT_00a2f244` contents

Adjacent table used for row-length/bytes-per-pixel math in uploads:

```
{
  0x00000000, 0x00000004, 0x00000004, 0x00000002, 0x00000002
}
```
