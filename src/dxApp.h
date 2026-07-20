#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <d3dx12.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <imgui.h>
#include <string>

#include "dxImgui.h"
#include "hdrManager.h"
#include "win32Window.h"

namespace DXApp
{

struct PushConstants
{
  alignas(4) int colorspaceEnum;
  alignas(4) int formatEnum;
  alignas(4) int formatBits;
  alignas(4) int formatSrgb;
  alignas(4) float appMaxCLL;
  alignas(4) float appWhitepoint;
  alignas(4) bool leftHanded;

  alignas(4) uint32_t screenWidth;
  alignas(4) uint32_t screenHeight;
};

// simple free list based allocator, only relevant for imgui
struct ExampleDescriptorHeapAllocator
{
  ID3D12DescriptorHeap       *Heap     = nullptr;
  D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
  D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
  D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
  UINT                        HeapHandleIncrement;
  ImVector<int>               FreeIndices;

  void Create(ID3D12Device *device, ID3D12DescriptorHeap *heap)
  {
    IM_ASSERT(Heap == nullptr && FreeIndices.empty());
    Heap                            = heap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    HeapType                        = desc.Type;
    HeapStartCpu                    = Heap->GetCPUDescriptorHandleForHeapStart();
    HeapStartGpu                    = Heap->GetGPUDescriptorHandleForHeapStart();
    HeapHandleIncrement             = device->GetDescriptorHandleIncrementSize(HeapType);
    FreeIndices.reserve((int)desc.NumDescriptors);
    for (int n = desc.NumDescriptors; n > 0; n--)
      FreeIndices.push_back(n - 1);
  }
  void Destroy()
  {
    Heap = nullptr;
    FreeIndices.clear();
  }
  void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_desc_handle)
  {
    IM_ASSERT(FreeIndices.Size > 0);
    int idx = FreeIndices.back();
    FreeIndices.pop_back();
    out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
    out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
  }
  void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
  {
    int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
    int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
    IM_ASSERT(cpu_idx == gpu_idx);
    FreeIndices.push_back(cpu_idx);
  }
};

class App
{
public:
  ExampleDescriptorHeapAllocator descHeapAlloc;

  void run(std::string path)
  {
    executableDirectory = path;
    onInit();
    mainLoop();
    onDestroy();
  }

  Win32API::Win32Window *getWindowAPI()
  {
    return windowAPI.get();
  }
  int getSwapChainWidth()
  {
    return m_width;
  }
  int getSwapChainHeight()
  {
    return m_height;
  }
  Microsoft::WRL::ComPtr<ID3D12Device> &getDevice()
  {
    return m_device;
  }
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> &getCommandQueue()
  {
    return m_commandQueue;
  }
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> &getCommandList()
  {
    return m_commandList;
  }
  UINT getFrameCount()
  {
    return FRAMECOUNT;
  }
  DXGI_FORMAT getCurrentFormat()
  {
    return currentFormat;
  }
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &getSrvDescriptorHeap()
  {
    return m_srvHeap;
  }
  HDR::ColorInfo &getColorInfo()
  {
    return hdrManager->colorInfo;
  }

private:
  std::string       executableDirectory;
  static const UINT FRAMECOUNT = 2;
  int               m_width;
  int               m_height;
  float             m_aspectRatio;

  // Pipeline objects.
  std::unique_ptr<Win32API::Win32Window>            windowAPI;
  D3D12_VIEWPORT                                    m_viewport;
  D3D12_RECT                                        m_scissorRect;
  Microsoft::WRL::ComPtr<IDXGISwapChain3>           m_swapChain;
  Microsoft::WRL::ComPtr<IDXGIFactory4>             m_dxgiFactory;
  Microsoft::WRL::ComPtr<ID3D12Device>              m_device;
  Microsoft::WRL::ComPtr<ID3D12Resource>            m_renderTargets[FRAMECOUNT];
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_commandAllocator;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue>        m_commandQueue;
  Microsoft::WRL::ComPtr<ID3D12RootSignature>       m_rootSignature;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_rtvHeap;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_srvHeap;
  Microsoft::WRL::ComPtr<ID3D12PipelineState>       m_pipelineStateSDR;
  Microsoft::WRL::ComPtr<ID3D12PipelineState>       m_pipelineStateHDR;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
  UINT                                              m_rtvDescriptorSize;
  UINT                                              m_srvDescriptorSize;

  // Synchronization objects.
  UINT                                m_frameIndex;
  HANDLE                              m_fenceEvent;
  Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
  UINT64                              m_fenceValue;

  // Other
  UINT m_dxgiFactoryFlags;
  bool m_tearingSupport = false;

  // HDR

  const DXGI_FORMAT                sdrPreferredFormat     = DXGI_FORMAT_R8G8B8A8_UNORM;
  const DXGI_COLOR_SPACE_TYPE      sdrPreferredColorspace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  const DXGI_FORMAT                hdrPreferredFormat     = DXGI_FORMAT_R16G16B16A16_FLOAT;
  const DXGI_COLOR_SPACE_TYPE      hdrPreferredColorspace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
  DXGI_FORMAT                      currentFormat          = DXGI_FORMAT_R8G8B8A8_UNORM;
  DXGI_COLOR_SPACE_TYPE            currentColorspace      = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  std::unique_ptr<HDR::HDRManager> hdrManager;
  std::unique_ptr<ImguiInstance>   imguiInstance;

  // 1st Level
  void onInit();
  void mainLoop();
  void onDestroy();

  void onUpdate();
  void onRender();

  // 2nd Level
  void loadPipeline();
  void loadAssets();
  void populateCommandList();
  void waitForPreviousFrame();
  void GetHardwareAdapter(_In_ IDXGIFactory1 *pFactory, _Outptr_result_maybenull_ IDXGIAdapter1 **ppAdapter,
                          bool requestHighPerformanceAdapter = false);
  ID3D12PipelineState *getPipelineState();
  std::wstring         getAssetFullPath(LPCWSTR assetName);
  void                 updateHDRVariables(DXGI_FORMAT format, DXGI_COLOR_SPACE_TYPE csp);
  void                 selectFormatAndColorSpace();
  void                 UpdateSwapChainBuffer(UINT width, UINT height, DXGI_FORMAT format, DXGI_COLOR_SPACE_TYPE csp);
  void                 setFullscreenState();
  void                 initImgui();
};
} // namespace DXApp