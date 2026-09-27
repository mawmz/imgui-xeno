#include "MemoryBuffer.h"
#include "imgui_impl_nvn.hpp"
#include "logger/Logger.hpp"
#include "helpers/memoryHelper.h"

MemoryBuffer::MemoryBuffer(size_t size)
    : MemoryBuffer(size, nvn::MemoryPoolFlags::CPU_UNCACHED | nvn::MemoryPoolFlags::GPU_CACHED) {}

MemoryBuffer::MemoryBuffer(size_t size, nvn::MemoryPoolFlags flags) {
  auto* bd = ImguiNvnBackend::getBackendData();
  const size_t alignedSize = ALIGN_UP(size, 0x1000);
  memBuffer = Mem::AllocateAlign(0x1000, alignedSize);
  if (!memBuffer)
    return;
  memset(memBuffer, 0, alignedSize);

  bd->memPoolBuilder.SetDefaults().SetDevice(bd->device).SetFlags(flags).SetStorage(memBuffer, alignedSize);
  if (!(poolReady = pool.Initialize(&bd->memPoolBuilder))) {
    Logger::log("Failed to Create Memory Pool!\n");
    Finalize();
    return;
  }
  bd->bufferBuilder.SetDefaults().SetDevice(bd->device).SetStorage(&pool, 0, alignedSize);
  if (!(mIsReady = buffer.Initialize(&bd->bufferBuilder))) {
    Logger::log("Failed to Init Buffer!\n");
    Finalize();
  }
}

MemoryBuffer::MemoryBuffer(size_t size, void* bufferPtr, nvn::MemoryPoolFlags flags)
    : MemoryBuffer(size, flags) {
  if (mIsReady)
    memcpy(memBuffer, bufferPtr, size);
}

void MemoryBuffer::Finalize() {
  // The caller must wait for GPU completion before releasing this buffer.
  if (mIsReady) buffer.Finalize();
  if (poolReady) pool.Finalize();
  Mem::Deallocate(memBuffer);
  memBuffer = nullptr;
  mIsReady = poolReady = false;
}

void MemoryBuffer::ClearBuffer() {
  if (mIsReady)
    memset(memBuffer, 0, pool.GetSize());
}
