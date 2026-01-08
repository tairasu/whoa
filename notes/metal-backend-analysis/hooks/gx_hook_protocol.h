#pragma once

// Draft IPC protocol between the 32-bit GX hook DLL and the 64-bit Metal helper.
// This is a design sketch only; wire format can change as reverse-engineering fills gaps.

#include <stdint.h>

#pragma pack(push, 1)

enum GxCmdType : uint32_t {
  kGxCmd_Invalid = 0,
  kGxCmd_Init = 1,
  kGxCmd_Shutdown = 2,

  kGxCmd_DeviceCreate = 10,
  kGxCmd_DeviceDestroy = 11,

  kGxCmd_FrameBegin = 20,
  kGxCmd_SceneClear = 21,
  kGxCmd_Draw = 22,
  kGxCmd_Present = 23,

  kGxCmd_BufferCreate = 30,
  kGxCmd_BufferDestroy = 31,
  kGxCmd_BufferUpdate = 32,

  kGxCmd_TextureCreate = 40,
  kGxCmd_TextureDestroy = 41,
  kGxCmd_TextureUpdate = 42,

  kGxCmd_ShaderCreate = 50,
  kGxCmd_ShaderDestroy = 51,
  kGxCmd_ShaderBind = 52,
  kGxCmd_ShaderConstantsSet = 53,

  kGxCmd_RenderStateSet = 60,
  kGxCmd_ViewportSet = 61,
  kGxCmd_ScissorSet = 62,
  kGxCmd_SamplerStateSet = 63
};

enum GxShaderStage : uint32_t {
  kGxStage_Vertex = 0,
  kGxStage_Pixel = 1
};

typedef uint32_t GxResId;

struct GxCmdHeader {
  uint32_t type;    // GxCmdType
  uint32_t size;    // total bytes including header
  uint32_t seq;     // monotonically increasing id
  uint32_t frame;   // frame index
};

struct GxCmdInit {
  GxCmdHeader header;
  uint32_t protocol_version;
};

struct GxCmdDeviceCreate {
  GxCmdHeader header;
  uint32_t adapter_index;
  uint32_t width;
  uint32_t height;
  uint32_t fullscreen; // 0/1
};

struct GxCmdDeviceDestroy {
  GxCmdHeader header;
};

struct GxCmdFrameBegin {
  GxCmdHeader header;
  uint32_t frame_id;
};

struct GxCmdSceneClear {
  GxCmdHeader header;
  uint32_t clear_flags; // matches EGxRenderState clear bits
  float color_rgba[4];
  float depth;
  uint32_t stencil;
};

struct GxCmdDraw {
  GxCmdHeader header;
  uint32_t prim_type;      // EGxPrimType
  uint32_t index_count;
  uint32_t start_index;
  uint32_t base_vertex;
  uint32_t vertex_count;
  GxResId index_buffer;
  GxResId vertex_buffer;
};

struct GxCmdPresent {
  GxCmdHeader header;
};

struct GxCmdBufferCreate {
  GxCmdHeader header;
  GxResId buffer_id;
  uint32_t size_bytes;
  uint32_t stride;
  uint32_t usage;      // GX buffer usage flags
  uint32_t is_index;   // 0/1
};

struct GxCmdBufferDestroy {
  GxCmdHeader header;
  GxResId buffer_id;
};

struct GxCmdBufferUpdate {
  GxCmdHeader header;
  GxResId buffer_id;
  uint32_t offset_bytes;
  uint32_t size_bytes;
  uint8_t data[1];
};

struct GxCmdTextureCreate {
  GxCmdHeader header;
  GxResId texture_id;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mip_count;
  uint32_t format;    // EGxTexFormat
  uint32_t usage;     // GX texture usage flags
};

struct GxCmdTextureDestroy {
  GxCmdHeader header;
  GxResId texture_id;
};

struct GxCmdTextureUpdate {
  GxCmdHeader header;
  GxResId texture_id;
  uint32_t mip_level;
  uint32_t array_slice;
  uint32_t width;
  uint32_t height;
  uint32_t row_pitch;
  uint32_t size_bytes;
  uint8_t data[1];
};

struct GxCmdShaderCreate {
  GxCmdHeader header;
  GxResId shader_id;
  uint32_t stage;       // GxShaderStage
  uint32_t bytecode_len;
  uint8_t bytecode[1];
};

struct GxCmdShaderDestroy {
  GxCmdHeader header;
  GxResId shader_id;
};

struct GxCmdShaderBind {
  GxCmdHeader header;
  uint32_t stage;       // GxShaderStage
  GxResId shader_id;
};

struct GxCmdShaderConstantsSet {
  GxCmdHeader header;
  uint32_t stage;       // GxShaderStage
  uint32_t start_vec4;
  uint32_t vec4_count;
  float data[1];
};

struct GxCmdRenderStateSet {
  GxCmdHeader header;
  uint32_t state_id;    // EGxRenderState
  uint32_t value;
};

struct GxCmdViewportSet {
  GxCmdHeader header;
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  float min_depth;
  float max_depth;
};

struct GxCmdScissorSet {
  GxCmdHeader header;
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
};

struct GxCmdSamplerStateSet {
  GxCmdHeader header;
  uint32_t stage;       // texture unit
  uint32_t state_id;    // EGxSamplerState
  uint32_t value;
};

#pragma pack(pop)
