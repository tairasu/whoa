# Metal backend mapping and hook points (3.3.5a source)

This repo exposes the graphics layer through a Gx abstraction. The "tables"
you want are split between common Gx enums/structs and per-backend mapping
tables or helper functions.

## Mapping tables and enums in this repo

- Core enums used everywhere:
  - `src/gx/Types.hpp` (EGxApi, EGxBlend, EGxPrim, EGxRenderState, EGxTexFormat, etc.)
- Common Gx tables:
  - `src/gx/CGxDevice.cpp` (alpha refs, primitive vertex adjust/div, tex format bit depth + bytes per block)
  - `src/gx/Buffer.cpp` (vertex format layouts in `Buffer::s_vertexBufDesc` and offsets in `Buffer::s_vertexBufOffset`)
- D3D9 mapping tables:
  - `src/gx/d3d/CGxDeviceD3d.cpp` (Gx texture formats -> D3D formats, primitive conversion, blend factors, wrap modes)
- OpenGL/GLL mapping tables:
  - `src/gx/gll/CGxDeviceGLL.cpp` (blend factors, primitive conversion, texture formats, buffer usage)
- Metal mapping functions:
  - `src/gx/mtl/CGxDeviceMTL.mm` (Gx primitive -> MTL, Gx texture format -> MTLPixelFormat, blend factors)

These are the concrete "what maps to what" definitions your Metal backend
should mirror when you map a Gx call to a Metal call.

## Call flow in the source (where Gx calls land)

- `src/gx/Device.cpp`:
  - `GxDevCreate()` chooses a backend and sets `g_theGxDevicePtr`.
  - `GxDevApi()` and `GxDevWindow()` forward to the device.
- `src/gx/RenderState.cpp`:
  - `GxRsSet/GxRsPush/GxRsPop` forward to `g_theGxDevicePtr`.
- `src/gx/Draw.cpp`:
  - `GxDraw()` and `GxScenePresent()` forward to `g_theGxDevicePtr`.
- `src/gx/Buffer.cpp`:
  - `GxBufLock/GxBufUnlock/GxPrimVertexPtr/GxPrimIndexPtr` forward to `g_theGxDevicePtr`.
- `src/gx/CGxDevice.hpp`:
  - Defines the virtual interface (DeviceCreate/Draw/BufLock/TexCreate/ShaderCreate/etc).

## Metal backend specifics in this repo

- `src/gx/mtl/CGxDeviceMTL.mm` contains:
  - Mappings for prim type, texture formats, blend factors.
  - A minimal shader set (inline Metal shader source).
  - Render-state handling; a number of render states are explicitly marked
    "not implemented" yet (polygon offset, texgen, fixed-function ops, etc.).

## What to find in the decompiled 3.3.5a client (Ghidra)

To replace or hook the graphics layer in the *compiled* client, you need to
identify the same concepts in the binary:

1. `g_theGxDevicePtr` (global)
   - The core Gx calls are thin wrappers around this pointer.
2. `GxDevCreate`, `GxDevApi`, `GxDevWindow`
   - Factory + accessor pattern; usually findable via strings like
     `d3d9.dll`, `GxWindowClassD3d`, or device creation logs.
3. `CGxDevice` vtable and method order
   - Match the interface in `src/gx/CGxDevice.hpp` (DeviceCreate, Draw,
     BufLock/Unlock, TexCreate, ShaderCreate, ShaderConstantsSet, etc.).
4. `CGxFormat`, `CGxCaps`, `CGxTex`, `CGxBuf`, `CGxShader`, `CGxBatch`
   - These structs must match size/layout. Use Ghidra to confirm offsets.
5. Render-state and vertex format tables
   - Expect arrays that map Gx enums to D3D9/GL constants.
   - `EGxRenderState` is often used in large switch statements in the device.

## Feasibility notes for a Metal backend on the original client

- The original 3.3.5a Windows client is D3D9-based; it has no Metal path.
- It *can* work if you hook at the Gx layer (preferred) or below at D3D9.
- The simplest injection model is:
  - Hook `GxDevCreate` to replace the device or swap the vtable to your
    proxy, then forward any unimplemented calls to the original device.
  - Alternatively, hook the Gx wrapper functions (`GxRsSet`, `GxDraw`,
    `GxBufLock`, etc.).
- The Metal backend in this repo is incomplete relative to D3D9; missing
  render states need to be filled or emulated for feature parity.

## Metal mapping spec (GX 3.3.5a, derived from Ghidra)

This section captures the concrete GX->GL/D3D mappings observed in the
3.3.5a binary and the expected Metal equivalents. Use these as the canonical
mapping set for a GX-proxy Metal backend.

### Blend factors (GX enum index -> GL -> Metal)

From `DAT_00ad8d90` (src) and `DAT_00ad8dc0` (dst), 12 entries:

```
Index | Src (GL)           | Dst (GL)           | Metal
0     | 0x00000001 ONE     | 0x00000000 ZERO    | .one / .zero
1     | 0x00000001 ONE     | 0x00000000 ZERO    | .one / .zero
2     | 0x00000302 SRC_A   | 0x00000303 1-SRC_A | .sourceAlpha / .oneMinusSourceAlpha
3     | 0x00000302 SRC_A   | 0x00000001 ONE     | .sourceAlpha / .one
4     | 0x00000306 DST_C   | 0x00000000 ZERO    | .destinationColor / .zero
5     | 0x00000306 DST_C   | 0x00000300 SRC_C   | .destinationColor / .sourceColor
6     | 0x00000306 DST_C   | 0x00000001 ONE     | .destinationColor / .one
7     | 0x00000303 1-SRC_A | 0x00000001 ONE     | .oneMinusSourceAlpha / .one
8     | 0x00000303 1-SRC_A | 0x00000000 ZERO    | .oneMinusSourceAlpha / .zero
9     | 0x00000302 SRC_A   | 0x00000000 ZERO    | .sourceAlpha / .zero
10    | 0x00000001 ONE     | 0x00000001 ONE     | .one / .one
11    | 0x00008003 CONST_A | 0x00008004 1-C_A   | .blendAlpha / .oneMinusBlendAlpha
```

Notes:
- `SRC_A` = `GL_SRC_ALPHA` (0x0302)
- `DST_C` = `GL_DST_COLOR` (0x0306)
- `SRC_C` = `GL_SRC_COLOR` (0x0300)
- `CONST_A` = `GL_CONSTANT_ALPHA` (0x8003)
- `1-C_A` = `GL_ONE_MINUS_CONSTANT_ALPHA` (0x8004)

### Depth compare (GX enum index -> GL -> Metal)

From `DAT_00ad8e80` (4 entries):

```
Index | GL         | Metal
0     | 0x0203 LE  | .lessEqual
1     | 0x0202 EQ  | .equal
2     | 0x0206 GE  | .greaterEqual
3     | 0x0201 LT  | .less
```

### Color material mapping (GX enum index -> GL)

From `DAT_00ad8e90`:

```
Index | GL
0     | 0x1602 GL_AMBIENT_AND_DIFFUSE
1     | 0x1202 GL_SPECULAR
2     | 0x1600 GL_EMISSION
```

### Primitive type (GX enum index -> GL -> Metal)

From `DAT_00ad8eac` (6 entries):

```
Index | GL               | Metal
0     | 0x0000 POINTS    | .point
1     | 0x0001 LINES     | .line
2     | 0x0003 LINE_STRIP| .lineStrip
3     | 0x0004 TRIANGLES | .triangle
4     | 0x0005 TRI_STRIP | .triangleStrip
5     | 0x0006 TRI_FAN   | .triangleFan
```

### Buffer targets and usage

From `DAT_00a2e000` / `DAT_00a2e010`:

```
GL_ARRAY_BUFFER        0x8892  -> MTLBuffer
GL_ELEMENT_ARRAY_BUFFER0x8893  -> MTLBuffer (index)

Usage map (GX enum index -> GL -> Metal storage)
0: 0x88E4 GL_STATIC_DRAW  -> .storageModeManaged/.shared (upload once)
1: 0x88E8 GL_DYNAMIC_DRAW -> .storageModeManaged/.shared (frequent update)
2: 0x88E0 GL_STREAM_DRAW  -> .storageModeManaged/.shared (per-frame update)
```

### Texture targets

From `DAT_00a2e05c`:

```
Index 0: 0x0DE1 GL_TEXTURE_2D
Index 1: 0x8513 GL_TEXTURE_CUBE_MAP
Index 2: 0x84F5 GL_TEXTURE_RECTANGLE (treat as 2D + unnormalized UV)
Index 3: 0x0DE1 GL_TEXTURE_2D
```

### Cube faces

From `DAT_00a2f2e0`:

```
0x8515..0x851A -> POS_X, NEG_X, POS_Y, NEG_Y, POS_Z, NEG_Z
```

### Sampler filters

From `DAT_00a2f1f8` (MAG) and `DAT_00a2f1e0` (MIN):

```
0x2600 GL_NEAREST                -> min/mag nearest, mip none
0x2601 GL_LINEAR                 -> min/mag linear, mip none
0x2700 GL_NEAREST_MIPMAP_NEAREST -> min nearest, mip nearest
0x2701 GL_LINEAR_MIPMAP_NEAREST  -> min linear,  mip nearest
0x2703 GL_LINEAR_MIPMAP_LINEAR   -> min linear,  mip linear
```

### Texture format mapping

Internal formats from `DAT_00a2f210` (selected known values):

```
0x8058 GL_RGBA8            -> MTLPixelFormatRGBA8Unorm
0x8050 GL_RGB8             -> MTLPixelFormatRGBA8Unorm (expand) or RGB8 if supported
0x8056 GL_RGBA4            -> MTLPixelFormatRGBA8Unorm (expand) or RGBA4 if supported
0x8057 GL_RGB5_A1          -> MTLPixelFormatB5G5R5A1Unorm
0x83F1 GL_COMPRESSED_RGBA_S3TC_DXT1_EXT -> MTLPixelFormatBC1_RGBA
0x83F2 GL_COMPRESSED_RGBA_S3TC_DXT3_EXT -> MTLPixelFormatBC2_RGBA
0x83F3 GL_COMPRESSED_RGBA_S3TC_DXT5_EXT -> MTLPixelFormatBC3_RGBA
0x8709 GL_DEPTH_COMPONENT16 -> MTLPixelFormatDepth16Unorm
0x881B GL_DEPTH_COMPONENT24 -> MTLPixelFormatDepth24Unorm_Stencil8 (closest)
0x81A6 GL_ALPHA8            -> MTLPixelFormatA8Unorm (or R8Unorm)
```

Pixel format/type from `DAT_00a2f278` / `DAT_00a2f2ac`:

```
Format: 0x1908 GL_RGBA, 0x80E1 GL_BGRA
Type:   0x8367 GL_UNSIGNED_INT_8_8_8_8_REV
        0x8365 GL_UNSIGNED_SHORT_4_4_4_4_REV
        0x8366 GL_UNSIGNED_SHORT_1_5_5_5_REV
```

Bytes-per-pixel/block tables used by `FUN_0069d920`:

```
DAT_00a2daf8 / DAT_00a2db60 / DAT_00a2db0c
```

These drive row pitch and block size for compressed formats and should be
reflected in Metal upload helpers.

## Wine + macOS caveat (important)

The WoW 3.3.5a client is 32-bit. On modern macOS versions, native 32-bit
libraries cannot be loaded into a process. If your Metal backend is a native
macOS library, you will likely need a 64-bit helper process and an IPC bridge
from the injected 32-bit DLL (shared memory, sockets, or a command queue).
