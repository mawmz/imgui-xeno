#pragma once

#include "imgui.h"
#include "nvn_Cpp.h"
#include "nvn_CppMethods.h"

namespace ImguiNvnBackend {

// Owns the renderer's descriptors, dynamic atlas textures and submission fence.
// Single context/queue only. Call WaitIdle before reusing any overlay buffers.
class NvnTextureManager {
public:
  bool Initialize(nvn::Device* device);
  bool WaitIdle();
  bool UpdateTextures(ImVector<ImTextureData*>* textures);
  void BindPools(nvn::CommandBuffer* commands);
  void MarkSubmitted(nvn::Queue* queue);
  bool Shutdown();

private:
  static constexpr int DescriptorCount = 356;
  struct Texture {
    nvn::MemoryPool pool;
    nvn::Texture texture;
    void* allocation = nullptr;
    ImTextureData* source = nullptr;
    int descriptor = -1;
    bool poolReady = false;
    bool textureReady = false;
  };

  bool Update(ImTextureData* texture);
  void Destroy(Texture* texture);
  nvn::Device* device_ = nullptr;
  nvn::MemoryPool descriptorMemory_;
  nvn::TexturePool texturePool_;
  nvn::SamplerPool samplerPool_;
  nvn::Sampler sampler_;
  nvn::Sync fence_;
  void* descriptorAllocation_ = nullptr;
  Texture* textures_[DescriptorCount] = {};
  int firstTexture_ = 0;
  int samplerId_ = 0;
  int maxTextureSize_ = 0;
  bool memoryReady_ = false;
  bool texturePoolReady_ = false;
  bool samplerPoolReady_ = false;
  bool samplerReady_ = false;
  bool fenceReady_ = false;
  bool pending_ = false;
  bool dirty_ = false;
  bool ready_ = false;
};

} // namespace ImguiNvnBackend
