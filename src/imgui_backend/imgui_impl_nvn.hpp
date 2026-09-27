#pragma once

#define IMGUI_USER_CONFIG "imgui_user_config.h"

#include "ImguiShaderCompiler.h"
#include "imgui.h"
#include "nvn_Cpp.h"
#include "nvn_CppMethods.h"
#include "types.h"
#include "MemoryBuffer.h"
#include "NvnTextureManager.h"

#include "os/os_tick.hpp"

#ifdef __cplusplus

namespace ImguiNvnBackend {

  struct NvnBackendInitInfo {
    nvn::Device *device;
    nvn::Queue *queue;
    nvn::CommandBuffer *cmdBuf;
  };

  struct NvnBackendData {

    // general data

    nvn::Device *device;
    nvn::Queue *queue;
    nvn::CommandBuffer *cmdBuf;

    // builders

    nvn::BufferBuilder bufferBuilder;
    nvn::MemoryPoolBuilder memPoolBuilder;
    nvn::TextureBuilder texBuilder;
    nvn::SamplerBuilder samplerBuilder;

    // shader data

    nvn::Program shaderProgram;

    MemoryBuffer *shaderMemory;
    MemoryBuffer *uniformMemory;

    nvn::ShaderData shaderDatas[2]; // 0 - Vert 1 - Frag

    nvn::VertexStreamState streamState;
    nvn::VertexAttribState attribStates[3];

    // Dynamic atlas textures, descriptors, and GPU lifetime tracking.
    NvnTextureManager textures;
    bool shaderProgramReady = false;

    // render data

    MemoryBuffer *vtxBuffer;
    MemoryBuffer *idxBuffer;

    // misc data

    nn::TimeSpanType lastTick;
    bool isInitialized;

    bool isDisableInput = true;

    CompiledData imguiShaderBinary;

    // test shader data

    bool isUseTestShader = false;
    nvn::Program testShader;
    nvn::ShaderData testShaderDatas[2]; // 0 - Vert 1 - Frag
    MemoryBuffer *testShaderBuffer;
    CompiledData testShaderBinary;
  };

  bool createShaders();

  bool setupShaders(u8 *shaderBinary, ulong binarySize);

  bool setupTextures();

  bool InitBackend(const NvnBackendInitInfo &initInfo);

  void ShutdownBackend();

  void updateInput();

  void newFrame();

  void setRenderStates();

  void renderDrawData(ImDrawData *drawData);

  NvnBackendData *getBackendData();
}; // namespace ImguiNvnBackend

#endif
