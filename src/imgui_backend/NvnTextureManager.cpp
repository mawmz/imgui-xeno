#include "NvnTextureManager.h"

namespace ImguiNvnBackend {
namespace {
  size_t AlignUp(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
  }

  // Keep the original ImGui allocation for freeing; NVN receives its aligned interior.
  void* AllocateAligned(size_t size, size_t alignment, void*& allocation) {
    allocation = IM_ALLOC(size + alignment - 1);
    return allocation ? reinterpret_cast<void*>(AlignUp(reinterpret_cast<uintptr_t>(allocation), alignment)) : nullptr;
  }
}

bool NvnTextureManager::Initialize(nvn::Device* device) {
  device_ = device;
  int samplerSize = 0, textureSize = 0;
  device->GetInteger(nvn::DeviceInfo::SAMPLER_DESCRIPTOR_SIZE, &samplerSize);
  device->GetInteger(nvn::DeviceInfo::TEXTURE_DESCRIPTOR_SIZE, &textureSize);
  device->GetInteger(nvn::DeviceInfo::RESERVED_TEXTURE_DESCRIPTORS, &firstTexture_);
  device->GetInteger(nvn::DeviceInfo::RESERVED_SAMPLER_DESCRIPTORS, &samplerId_);
  device->GetInteger(nvn::DeviceInfo::MAX_TEXTURE_SIZE, &maxTextureSize_);
  if (samplerSize <= 0 || textureSize <= 0 || firstTexture_ < 0 || firstTexture_ >= DescriptorCount ||
      samplerId_ < 0 || samplerId_ >= DescriptorCount || maxTextureSize_ <= 0)
    return false;

  const size_t textureOffset = AlignUp(size_t(samplerSize) * DescriptorCount, 0x1000);
  const size_t poolSize = AlignUp(textureOffset + size_t(textureSize) * DescriptorCount, 0x1000);
  void* storage = AllocateAligned(poolSize, 0x1000, descriptorAllocation_);
  if (!storage)
    return false;
  memset(storage, 0, poolSize);
  nvn::MemoryPoolBuilder poolBuilder;
  poolBuilder.SetDefaults().SetDevice(device)
      .SetFlags(nvn::MemoryPoolFlags::CPU_UNCACHED | nvn::MemoryPoolFlags::GPU_CACHED)
      .SetStorage(storage, poolSize);
  if (!(memoryReady_ = descriptorMemory_.Initialize(&poolBuilder)))
    return false;
  if (!(samplerPoolReady_ = samplerPool_.Initialize(&descriptorMemory_, 0, DescriptorCount)))
    return false;
  if (!(texturePoolReady_ = texturePool_.Initialize(&descriptorMemory_, textureOffset, DescriptorCount)))
    return false;

  nvn::SamplerBuilder samplerBuilder;
  samplerBuilder.SetDefaults().SetDevice(device)
      .SetMinMagFilter(nvn::MinFilter::LINEAR, nvn::MagFilter::LINEAR)
      .SetWrapMode(nvn::WrapMode::CLAMP_TO_EDGE, nvn::WrapMode::CLAMP_TO_EDGE, nvn::WrapMode::CLAMP_TO_EDGE);
  if (!(samplerReady_ = sampler_.Initialize(&samplerBuilder)))
    return false;
  samplerPool_.RegisterSampler(samplerId_, &sampler_);
  if (!(fenceReady_ = fence_.Initialize(device)))
    return false;
  ready_ = true;
  return true;
}

bool NvnTextureManager::WaitIdle() {
  if (!pending_)
    return true;
  // A timeout/failure leaves ownership intact; the renderer skips this frame.
  const auto result = fence_.Wait(1000000000ULL);
  if (result != nvn::SyncWaitResult::ALREADY_SIGNALED && result != nvn::SyncWaitResult::CONDITION_SATISFIED)
    return false;
  pending_ = false;
  return true;
}

void NvnTextureManager::MarkSubmitted(nvn::Queue* queue) {
  queue->FenceSync(&fence_, nvn::SyncCondition::ALL_GPU_COMMANDS_COMPLETE, nvn::SyncFlagBits::FLUSH_FOR_CPU);
  queue->Flush(); // Ensure the CPU wait can observe this fence without another submission.
  pending_ = true;
}

void NvnTextureManager::Destroy(Texture* texture) {
  if (texture->textureReady)
    texture->texture.Finalize();
  if (texture->poolReady)
    texture->pool.Finalize();
  IM_FREE(texture->allocation);
  if (texture->source) {
    texture->source->SetTexID(ImTextureID_Invalid);
    texture->source->BackendUserData = nullptr;
    texture->source->SetStatus(ImTextureStatus_Destroyed);
  }
  if (texture->descriptor >= 0)
    textures_[texture->descriptor] = nullptr;
  IM_DELETE(texture);
}

bool NvnTextureManager::Update(ImTextureData* data) {
  if (data->Status == ImTextureStatus_OK || data->Status == ImTextureStatus_Destroyed)
    return true;
  if (data->Status == ImTextureStatus_WantCreate) {
    if (data->BackendUserData || data->GetTexID() != ImTextureID_Invalid ||
        data->Format != ImTextureFormat_RGBA32 || !data->Pixels ||
        data->Width <= 0 || data->Height <= 0 || data->Width > maxTextureSize_ || data->Height > maxTextureSize_)
      return false;
    int descriptor = firstTexture_;
    while (descriptor < DescriptorCount && textures_[descriptor])
      ++descriptor;
    if (descriptor == DescriptorCount)
      return false;

    auto* texture = IM_NEW(Texture)();
    nvn::TextureBuilder builder;
    builder.SetDefaults().SetDevice(device_).SetTarget(nvn::TextureTarget::TARGET_2D)
        .SetFormat(nvn::Format::RGBA8).SetSize2D(data->Width, data->Height);
    const size_t alignment = builder.GetStorageAlignment() < 0x1000 ? 0x1000 : builder.GetStorageAlignment();
    const size_t size = AlignUp(builder.GetStorageSize(), alignment);
    void* storage = AllocateAligned(size, alignment, texture->allocation);
    if (!storage) {
      Destroy(texture);
      return false;
    }
    memset(storage, 0, size);
    nvn::MemoryPoolBuilder poolBuilder;
    poolBuilder.SetDefaults().SetDevice(device_)
        .SetFlags(nvn::MemoryPoolFlags::CPU_UNCACHED | nvn::MemoryPoolFlags::GPU_CACHED)
        .SetStorage(storage, size);
    if (!(texture->poolReady = texture->pool.Initialize(&poolBuilder))) {
      Destroy(texture);
      return false;
    }
    builder.SetStorage(&texture->pool, 0);
    if (!(texture->textureReady = texture->texture.Initialize(&builder))) {
      Destroy(texture);
      return false;
    }
    const nvn::CopyRegion region = {0, 0, 0, data->Width, data->Height, 1};
    texture->texture.WriteTexelsStrided(nullptr, &region, data->GetPixels(), data->GetPitch(), 0);
    texture->texture.FlushTexels(nullptr, &region);
    texturePool_.RegisterTexture(descriptor, &texture->texture, nullptr);
    texture->source = data;
    texture->descriptor = descriptor;
    textures_[descriptor] = texture;
    data->BackendUserData = texture;
    data->SetTexID(static_cast<ImTextureID>(device_->GetTextureHandle(descriptor, samplerId_)));
    data->SetStatus(ImTextureStatus_OK);
    dirty_ = true;
    return true;
  }

  auto* texture = static_cast<Texture*>(data->BackendUserData);
  if (!texture || texture->source != data)
    return false;
  if (data->Status == ImTextureStatus_WantUpdates) {
    const ImTextureRect& box = data->UpdateRect;
    if (!data->Pixels || data->Format != ImTextureFormat_RGBA32 ||
        int(box.x) + box.w > data->Width || int(box.y) + box.h > data->Height)
      return false;
    if (box.w && box.h) {
      const nvn::CopyRegion region = {box.x, box.y, 0, box.w, box.h, 1};
      // The source remains a full-width atlas even when this rectangle is narrow.
      texture->texture.WriteTexelsStrided(nullptr, &region, data->GetPixelsAt(box.x, box.y), data->GetPitch(), 0);
      texture->texture.FlushTexels(nullptr, &region);
      dirty_ = true;
    }
    data->SetStatus(ImTextureStatus_OK);
  } else if (data->Status == ImTextureStatus_WantDestroy && data->UnusedFrames > 0) {
    Destroy(texture); // WaitIdle already proved that submitted GPU work is complete.
  }
  return true;
}

bool NvnTextureManager::UpdateTextures(ImVector<ImTextureData*>* textures) {
  if (!ready_ || !WaitIdle())
    return false;
  if (textures)
    for (ImTextureData* texture : *textures)
      if (!Update(texture))
        return false;
  return true;
}

void NvnTextureManager::BindPools(nvn::CommandBuffer* commands) {
  commands->SetTexturePool(&texturePool_);
  commands->SetSamplerPool(&samplerPool_);
  if (dirty_) {
    commands->Barrier(nvn::BarrierBits::INVALIDATE_TEXTURE | nvn::BarrierBits::INVALIDATE_TEXTURE_DESCRIPTOR);
    dirty_ = false;
  }
}

bool NvnTextureManager::Shutdown() {
  if (!WaitIdle())
    return false;
  for (Texture* texture : textures_)
    if (texture)
      Destroy(texture);
  if (samplerReady_) sampler_.Finalize();
  if (texturePoolReady_) texturePool_.Finalize();
  if (samplerPoolReady_) samplerPool_.Finalize();
  if (memoryReady_) descriptorMemory_.Finalize();
  if (fenceReady_) fence_.Finalize();
  IM_FREE(descriptorAllocation_);
  descriptorAllocation_ = nullptr;
  samplerReady_ = texturePoolReady_ = samplerPoolReady_ = memoryReady_ = fenceReady_ = ready_ = dirty_ = false;
  return true;
}

} // namespace ImguiNvnBackend
