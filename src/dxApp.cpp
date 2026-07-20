#include "dxApp.h"
#include "dxHelper.h"
#include "hdrManager.h"

#include <filesystem>
#include <iostream>
#include <string>

using Microsoft::WRL::ComPtr;
namespace DXApp
{
void App::onInit()
{
  loadPipeline();
  loadAssets();
  initImgui();
}

void App::initImgui()
{
  imguiInstance = std::make_unique<ImguiInstance>();
  imguiInstance->initImgui(this);
}

void App::loadPipeline()
{
  m_dxgiFactoryFlags = 0;
#if defined(_DEBUG)
  // enable the d3d12 debug layer.
  {
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
      debugController->EnableDebugLayer();
      // Enable additional debug layers.
      m_dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
  }
#endif

  // hdr
  hdrManager                                 = std::make_unique<HDR::HDRManager>();
  hdrManager->colorInfo.hdrGraphicsSupported = true; //@todo any requirements?

  // create window
  windowAPI = std::make_unique<Win32API::Win32Window>();
  windowAPI->init(L"DirectX 12 App", hdrManager.get());

  windowAPI->getFramebufferSize(m_width, m_height);
  m_aspectRatio = static_cast<float>(m_height) / static_cast<float>(m_width);

  // init more members
  m_frameIndex        = 0;
  m_viewport          = D3D12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
  m_scissorRect       = D3D12_RECT(0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height));
  m_rtvDescriptorSize = UINT(0);
  m_srvDescriptorSize = UINT(0);

  // create device
  // ComPtr<IDXGIFactory4> factory;
  ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory)));

  ComPtr<IDXGIAdapter1> hardwareAdapter;
  GetHardwareAdapter(m_dxgiFactory.Get(), &hardwareAdapter);

  ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

  // Describe and create the command queue.
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;

  ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

  // Describe and create the swap chain.
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.BufferCount           = FRAMECOUNT;
  swapChainDesc.Width                 = m_width;
  swapChainDesc.Height                = m_height;
  swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.SampleDesc.Count      = 1;
  selectFormatAndColorSpace(); // @HDR
  swapChainDesc.Format = currentFormat;
  {
    BOOL                  allowTearing = FALSE;
    ComPtr<IDXGIFactory6> factory;
    HRESULT               hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr))
    {
      hr = factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
      m_tearingSupport = SUCCEEDED(hr) && allowTearing;
    }
  }
  swapChainDesc.Flags = m_tearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
  ComPtr<IDXGISwapChain1> swapChain;
  ThrowIfFailed(m_dxgiFactory->CreateSwapChainForHwnd(
      m_commandQueue.Get(), // Swap chain needs the queue so that it can force a flush on it.
      windowAPI->getHwnd(),
      &swapChainDesc,
      nullptr,
      nullptr,
      &swapChain));

  // This sample does not support fullscreen transitions.
  ThrowIfFailed(m_dxgiFactory->MakeWindowAssociation(windowAPI->getHwnd(), DXGI_MWA_NO_ALT_ENTER));

  ThrowIfFailed(swapChain.As(&m_swapChain));
  // check hdr sample for additional display and color space code at this position
  ThrowIfFailed(m_swapChain->SetColorSpace1(currentColorspace));
  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

  // Create descriptor heaps.
  {
    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors             = FRAMECOUNT;
    rtvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  }
  {
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    const int APP_SRV_HEAP_SIZE            = 64;
    srvHeapDesc.NumDescriptors             = APP_SRV_HEAP_SIZE;
    srvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)) != S_OK);
    descHeapAlloc.Create(m_device.Get(), m_srvHeap.Get());
    m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }

  // Create frame resources.
  {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    // Create a RTV for each frame.
    for (UINT n = 0; n < FRAMECOUNT; n++)
    {
      ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
      m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
      rtvHandle.Offset(1, m_rtvDescriptorSize);
    }
  }

  ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void App::loadAssets()
{
  // Create an empty root signature.
  {
    D3D12_ROOT_PARAMETER rootParameters[1]     = {};
    rootParameters[0].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].Constants.ShaderRegister = 0;
    rootParameters[0].Constants.RegisterSpace  = 0;
    rootParameters[0].Constants.Num32BitValues = sizeof(PushConstants) / 4; // convert byte to DWORD
    rootParameters[0].ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(1, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(m_device->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
  }

  // Create the pipeline state, which includes compiling and loading shaders.
  {
    UINT8 *pVertexShaderData      = nullptr;
    UINT8 *pPixelShaderData       = nullptr;
    UINT   vertexShaderDataLength = 0;
    UINT   pixelShaderDataLength  = 0;

    ThrowIfFailed(
        ReadDataFromFile(getAssetFullPath(L"slang-vs.dxil").c_str(), &pVertexShaderData, &vertexShaderDataLength));
    ThrowIfFailed(
        ReadDataFromFile(getAssetFullPath(L"slang-ps.dxil").c_str(), &pPixelShaderData, &pixelShaderDataLength));

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout                        = {nullptr, 0};
    psoDesc.pRootSignature                     = m_rootSignature.Get();
    psoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(pVertexShaderData, vertexShaderDataLength);
    psoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(pPixelShaderData, pixelShaderDataLength);
    psoDesc.RasterizerState                    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode           = D3D12_CULL_MODE_FRONT; // left-handed *facepalm*
    psoDesc.BlendState                         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable      = FALSE;
    psoDesc.DepthStencilState.StencilEnable    = FALSE;
    psoDesc.SampleMask                         = UINT_MAX;
    psoDesc.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets                   = 1;
    psoDesc.RTVFormats[0]                      = sdrPreferredFormat;
    psoDesc.SampleDesc.Count                   = 1;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateSDR)));
    psoDesc.RTVFormats[0] = hdrPreferredFormat;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateHDR)));
  }

  // Create the command list.
  ThrowIfFailed(m_device->CreateCommandList(
      0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), getPipelineState(), IID_PPV_ARGS(&m_commandList)));

  // Command lists are created in the recording state, but there is nothing
  // to record yet. The main loop expects it to be closed, so close it now.
  ThrowIfFailed(m_commandList->Close());

  // Create synchronization objects and wait until assets have been uploaded to the GPU.
  {
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;

    // Create an event handle to use for frame synchronization.
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
      ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    // Wait for the command list to execute; we are reusing the same command
    // list in our main loop but for now, we just want to wait for setup to
    // complete before continuing.
    waitForPreviousFrame();
  }
}

// Update frame-based values.
void App::onUpdate()
{
  hdrManager->updateHDRVariables(); // @todo we only spam this because we lack a user button callback
  windowAPI->updateState();
  setFullscreenState();

  bool swapChainResize = windowAPI->getState().swapChainInvalid; // only cares for resize not csp;
  bool colorUpdate     = windowAPI->getState().swapChainInvalid || m_dxgiFactory->IsCurrent() == false ||
                         hdrManager->colorInfo.swapChainInvalid;
  // check for window resize, window movement, backbuffer format changes.
  // &/or WM_SIZE WM_DISPLAYCHANGE WM_MOVE
  if (swapChainResize || colorUpdate)
  {
    std::cout << "Something changed..." << swapChainResize << " " << colorUpdate << " "
              << (m_dxgiFactory->IsCurrent() == false) << std::endl;
    if (swapChainResize)
    {
      windowAPI->getFramebufferSize(m_width, m_height);
      m_aspectRatio = static_cast<float>(m_height) / static_cast<float>(m_width);
      m_viewport    = D3D12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
      m_scissorRect = D3D12_RECT(0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height));
    }
    // no special operations needed for window_moved only hdr + swapchain
    if (m_dxgiFactory->IsCurrent() == false)
    {
      ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory)));
    }
    // update swapchain
    selectFormatAndColorSpace();
    // @todo do we really update all the swapchain resources and imgui with tiny changes like move?
    UpdateSwapChainBuffer(m_width, m_height, currentFormat, currentColorspace);
    imguiInstance->updateImgui();
  }
}
void App::selectFormatAndColorSpace()
{
  bool useHDRFormat                   = hdrManager->colorInfo.hdrPossible;
  currentFormat                       = useHDRFormat ? hdrPreferredFormat : sdrPreferredFormat;
  currentColorspace                   = useHDRFormat ? hdrPreferredColorspace : sdrPreferredColorspace;
  hdrManager->colorInfo.hdrGameActive = useHDRFormat;
  updateHDRVariables(currentFormat, currentColorspace); // @HDR
}

void App::UpdateSwapChainBuffer(UINT width, UINT height, DXGI_FORMAT format, DXGI_COLOR_SPACE_TYPE csp)
{
  if (!m_swapChain)
  {
    return;
  }

  // Flush all current GPU commands.
  // waitForPreviousFrame(); for now we dont need to wait here, because of waiting at the end of rendering

  // Release the resources holding references to the swap chain (requirement of
  // IDXGISwapChain::ResizeBuffers)
  for (UINT n = 0; n < FRAMECOUNT; n++)
  {
    m_renderTargets[n].Reset();
  }
  // Resize the swap chain to the desired dimensions.
  DXGI_SWAP_CHAIN_DESC1 desc = {};
  m_swapChain->GetDesc1(&desc);
  ThrowIfFailed(m_swapChain->ResizeBuffers(FRAMECOUNT, width, height, format, desc.Flags));

  // check directx hdr sample for additional display and color space code at this position
  // @todo technically we need to test for colorspace support, but scrgb should always be supported
  // because windows uses it as CCCS
  ThrowIfFailed(m_swapChain->SetColorSpace1(csp));

  // Reset the frame index to the current back buffer index.
  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

  // update resources
  // Create frame resources.
  {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    // Create a RTV for each frame.
    for (UINT n = 0; n < FRAMECOUNT; n++)
    {
      ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
      m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
      rtvHandle.Offset(1, m_rtvDescriptorSize);
    }
  }
}

// Render the scene.
void App::onRender()
{
  imguiInstance->imguiStartFrame();
  // Record all the commands we need to render the scene into the command list.
  populateCommandList();

  // Execute the command list.
  ID3D12CommandList *ppCommandLists[] = {m_commandList.Get()};
  m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

  // Present the frame.
  windowAPI->preparePresent();
  ThrowIfFailed(m_swapChain->Present(1, 0));

  waitForPreviousFrame();
}

void App::onDestroy()
{
  // Ensure that the GPU is no longer referencing resources that are about to be
  // cleaned up by the destructor.
  waitForPreviousFrame();

  imguiInstance->cleanupImgui();
  imguiInstance.reset();
  imguiInstance = nullptr;

  CloseHandle(m_fenceEvent);

  windowAPI->terminate();
  windowAPI.reset();
  windowAPI = nullptr;

  hdrManager.reset();
  hdrManager = nullptr;
}

void App::populateCommandList()
{
  // Command list allocators can only be reset when the associated
  // command lists have finished execution on the GPU; apps should use
  // fences to determine GPU execution progress.
  ThrowIfFailed(m_commandAllocator->Reset());

  // However, when ExecuteCommandList() is called on a particular command
  // list, that command list can then be reset at any time and must be before
  // re-recording.
  ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), getPipelineState()));

  // Set necessary state.
  m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

  PushConstants pc  = {};
  pc.colorspaceEnum = hdrManager->colorInfo.colorspaceEnum;
  pc.formatEnum     = hdrManager->colorInfo.formatEnum;
  pc.formatBits     = hdrManager->colorInfo.formatBits;
  pc.formatSrgb     = hdrManager->colorInfo.formatSrgb;
  pc.appMaxCLL      = hdrManager->colorInfo.appMaxCLL;
  pc.appWhitepoint  = hdrManager->colorInfo.appWhitepoint;
  pc.leftHanded     = true;
  pc.screenHeight   = m_height;
  pc.screenWidth    = m_width;
  m_commandList->SetGraphicsRoot32BitConstants(0, sizeof(PushConstants) / 4, &pc, 0);

  m_commandList->RSSetViewports(1, &m_viewport);
  m_commandList->RSSetScissorRects(1, &m_scissorRect);

  // Indicate that the back buffer will be used as a render target.
  auto pBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  m_commandList->ResourceBarrier(1, &pBarrier);

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
      m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);

  m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

  // Record commands.
  const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
  m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_commandList->DrawInstanced(3, 1, 0, 0);
  m_commandList->SetDescriptorHeaps(1, m_srvHeap.GetAddressOf()); // imgui
  imguiInstance->imguiRender();
  // Indicate that the back buffer will now be used to present.
  auto pBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  m_commandList->ResourceBarrier(1, &pBarrier2);

  ThrowIfFailed(m_commandList->Close());
}

// Helper function for acquiring the first available hardware adapter that
// supports Direct3D 12. If no such adapter can be found, *ppAdapter will be set
// to nullptr.
_Use_decl_annotations_ void App::GetHardwareAdapter(IDXGIFactory1 *pFactory, IDXGIAdapter1 **ppAdapter,
                                                    bool requestHighPerformanceAdapter)
{
  *ppAdapter = nullptr;

  ComPtr<IDXGIAdapter1> adapter;

  ComPtr<IDXGIFactory6> factory6;
  if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
  {
    for (UINT adapterIndex = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(
             adapterIndex,
             requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                                                   : DXGI_GPU_PREFERENCE_UNSPECIFIED,
             IID_PPV_ARGS(&adapter)));
         ++adapterIndex)
    {
      DXGI_ADAPTER_DESC1 desc;
      adapter->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
      {
        // Don't select the Basic Render Driver adapter.
        // If you want a software adapter, pass in "/warp" on the command line.
        continue;
      }

      // Check to see whether the adapter supports Direct3D 12, but don't create
      // the actual device yet.
      if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
      {
        break;
      }
    }
  }

  if (adapter.Get() == nullptr)
  {
    for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
    {
      DXGI_ADAPTER_DESC1 desc;
      adapter->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
      {
        // Don't select the Basic Render Driver adapter.
        // If you want a software adapter, pass in "/warp" on the command line.
        continue;
      }

      // Check to see whether the adapter supports Direct3D 12, but don't create
      // the actual device yet.
      if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
      {
        break;
      }
    }
  }

  *ppAdapter = adapter.Detach();
}

std::wstring App::getAssetFullPath(LPCWSTR assetName)
{
  std::wstring wstr = std::filesystem::path(executableDirectory).wstring() + L"/shaders/" + assetName;
  std::wcout << wstr << std::endl;
  return wstr;
}
ID3D12PipelineState *App::getPipelineState()
{
  if (currentFormat == hdrPreferredFormat)
    return m_pipelineStateHDR.Get();
  else
    return m_pipelineStateSDR.Get();
}

void App::setFullscreenState()
{
  if (windowAPI->getState().is_fullscreen == windowAPI->getState().want_fullscreen)
    return; // we dont need to update

  if (m_tearingSupport)
  {

    std::cout << "Changing to tearing fullscreen mode: " << windowAPI->getState().want_fullscreen << std::endl;
    windowAPI->setFullScreenState();
  }
  if (!m_tearingSupport)
  {
    std::cout << "Changing to non-tearing fullscreen mode: " << windowAPI->getState().want_fullscreen << std::endl;
    BOOL fullscreenState;
    ThrowIfFailed(m_swapChain->GetFullscreenState(&fullscreenState, nullptr));
    assert(bool(fullscreenState) == windowAPI->getState().is_fullscreen);
    if (FAILED(m_swapChain->SetFullscreenState(windowAPI->getState().want_fullscreen, nullptr)))
    {
      // Transitions to fullscreen mode can fail when running apps over
      // terminal services or for some other unexpected reason.  Consider
      // notifying the user in some way when this happens.
      std::cout << "WARN: Fullscreen transition failed!" << std::endl;
      assert(false);
    }
  }

  windowAPI->getState().is_fullscreen = windowAPI->getState().want_fullscreen;
}

void App::waitForPreviousFrame()
{
  // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
  // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
  // sample illustrates how to use fences for efficient resource usage and to
  // maximize GPU utilization.

  // Signal and increment the fence value.
  const UINT64 fence = m_fenceValue;
  ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
  m_fenceValue++;

  // Wait until the previous frame is finished.
  if (m_fence->GetCompletedValue() < fence)
  {
    ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
  }

  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void App::mainLoop()
{
  while (windowAPI->dispatch() && !windowAPI->windowShouldClose())
  {
    if (windowAPI->getState().frame_done) // only draw new frame if compositor is ready
    {
      onUpdate();
      onRender();
    }
  }
  waitForPreviousFrame();
}
} // namespace DXApp