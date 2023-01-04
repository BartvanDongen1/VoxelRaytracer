#include "rendering\graphics.h"
#include "window.h"
#include "helper.h"

using namespace Microsoft::WRL;

Graphics::Graphics()
{
    constantBuffer = new ConstantBuffer();
}

Graphics::~Graphics()
{
    delete constantBuffer;
}

void Graphics::init()
{
	loadPipeline();
	loadAssets();

	initImGui();
}

void Graphics::shutdown()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    waitForGpu();

    // imgui cleanup
    if (pd3dSrvDescHeap) { pd3dSrvDescHeap->Release(); pd3dSrvDescHeap = NULL; }

    CloseHandle(fenceEvent);
}

void Graphics::beginFrame()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(commandAllocators[frameIndex]->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), nullptr));

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    // Indicate that the back buffer will be used as a render target.
    auto ResourceBarrier1 = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &ResourceBarrier1);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

void Graphics::endFrame()
{
    // Indicate that the back buffer will now be used to present.
    auto ResourceBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &ResourceBarrier2);

    ThrowIfFailed(commandList->Close());

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(swapChain->Present(0, 0));

    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = fenceValues[frameIndex];
    ThrowIfFailed(commandQueue->Signal(fence.Get(), currentFenceValue));

    // Update the frame index.
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (fence->GetCompletedValue() < fenceValues[frameIndex])
    {
        ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
        WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    fenceValues[frameIndex] = currentFenceValue + 1;
}

void Graphics::renderFrame()
{
    //compute shader for raytracing
    updateConstantBuffer();

    ID3D12DescriptorHeap* pHeaps[] = { cbvSrvUavHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(pHeaps), pHeaps);

    commandList->SetComputeRootSignature(computeRootSignature.Get());

    commandList->SetComputeRootDescriptorTable(0, cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());

    CD3DX12_GPU_DESCRIPTOR_HANDLE descriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 1 + frameIndex, cbvSrvUavDescriptorSize);
    commandList->SetComputeRootDescriptorTable(1, descriptorHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE SceneTextureDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 3, cbvSrvUavDescriptorSize);
    commandList->SetComputeRootDescriptorTable(2, SceneTextureDescriptorHandle);

    commandList->SetPipelineState(computePipelineState.Get());
    commandList->Dispatch(threadGroupX, threadGroupY, 1);
}

void Graphics::renderImGui(int aFPS)
{
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    bool windowOpen = true;

    ImGui::Begin("settings", &windowOpen, ImGuiWindowFlags_None);
    ImGui::Text("current FPS: %i", aFPS);
    ImGui::End();

    // Rendering
    ImGui::Render();

    // Render Dear ImGui graphics
    commandList->SetDescriptorHeaps(1, &pd3dSrvDescHeap);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

    // Update and Render additional Platform Windows
    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(NULL, (void*)commandList.Get());
    }
}

void Graphics::copyComputeTextureToBackbuffer()
{
    // transition resources to get coppied
    TransitionResource(commandList.Get(), renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionResource(commandList.Get(), computeTexture[frameIndex].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

    commandList.Get()->CopyResource(renderTargets[frameIndex].Get(), computeTexture[frameIndex].Get());

    // transition resources back to original state
    TransitionResource(commandList.Get(), renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(commandList.Get(), computeTexture[frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void Graphics::updateCameraVariables(Camera& aCamera)
{
    constantBuffer->camPosition = glm::vec4(aCamera.position, 0.f);
    constantBuffer->camDirection = glm::vec4(aCamera.getDirection(), 0.f);
    constantBuffer->camUpperLeftCorner = glm::vec4(aCamera.getUpperLeftCorner(), 0.f);

    constantBuffer->camPixelOffsetHorizontal = glm::vec4(aCamera.getPixelOffsetHorizontal(), 0.f);
    constantBuffer->camPixelOffsetVertical = glm::vec4(aCamera.getPixelOffsetVertical(), 0.f);
}

void Graphics::updateOctreeVariables(const VoxelModel& aModel)
{
    assert(aModel.sizeX == 32 && aModel.sizeY == 32 && aModel.sizeZ == 32);

    ThrowIfFailed(commandAllocators[frameIndex]->Reset());
    ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), nullptr));

    //transition texture
    auto myResourceBarrierBefore = CD3DX12_RESOURCE_BARRIER::Transition(sceneDataTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->ResourceBarrier(1, &myResourceBarrierBefore);

    //copy over data
    ComPtr<ID3D12Resource> textureUploadHeap;

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(sceneDataTexture.Get(), 0, 1);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Create the GPU upload buffer.
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureUploadHeap)));

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = aModel.data;
    textureData.RowPitch = static_cast<long>(aModel.sizeX) * 4;
    textureData.SlicePitch = textureData.RowPitch * aModel.sizeY;

    UpdateSubresources(commandList.Get(), sceneDataTexture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);

    //transition back
    auto myResourceBarrierAfter = CD3DX12_RESOURCE_BARRIER::Transition(sceneDataTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &myResourceBarrierAfter);

    ThrowIfFailed(commandList->Close());

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    waitForGpu();
}

void Graphics::updateConstantBuffer()
{
    constantBuffer->maxThreadIter = glm::vec4(Window::getWidth(), Window::getHeight(), 0, 0);
    
    memcpy(pCbvDataBegin, constantBuffer, sizeof(ConstantBuffer));
}

bool Graphics::loadPipeline()
{
    frameIndex = 0;

    viewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, static_cast<float>(Window::getWidth()), static_cast<float>(Window::getHeight()) };
    scissorRect = CD3DX12_RECT{ 0, 0, static_cast<LONG>(Window::getWidth()), static_cast<LONG>(Window::getHeight()) };
    rtvDescriptorSize = 0;

    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device)
        ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device)
        ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FRAME_COUNT;
    swapChainDesc.Width = Window::getWidth();
    swapChainDesc.Height = Window::getHeight();
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> mySwapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Window::getHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &mySwapChain
    ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Window::getHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(mySwapChain.As(&swapChain));
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FRAME_COUNT;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));

        rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Describe and create a Unordered Access View (UAV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = 4;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&cbvSrvUavHeap)));

        cbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT i = 0; i < FRAME_COUNT; i++)
        {
            ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])));
            device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, rtvDescriptorSize);

            ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i])));
        }
    }

    // create ImGui descriptor heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 2;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pd3dSrvDescHeap)) != S_OK)
            return false;
    }

    return true;
}

void Graphics::loadAssets()
{
    // Create the command list.
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex].Get(), nullptr, IID_PPV_ARGS(&commandList)));
    ThrowIfFailed(commandList->Close());

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(device->CreateFence(fenceValues[frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        fenceValues[frameIndex]++;

        // Create an event handle to use for frame synchronization.
        fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        waitForGpu();
    }

    loadComputeStage();
}

void Graphics::loadComputeStage()
{
    //create out textures for the compute shader
    const D3D12_HEAP_PROPERTIES myDefaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC myTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1920, 1080, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ThrowIfFailed(
        device->CreateCommittedResource(
            &myDefaultHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &myTextureDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            IID_PPV_ARGS(computeTexture[0].ReleaseAndGetAddressOf())));
    computeTexture[0]->SetName(L"compute Texture 0");

    ThrowIfFailed(
        device->CreateCommittedResource(
            &myDefaultHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &myTextureDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            IID_PPV_ARGS(computeTexture[1].ReleaseAndGetAddressOf())));
    computeTexture[1]->SetName(L"compute Texture 1");

    threadGroupX = static_cast<uint32_t>(myTextureDesc.Width) / SHADER_THREAD_COUNT;
    threadGroupY = myTextureDesc.Height / SHADER_THREAD_COUNT;

    // create uav
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle1(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 1, cbvSrvUavDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle2(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 2, cbvSrvUavDescriptorSize);
    device->CreateUnorderedAccessView(computeTexture[0].Get(), nullptr, nullptr, srvHandle1);
    device->CreateUnorderedAccessView(computeTexture[1].Get(), nullptr, nullptr, srvHandle2);

    //create 3D texture for the scene
    {
        const D3D12_RESOURCE_DESC myTexture3DDesc = CD3DX12_RESOURCE_DESC::Tex3D(DXGI_FORMAT_R32_UINT, 32, 32, 32);
        ThrowIfFailed(
            device->CreateCommittedResource(
                &myDefaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &myTexture3DDesc,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                nullptr,
                IID_PPV_ARGS(sceneDataTexture.ReleaseAndGetAddressOf())));
        sceneDataTexture->SetName(L"Scene Data Texture");

        D3D12_SHADER_RESOURCE_VIEW_DESC mySceneDataDesc = {};
        mySceneDataDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        mySceneDataDesc.Format = myTexture3DDesc.Format;
        mySceneDataDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        mySceneDataDesc.Texture3D.MipLevels = 1;

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle3(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 3, cbvSrvUavDescriptorSize);
        device->CreateShaderResourceView(sceneDataTexture.Get(), &mySceneDataDesc, srvHandle3);
    }

    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    //D3D_SHADER_MACRO macros[] = { "TEST", "1", NULL, NULL };
    D3D_SHADER_MACRO macros[] = { NULL, NULL };

    ID3DBlob* errorBlob = nullptr;
    ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/raytraceCompute.hlsl", macros, nullptr, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/rayDirToColor.hlsl", macros, nullptr, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));

    CD3DX12_DESCRIPTOR_RANGE1 myRanges[3];
    myRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    myRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    myRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 myRootParameters[3];
    myRootParameters[0].InitAsDescriptorTable(1, &myRanges[0], D3D12_SHADER_VISIBILITY_ALL);
    myRootParameters[1].InitAsDescriptorTable(1, &myRanges[1], D3D12_SHADER_VISIBILITY_ALL);
    myRootParameters[2].InitAsDescriptorTable(1, &myRanges[2], D3D12_SHADER_VISIBILITY_ALL);

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // Allow input layout and deny uneccessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(myRootParameters), myRootParameters, 0, nullptr, rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &globalErrorBlob));
    ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature)));

    // Create compute pipeline state
    D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
    descComputePSO.pRootSignature = computeRootSignature.Get();
    descComputePSO.CS.pShaderBytecode = computeShader.Get()->GetBufferPointer();
    descComputePSO.CS.BytecodeLength = computeShader.Get()->GetBufferSize();

    ThrowIfFailed(device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(computePipelineState.ReleaseAndGetAddressOf())));
    computePipelineState->SetName(L"Compute PSO");

    auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBuffer));

    ThrowIfFailed(device->CreateCommittedResource(
        &heapUpload,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&computeConstantBuffer)));

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(computeConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pCbvDataBegin)));

    // Describe and create a constant buffer view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC myBufferDesc{};

    myBufferDesc.BufferLocation = computeConstantBuffer->GetGPUVirtualAddress();
    myBufferDesc.SizeInBytes = static_cast<UINT>(sizeof(ConstantBuffer));

    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 0, cbvSrvUavDescriptorSize);
    device->CreateConstantBufferView(&myBufferDesc, cbvHandle);
}

void Graphics::initImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO(); (void)io;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    //io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    //io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(Window::getHwnd());
    ImGui_ImplDX12_Init(device.Get(), FRAME_COUNT,
        DXGI_FORMAT_R8G8B8A8_UNORM, pd3dSrvDescHeap,
        pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

}

void Graphics::waitForGpu()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValues[frameIndex]));

    // Wait until the fence has been processed.
    ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
    WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);

    // Increment the fence value for the current frame.
    fenceValues[frameIndex]++;
}

void Graphics::GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (
            UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
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

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
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

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
}
