#include "rendering\graphics.h"
#include "window.h"
#include "helper.h"
#include "engine\logger.h"
#include "engine\timer.h"

#include "imgui-docking/imgui_impl_dx12.h"
#include "imgui-docking/imgui_impl_win32.h"
#include "rendering\imgui-docking\implot.h"

using namespace Microsoft::WRL;

Graphics::Graphics()
{
    computeConstantBuffer = new ConstantBuffer();
    
    octreeConstantBuffer = new OctreeBuffer();
    
    accumulationConstantBuffer = new AcummulationBuffer();

    voxelGridConstantBuffer = new VoxelGridBuffer();

    profiler = new GPUProfiler();
}

Graphics::~Graphics()
{
    //delete const buffers
    delete computeConstantBuffer;
    delete octreeConstantBuffer;
    delete accumulationConstantBuffer;
    delete voxelGridConstantBuffer;

    delete profiler;
}

void Graphics::init(const unsigned int aSizeX, const unsigned int aSizeY)
{
	loadPipeline();
	loadAssets(aSizeX, aSizeY);

	initImGui();

    profiler->init(device.Get(), commandQueue.Get(), swapChain.Get());
}

void Graphics::shutdown()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    waitForGpu();

    // imgui cleanup
    if (pd3dSrvDescHeap) { pd3dSrvDescHeap->Release(); pd3dSrvDescHeap = NULL; }

    ImPlot::DestroyContext();
    //ImGui::DestroyContext(); this causes an error for some reason

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
    //const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    //commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    profiler->NewFrame();

    profiler->BeginProfileScope(commandList.Get(), "RenderTime");
}

void Graphics::endFrame()
{

    profiler->EndProfileScope("RenderTime");

    profiler->EndFrame();


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
        //Timer myTimer;

        ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
        WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);

        //float myTime = myTimer.getTotalTime() * 1000;
        //LOG_INFO("waited: %f MS, for GPU to finish", myTime);
    }
    else
    {
        //LOG_WARNING("CPU did not have to wait for GPU to complete rendering: %i", fenceValues[frameIndex]);
    }
    
    // Set the fence value for the next frame.
    fenceValues[frameIndex] = currentFenceValue + 1;
}

void Graphics::renderFrame()
{
    updateConstantBuffer();

    ID3D12DescriptorHeap* pHeaps[] = { cbvSrvUavHeap.Get(), samplerHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(pHeaps), pHeaps);

    profiler->BeginProfileScope(commandList.Get(), "main ray");
    
    //compute shader for raytracing
    {
        commandList->SetComputeRootSignature(computeRootSignature.Get());

        // camera const buffer
        CD3DX12_GPU_DESCRIPTOR_HANDLE constBufferDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 3, cbvSrvUavDescriptorSize);
        commandList->SetComputeRootDescriptorTable(0, constBufferDescriptorHandle);

        // output texture
        CD3DX12_GPU_DESCRIPTOR_HANDLE outputTextureDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 0, cbvSrvUavDescriptorSize);
        commandList->SetComputeRootDescriptorTable(1, outputTextureDescriptorHandle);

        // noise texture
        CD3DX12_GPU_DESCRIPTOR_HANDLE noiseTextureDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 2, cbvSrvUavDescriptorSize);
        commandList->SetComputeRootDescriptorTable(2, noiseTextureDescriptorHandle);

        // octree buffer
        CD3DX12_GPU_DESCRIPTOR_HANDLE octreeBufferDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 6, cbvSrvUavDescriptorSize);
        commandList->SetComputeRootDescriptorTable(3, octreeBufferDescriptorHandle);

        // octree const buffer
        CD3DX12_GPU_DESCRIPTOR_HANDLE octreeConstBufferDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 5, cbvSrvUavDescriptorSize);
        commandList->SetComputeRootDescriptorTable(4, octreeConstBufferDescriptorHandle);

        {
            // voxel grid top level buffer
            CD3DX12_GPU_DESCRIPTOR_HANDLE GridTopLayerDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 8, cbvSrvUavDescriptorSize);
            commandList->SetComputeRootDescriptorTable(5, GridTopLayerDescriptorHandle);

            // voxel grid layer 1 buffer
            CD3DX12_GPU_DESCRIPTOR_HANDLE GridLayer1DescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 9, cbvSrvUavDescriptorSize);
            commandList->SetComputeRootDescriptorTable(6, GridLayer1DescriptorHandle);

            // voxel grid layer 2 buffer
            CD3DX12_GPU_DESCRIPTOR_HANDLE GridLayer2DescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 10, cbvSrvUavDescriptorSize);
            commandList->SetComputeRootDescriptorTable(7, GridLayer2DescriptorHandle);

            // voxel grid const buffer
            CD3DX12_GPU_DESCRIPTOR_HANDLE GridConstBufferDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 7, cbvSrvUavDescriptorSize);
            commandList->SetComputeRootDescriptorTable(8, GridConstBufferDescriptorHandle);

            // voxel Atlas buffer
            CD3DX12_GPU_DESCRIPTOR_HANDLE voxelAtlasDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 11, cbvSrvUavDescriptorSize);
            commandList->SetComputeRootDescriptorTable(9, voxelAtlasDescriptorHandle);
        }

        //skydome
        {
            //skydome texture
            CD3DX12_GPU_DESCRIPTOR_HANDLE skydomeTextureDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 12, cbvSrvUavDescriptorSize);
            commandList->SetComputeRootDescriptorTable(10, skydomeTextureDescriptorHandle);

            //skydome sampler
            CD3DX12_GPU_DESCRIPTOR_HANDLE skydomeSamplerDescriptorHandle(samplerHeap->GetGPUDescriptorHandleForHeapStart(), 0, samplerDescriptorSize);
            commandList->SetComputeRootDescriptorTable(11, skydomeSamplerDescriptorHandle);
        }
       
        commandList->SetPipelineState(computePipelineState.Get());
        commandList->Dispatch(threadGroupX, threadGroupY, 1);
    }

    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
    commandList->ResourceBarrier(1, &barrier);

    profiler->EndProfileScope("main ray");

    profiler->BeginProfileScope(commandList.Get(), "accumulation shader");

    //accumulation shader
    {
        commandList->SetComputeRootSignature(accumulationRootSignature.Get());

        // input texture
        CD3DX12_GPU_DESCRIPTOR_HANDLE inputTextureDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 0, cbvSrvUavDescriptorSize);
        commandList->SetComputeRootDescriptorTable(0, inputTextureDescriptorHandle);

        // output texture
        CD3DX12_GPU_DESCRIPTOR_HANDLE outputTextureDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 1, cbvSrvUavDescriptorSize);
        commandList->SetComputeRootDescriptorTable(1, outputTextureDescriptorHandle);

        // accumulation const buffer
        CD3DX12_GPU_DESCRIPTOR_HANDLE accumulationConstBufferDescriptorHandle(cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 4, cbvSrvUavDescriptorSize);
        commandList->SetComputeRootDescriptorTable(2, accumulationConstBufferDescriptorHandle);

        commandList->SetPipelineState(accumulationPipelineState.Get());
        commandList->Dispatch(threadGroupX, threadGroupY, 1);
    }

    commandList->ResourceBarrier(1, &barrier);

    profiler->EndProfileScope("accumulation shader");
}

void Graphics::renderImGui()
{
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

void Graphics::copyAccumulationBufferToBackbuffer()
{
    // transition resources to get coppied
    TransitionResource(commandList.Get(), renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionResource(commandList.Get(), accumulationOutputTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

    commandList.Get()->CopyResource(renderTargets[frameIndex].Get(), accumulationOutputTexture.Get());

    // transition resources back to original state
    TransitionResource(commandList.Get(), renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    TransitionResource(commandList.Get(), accumulationOutputTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

int wangHash(int aSeed)
{
    aSeed = (aSeed ^ 61) ^ (aSeed >> 16);
    aSeed *= 9;
    aSeed = aSeed ^ (aSeed >> 4);
    aSeed *= 0x27d4eb2d;
    aSeed = aSeed ^ (aSeed >> 15);
    return aSeed;
}

static int sFrameCount = 0;

void Graphics::updateCameraVariables(Camera& aCamera, bool aFocussed, int aSize)
{
    octreeConstantBuffer->octreeSize = aSize;

    computeConstantBuffer->frameSeed = wangHash(sFrameCount++);

    computeConstantBuffer->sampleCount = aFocussed == true ? 1 : 1;

    computeConstantBuffer->camPosition = glm::vec4(aCamera.position, 0.f);
    computeConstantBuffer->camDirection = glm::vec4(aCamera.getDirection(), 0.f);
    computeConstantBuffer->camUpperLeftCorner = glm::vec4(aCamera.getUpperLeftCorner(), 0.f);

    computeConstantBuffer->camPixelOffsetHorizontal = glm::vec4(aCamera.getPixelOffsetHorizontal(), 0.f);
    computeConstantBuffer->camPixelOffsetVertical = glm::vec4(aCamera.getPixelOffsetVertical(), 0.f);

    computeConstantBuffer->octreeSize = aSize;
}

void Graphics::updateAccumulationVariables(bool aShouldNotAccumulate)
{
    if (aShouldNotAccumulate)
    {
        accumulationConstantBuffer->framesAccumulated = 1;
        accumulationConstantBuffer->shouldAcummulate = false;
        return;
    }

    accumulationConstantBuffer->framesAccumulated++;
    accumulationConstantBuffer->shouldAcummulate = true;
}

void Graphics::updateNoiseTexture(const Texture& aTexture)
{
    //assert(aTexture.textureWidth == 470 && aTexture.textureHeight == 470);
    //
    //ThrowIfFailed(commandAllocators[frameIndex]->Reset());
    //ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), nullptr));
    //
    ////transition texture
    //auto myResourceBarrierBefore = CD3DX12_RESOURCE_BARRIER::Transition(noiseTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    //commandList->ResourceBarrier(1, &myResourceBarrierBefore);
    //
    ////copy over data
    //ComPtr<ID3D12Resource> textureUploadHeap;
    //
    //const UINT64 uploadBufferSize = GetRequiredIntermediateSize(noiseTexture.Get(), 0, 1);
    //auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    //
    //auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    //
    //// Create the GPU upload buffer.
    //ThrowIfFailed(device->CreateCommittedResource(
    //    &uploadHeapProperties,
    //    D3D12_HEAP_FLAG_NONE,
    //    &resourceDesc,
    //    D3D12_RESOURCE_STATE_GENERIC_READ,
    //    nullptr,
    //    IID_PPV_ARGS(&textureUploadHeap)));
    //
    //D3D12_SUBRESOURCE_DATA textureData = {};
    //textureData.pData = aTexture.textureData;
    //textureData.RowPitch = static_cast<long>(aTexture.textureWidth) * aTexture.bytesPerPixel;
    //textureData.SlicePitch = textureData.RowPitch * aTexture.textureHeight;
    //
    //UpdateSubresources(commandList.Get(), noiseTexture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
    //
    ////transition back
    //auto myResourceBarrierAfter = CD3DX12_RESOURCE_BARRIER::Transition(noiseTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    //commandList->ResourceBarrier(1, &myResourceBarrierAfter);
    //
    //ThrowIfFailed(commandList->Close());
    //
    //// Execute the command list.
    //ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    //commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    //
    //waitForGpu();

    // generate random noise values with the size of the screen
    int* randomValues = reinterpret_cast<int*>(malloc(static_cast<size_t>(sizeX) * sizeY * sizeof(int)));

    assert(randomValues != nullptr);

    int rand = 1;
    for (int i = 0; i < sizeX * sizeY; i++)
    {
        // generate values
        rand = wangHash(rand);
        randomValues[i] = rand;
    }

    void* mappedData;

    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(noiseTexture->Map(0, &readRange, &mappedData));

    // Update the data
    memcpy(mappedData, randomValues, sizeX * sizeY * sizeof(int));

    // Unmap the buffer
    noiseTexture->Unmap(0, &readRange);
}

void Graphics::updateSkydomeTexture(const Texture& aTexture)
{
    assert(aTexture.textureWidth == 4096 && aTexture.textureHeight == 2048);

    ThrowIfFailed(commandAllocators[frameIndex]->Reset());
    ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), nullptr));

    //transition texture
    auto myResourceBarrierBefore = CD3DX12_RESOURCE_BARRIER::Transition(skydomeTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->ResourceBarrier(1, &myResourceBarrierBefore);

    //copy over data
    ComPtr<ID3D12Resource> textureUploadHeap;

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(skydomeTexture.Get(), 0, 1);
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
    textureData.pData = aTexture.textureData;
    textureData.RowPitch = static_cast<long>(aTexture.textureWidth) * aTexture.bytesPerPixel;
    textureData.SlicePitch = textureData.RowPitch * aTexture.textureHeight;

    UpdateSubresources(commandList.Get(), skydomeTexture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);

    //transition back
    auto myResourceBarrierAfter = CD3DX12_RESOURCE_BARRIER::Transition(skydomeTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &myResourceBarrierAfter);

    ThrowIfFailed(commandList->Close());

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    waitForGpu();
}

void Graphics::updateOctreeVariables(const Octree& aOctree)
{
    // Get a pointer to the mapped data
    void* mappedData;

    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(octreeBuffer->Map(0, &readRange, &mappedData));

    size_t test = aOctree.getSize() * sizeof(OctreeElement[8]);

    // Update the data
    memcpy(mappedData, aOctree.getData(), test);

    // Unmap the buffer
    octreeBuffer->Unmap(0, &readRange);

    // set const buffer varaible for ready the octree properly later
    //computeConstantBuffer->octreeLayerCount = aOctree.getLayerCount();
    octreeConstantBuffer->octreeLayerCount = aOctree.getLayerCount();
}

void Graphics::updateVoxelGridVariables(const VoxelGrid& aGrid)
{
    //update top level
    {
        void* mappedData;

        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(voxelGridTopLevelBuffer->Map(0, &readRange, &mappedData));

        size_t test = aGrid.getGridSize() * sizeof(int);

        // Update the data
        memcpy(mappedData, aGrid.getGridData(), test);

        // Unmap the buffer
        voxelGridTopLevelBuffer->Unmap(0, &readRange);
    }

    //update level 1
    {
        void* mappedData;

        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(voxelGridLayer1Buffer->Map(0, &readRange, &mappedData));

        size_t test = aGrid.getLayer1ChunkDataSize() * sizeof(Layer1Chunk);

        // Update the data
        memcpy(mappedData, aGrid.getLayer1ChunkData(), test);

        // Unmap the buffer
        voxelGridLayer1Buffer->Unmap(0, &readRange);
    }

    //update level 2
    {
        void* mappedData;

        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(voxelGridLayer2Buffer->Map(0, &readRange, &mappedData));

        size_t test = aGrid.getLayer2ChunkDataSize() * sizeof(Layer2Chunk);

        // Update the data
        memcpy(mappedData, aGrid.getLayer2ChunkData(), test);

        // Unmap the buffer
        voxelGridLayer2Buffer->Unmap(0, &readRange);
    }

    //update constant buffer
    voxelGridConstantBuffer->voxelGridSize = glm::uvec4(aGrid.getSizeX(), aGrid.getSizeY(), aGrid.getSizeZ(), 0);

    voxelGridConstantBuffer->topLevelChunkSize = voxelGridConstantBuffer->voxelGridSize / glm::uvec4(16);
}

void Graphics::updateVoxelAtlasVariables(const VoxelAtlas& aAtlas)
{
    // Get a pointer to the mapped data
    void* mappedData;

    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(voxelAtlasBuffer->Map(0, &readRange, &mappedData));

    size_t test = aAtlas.getItemCount() * sizeof(VoxelAtlasItem);

    // Update the data
    memcpy(mappedData, aAtlas.getItems(), test);

    // Unmap the buffer
    voxelAtlasBuffer->Unmap(0, &readRange);
}

GPUProfiler* Graphics::getProfiler() const
{
    return profiler;
}

void Graphics::updateConstantBuffer()
{
    memcpy(pOctreeCbvDataBegin, octreeConstantBuffer, sizeof(OctreeBuffer));

    computeConstantBuffer->maxThreadIter = glm::vec4(Window::getWidth(), Window::getHeight(), 0, 0);
    memcpy(pComputeCbvDataBegin, computeConstantBuffer, sizeof(ConstantBuffer));

    memcpy(pAccumulationCbvDataBegin, accumulationConstantBuffer, sizeof(AcummulationBuffer));

    memcpy(pVoxelGridBufferCbvDataBegin, voxelGridConstantBuffer, sizeof(VoxelGridBuffer));
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

    // Does not support fullscreen transitions.
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
        srvHeapDesc.NumDescriptors = 13;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&cbvSrvUavHeap)));

        cbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Describe and create a sampler descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.NumDescriptors = 1;
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&samplerHeap)));

        samplerDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
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

void Graphics::loadAssets(const unsigned int aSizeX, const unsigned int aSizeY)
{
    sizeX = aSizeX;
    sizeY = aSizeY;

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

    loadComputeStage(aSizeX, aSizeY);
    loadAccumulationStage(aSizeX, aSizeY);
}

void Graphics::loadComputeStage(const unsigned int aSizeX, const unsigned int aSizeY)
{
    const D3D12_HEAP_PROPERTIES myDefaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    //create out textures for the compute shader
    {
        const D3D12_RESOURCE_DESC myTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, aSizeX, aSizeY, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        ThrowIfFailed(
            device->CreateCommittedResource(
                &myDefaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &myTextureDesc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                nullptr,
                IID_PPV_ARGS(raytraceOutputTexture.ReleaseAndGetAddressOf())));
        raytraceOutputTexture->SetName(L"compute Texture");

        threadGroupX = static_cast<uint32_t>(myTextureDesc.Width) / SHADER_THREAD_COUNT_X;
        threadGroupY = myTextureDesc.Height / SHADER_THREAD_COUNT_Y;

        // create uav
        CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle1(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 0, cbvSrvUavDescriptorSize);
        device->CreateUnorderedAccessView(raytraceOutputTexture.Get(), nullptr, nullptr, uavHandle1);
    }
   
    ////create noise texture for random values in the shader
    //{
    //    // Describe and create a Texture2D.
    //    D3D12_RESOURCE_DESC textureDesc = {};
    //    textureDesc.MipLevels = 1;
    //    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    //    textureDesc.Width = 470;
    //    textureDesc.Height = 470;
    //    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    //    textureDesc.DepthOrArraySize = 1;
    //    textureDesc.SampleDesc.Count = 1;
    //    textureDesc.SampleDesc.Quality = 0;
    //    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    //
    //    ThrowIfFailed(
    //        device->CreateCommittedResource(
    //            &myDefaultHeapProperties,
    //            D3D12_HEAP_FLAG_NONE,
    //            &textureDesc,
    //            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
    //            nullptr,
    //            IID_PPV_ARGS(noiseTexture.ReleaseAndGetAddressOf())));
    //    noiseTexture->SetName(L"Noise Texture");
    //
    //    D3D12_SHADER_RESOURCE_VIEW_DESC mySceneDataDesc = {};
    //    mySceneDataDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    //    mySceneDataDesc.Format = textureDesc.Format;
    //    mySceneDataDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    //    mySceneDataDesc.Texture3D.MipLevels = 1;
    //
    //    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 2, cbvSrvUavDescriptorSize);
    //    device->CreateShaderResourceView(noiseTexture.Get(), &mySceneDataDesc, srvHandle);
    //}

    //create noise buffer for random values in the shader
    {
        auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        D3D12_RESOURCE_ALLOCATION_INFO myAllocationInfo;
        myAllocationInfo.SizeInBytes = aSizeX * aSizeY * sizeof(int);
        myAllocationInfo.Alignment = 0;

        const D3D12_RESOURCE_DESC myBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(myAllocationInfo);

        ThrowIfFailed(
            device->CreateCommittedResource(
                &heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &myBufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(noiseTexture.ReleaseAndGetAddressOf())));

        noiseTexture->SetName(L"noiseValues");

        D3D12_SHADER_RESOURCE_VIEW_DESC myOctreeDataDesc = {};
        myOctreeDataDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        myOctreeDataDesc.Format = DXGI_FORMAT_UNKNOWN;
        myOctreeDataDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        myOctreeDataDesc.Buffer.NumElements = aSizeX * aSizeY;
        myOctreeDataDesc.Buffer.StructureByteStride = sizeof(int);

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 2, cbvSrvUavDescriptorSize);
        device->CreateShaderResourceView(noiseTexture.Get(), &myOctreeDataDesc, srvHandle);
    }

    //create structured buffer for the octree
    {
        auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        D3D12_RESOURCE_ALLOCATION_INFO myAllocationInfo;
        // max size of 16x16x16 octree
        //myAllocationInfo.SizeInBytes = 586 * sizeof(OctreeElement[8]);
        //myAllocationInfo.SizeInBytes = 3004 * sizeof(OctreeElement[8]);
        //myAllocationInfo.SizeInBytes = 52278 * sizeof(OctreeElement[8]);
        myAllocationInfo.SizeInBytes = 124622 * sizeof(OctreeElement[8]);
        myAllocationInfo.Alignment = 0;

        const D3D12_RESOURCE_DESC myBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(myAllocationInfo);

        ThrowIfFailed(
            device->CreateCommittedResource(
                &heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &myBufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(octreeBuffer.ReleaseAndGetAddressOf())));

        octreeBuffer->SetName(L"Octree buffer");

        D3D12_SHADER_RESOURCE_VIEW_DESC myOctreeDataDesc = {};
        myOctreeDataDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        myOctreeDataDesc.Format = DXGI_FORMAT_UNKNOWN;
        myOctreeDataDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        // 586 is the size of my current octree
        //myOctreeDataDesc.Buffer.NumElements = 586;
        //myOctreeDataDesc.Buffer.NumElements = 3004;
        //myOctreeDataDesc.Buffer.NumElements = 52278;
        myOctreeDataDesc.Buffer.NumElements = 124622;
        myOctreeDataDesc.Buffer.StructureByteStride = sizeof(OctreeElement[8]);

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 6, cbvSrvUavDescriptorSize);
        device->CreateShaderResourceView(octreeBuffer.Get(), &myOctreeDataDesc, srvHandle);
    }

    //create buffers for voxel grid
    {
        //const buffer
        {
            auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(VoxelGridBuffer));

            ThrowIfFailed(device->CreateCommittedResource(
                &heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&voxelGridBufferResource)));

            // Map and initialize the constant buffer. We don't unmap this until the
            // app closes. Keeping things mapped for the lifetime of the resource is okay.
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            ThrowIfFailed(voxelGridBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&pVoxelGridBufferCbvDataBegin)));

            // Describe and create a constant buffer view.
            D3D12_CONSTANT_BUFFER_VIEW_DESC myBufferDesc{};

            myBufferDesc.BufferLocation = voxelGridBufferResource->GetGPUVirtualAddress();
            myBufferDesc.SizeInBytes = static_cast<UINT>(sizeof(VoxelGridBuffer));

            CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 7, cbvSrvUavDescriptorSize);
            device->CreateConstantBufferView(&myBufferDesc, cbvHandle);
        }

        // top level
        {
            auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

            D3D12_RESOURCE_ALLOCATION_INFO myAllocationInfo;
            // max chunks for 128x128x128 scene
            myAllocationInfo.SizeInBytes = 512 * sizeof(int);
            myAllocationInfo.Alignment = 0;

            const D3D12_RESOURCE_DESC myBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(myAllocationInfo);

            ThrowIfFailed(
                device->CreateCommittedResource(
                    &heapUpload,
                    D3D12_HEAP_FLAG_NONE,
                    &myBufferDesc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(voxelGridTopLevelBuffer.ReleaseAndGetAddressOf())));

            voxelGridTopLevelBuffer->SetName(L"gridTopLevelBuffer");

            D3D12_SHADER_RESOURCE_VIEW_DESC myOctreeDataDesc = {};
            myOctreeDataDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            myOctreeDataDesc.Format = DXGI_FORMAT_UNKNOWN;
            myOctreeDataDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            myOctreeDataDesc.Buffer.NumElements = 512;
            myOctreeDataDesc.Buffer.StructureByteStride = sizeof(int);

            CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 8, cbvSrvUavDescriptorSize);
            device->CreateShaderResourceView(voxelGridTopLevelBuffer.Get(), &myOctreeDataDesc, srvHandle);
        }

        // layer 1
        {
            auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

            D3D12_RESOURCE_ALLOCATION_INFO myAllocationInfo;
            // max chunks for 128x128x128 scene
            myAllocationInfo.SizeInBytes = 512 * sizeof(Layer1Chunk);
            myAllocationInfo.Alignment = 0;

            const D3D12_RESOURCE_DESC myBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(myAllocationInfo);

            ThrowIfFailed(
                device->CreateCommittedResource(
                    &heapUpload,
                    D3D12_HEAP_FLAG_NONE,
                    &myBufferDesc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(voxelGridLayer1Buffer.ReleaseAndGetAddressOf())));

            voxelGridLayer1Buffer->SetName(L"gridLayer1Buffer");

            D3D12_SHADER_RESOURCE_VIEW_DESC myOctreeDataDesc = {};
            myOctreeDataDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            myOctreeDataDesc.Format = DXGI_FORMAT_UNKNOWN;
            myOctreeDataDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            myOctreeDataDesc.Buffer.NumElements = 512;
            myOctreeDataDesc.Buffer.StructureByteStride = sizeof(Layer1Chunk);

            CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 9, cbvSrvUavDescriptorSize);
            device->CreateShaderResourceView(voxelGridLayer1Buffer.Get(), &myOctreeDataDesc, srvHandle);
        }

        // layer 2
        {
            auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

            D3D12_RESOURCE_ALLOCATION_INFO myAllocationInfo;
            // max chunks for 128x128x128 scene
            myAllocationInfo.SizeInBytes = 32768 * sizeof(Layer2Chunk);
            myAllocationInfo.Alignment = 0;

            const D3D12_RESOURCE_DESC myBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(myAllocationInfo);

            ThrowIfFailed(
                device->CreateCommittedResource(
                    &heapUpload,
                    D3D12_HEAP_FLAG_NONE,
                    &myBufferDesc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(voxelGridLayer2Buffer.ReleaseAndGetAddressOf())));

            voxelGridLayer2Buffer->SetName(L"gridLayer2Buffer");

            D3D12_SHADER_RESOURCE_VIEW_DESC myOctreeDataDesc = {};
            myOctreeDataDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            myOctreeDataDesc.Format = DXGI_FORMAT_UNKNOWN;
            myOctreeDataDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            myOctreeDataDesc.Buffer.NumElements = 32768;
            myOctreeDataDesc.Buffer.StructureByteStride = sizeof(Layer2Chunk);

            CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 10, cbvSrvUavDescriptorSize);
            device->CreateShaderResourceView(voxelGridLayer2Buffer.Get(), &myOctreeDataDesc, srvHandle);
        }

    }

    //voxel atlas buffer
    {
        auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        D3D12_RESOURCE_ALLOCATION_INFO myAllocationInfo;
        // max chunks for 128x128x128 scene
        myAllocationInfo.SizeInBytes = 255 * sizeof(VoxelAtlasItem);
        myAllocationInfo.Alignment = 0;

        const D3D12_RESOURCE_DESC myBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(myAllocationInfo);

        ThrowIfFailed(
            device->CreateCommittedResource(
                &heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &myBufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(voxelAtlasBuffer.ReleaseAndGetAddressOf())));

        voxelAtlasBuffer->SetName(L"voxelAtlasBuffer");

        D3D12_SHADER_RESOURCE_VIEW_DESC myOctreeDataDesc = {};
        myOctreeDataDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        myOctreeDataDesc.Format = DXGI_FORMAT_UNKNOWN;
        myOctreeDataDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        myOctreeDataDesc.Buffer.NumElements = 255;
        myOctreeDataDesc.Buffer.StructureByteStride = sizeof(VoxelAtlasItem);

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 11, cbvSrvUavDescriptorSize);
        device->CreateShaderResourceView(voxelAtlasBuffer.Get(), &myOctreeDataDesc, srvHandle);
    }

    // create texture for sky dome
    {
        // Describe and create a Texture2D.
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDesc.Width = 4096;
        textureDesc.Height = 2048;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ThrowIfFailed(
            device->CreateCommittedResource(
                &myDefaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                nullptr,
                IID_PPV_ARGS(skydomeTexture.ReleaseAndGetAddressOf())));
        skydomeTexture->SetName(L"skydome");

        D3D12_SHADER_RESOURCE_VIEW_DESC mySceneDataDesc = {};
        mySceneDataDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        mySceneDataDesc.Format = textureDesc.Format;
        mySceneDataDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        mySceneDataDesc.Texture3D.MipLevels = 1;

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 12, cbvSrvUavDescriptorSize);
        device->CreateShaderResourceView(skydomeTexture.Get(), &mySceneDataDesc, srvHandle);
    }

    // create sampler for sky dome
    {
        D3D12_SAMPLER_DESC mySamplerDesc {};
        mySamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        mySamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        mySamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        mySamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        mySamplerDesc.MipLODBias = 0.0f;
        mySamplerDesc.MaxAnisotropy = 0;
        mySamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
        mySamplerDesc.BorderColor[0] = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        mySamplerDesc.MinLOD = 0;
        mySamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(samplerHeap->GetCPUDescriptorHandleForHeapStart(), 0, samplerDescriptorSize);
        device->CreateSampler(&mySamplerDesc, samplerHandle);
    }

    // use for debugging shader
    //UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    
    // use for checking assembly
    //UINT compileFlags = D3DCOMPILE_DEBUG;

    // use for profiling
    UINT compileFlags = NULL;

    //D3D_SHADER_MACRO macros[] = { "TEST", "1", NULL, NULL };
    D3D_SHADER_MACRO macros[] = { "MAX_STACK_SIZE", "7", NULL, NULL};

    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/raytraceComputeTest.hlsl", macros, nullptr, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/raytraceComputeOctree.hlsl", macros, nullptr, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/raytraceComputeOctreeRework.hlsl", macros, nullptr, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/raytraceComputeOctreeReworkOptimized.hlsl", macros, nullptr, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/test.hlsl", macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));
    D3DCompileFromFile(L"resources/shaders/raytraceLighting.hlsl", macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob);

        if (globalErrorBlob)
        {
            OutputDebugStringA((char*)globalErrorBlob->GetBufferPointer());
            globalErrorBlob->Release();
        }

    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/DDATraversal.hlsl", macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/raytraceLighting.hlsl", macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/raytraceCompute.hlsl", macros, nullptr, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));
    //ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/rayDirToColor.hlsl", macros, nullptr, "main", "cs_5_0", compileFlags, 0, &computeShader, &globalErrorBlob));

    CD3DX12_DESCRIPTOR_RANGE1 myRanges[12];
    myRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    myRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    myRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    myRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    myRanges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    
    myRanges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    myRanges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    myRanges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    myRanges[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
    myRanges[9].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);

    myRanges[10].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
    myRanges[11].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    CD3DX12_ROOT_PARAMETER1 myRootParameters[12];
    myRootParameters[0].InitAsDescriptorTable(1, &myRanges[0], D3D12_SHADER_VISIBILITY_ALL); // camera const buffer
    myRootParameters[1].InitAsDescriptorTable(1, &myRanges[1], D3D12_SHADER_VISIBILITY_ALL); // output texture
    myRootParameters[2].InitAsDescriptorTable(1, &myRanges[2], D3D12_SHADER_VISIBILITY_ALL); // noise texture

    myRootParameters[3].InitAsDescriptorTable(1, &myRanges[3], D3D12_SHADER_VISIBILITY_ALL); // octree buffer
    myRootParameters[4].InitAsDescriptorTable(1, &myRanges[4], D3D12_SHADER_VISIBILITY_ALL); // octree const buffer
    
    myRootParameters[5].InitAsDescriptorTable(1, &myRanges[5], D3D12_SHADER_VISIBILITY_ALL); // voxel grid top level buffer
    myRootParameters[6].InitAsDescriptorTable(1, &myRanges[6], D3D12_SHADER_VISIBILITY_ALL); // voxel grid layer 1 buffer
    myRootParameters[7].InitAsDescriptorTable(1, &myRanges[7], D3D12_SHADER_VISIBILITY_ALL); // voxel grid layer 2 buffer
    myRootParameters[8].InitAsDescriptorTable(1, &myRanges[8], D3D12_SHADER_VISIBILITY_ALL); // voxel grid const buffer
    myRootParameters[9].InitAsDescriptorTable(1, &myRanges[9], D3D12_SHADER_VISIBILITY_ALL); // voxel atlas buffer

    myRootParameters[10].InitAsDescriptorTable(1, &myRanges[10], D3D12_SHADER_VISIBILITY_ALL); // skydome
    myRootParameters[11].InitAsDescriptorTable(1, &myRanges[11], D3D12_SHADER_VISIBILITY_ALL); // skydome sampler

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

    //const buffer
    {
        auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBuffer));

        ThrowIfFailed(device->CreateCommittedResource(
            &heapUpload,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&computeConstantBufferResource)));

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(computeConstantBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&pComputeCbvDataBegin)));

        // Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC myBufferDesc{};

        myBufferDesc.BufferLocation = computeConstantBufferResource->GetGPUVirtualAddress();
        myBufferDesc.SizeInBytes = static_cast<UINT>(sizeof(ConstantBuffer));

        CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 3, cbvSrvUavDescriptorSize);
        device->CreateConstantBufferView(&myBufferDesc, cbvHandle);
    }

    //const buffer octree
    {
        auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(OctreeBuffer));

        ThrowIfFailed(device->CreateCommittedResource(
            &heapUpload,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&octreeConstantBufferResource)));

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(octreeConstantBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&pOctreeCbvDataBegin)));

        // Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC myBufferDesc{};

        myBufferDesc.BufferLocation = octreeConstantBufferResource->GetGPUVirtualAddress();
        myBufferDesc.SizeInBytes = static_cast<UINT>(sizeof(OctreeBuffer));

        CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 5, cbvSrvUavDescriptorSize);
        device->CreateConstantBufferView(&myBufferDesc, cbvHandle);
    }
}

void Graphics::loadAccumulationStage(const unsigned int aSizeX, const unsigned int aSizeY)
{
    const D3D12_HEAP_PROPERTIES myDefaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    //create out textures for the frame accumulation shader
    {
        const D3D12_RESOURCE_DESC myTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, aSizeX, aSizeY, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        ThrowIfFailed(
            device->CreateCommittedResource(
                &myDefaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &myTextureDesc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                nullptr,
                IID_PPV_ARGS(accumulationOutputTexture.ReleaseAndGetAddressOf())));
        accumulationOutputTexture->SetName(L"accumulation Texture");

        //threadGroupX = static_cast<uint32_t>(myTextureDesc.Width) / SHADER_THREAD_COUNT_X;
        //threadGroupY = myTextureDesc.Height / SHADER_THREAD_COUNT_Y;

        // create uav
        CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle1(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 1, cbvSrvUavDescriptorSize);
        device->CreateUnorderedAccessView(accumulationOutputTexture.Get(), nullptr, nullptr, uavHandle1);
    }

    //create the constant buffer for frame accumulation shader
    {
        auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBuffer));

        ThrowIfFailed(device->CreateCommittedResource(
            &heapUpload,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&accumulationConstantBufferResource)));

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(accumulationConstantBufferResource->Map(0, &readRange, reinterpret_cast<void**>(&pAccumulationCbvDataBegin)));

        // Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC myBufferDesc{};

        myBufferDesc.BufferLocation = accumulationConstantBufferResource->GetGPUVirtualAddress();
        myBufferDesc.SizeInBytes = static_cast<UINT>(sizeof(ConstantBuffer));

        CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 4, cbvSrvUavDescriptorSize);
        device->CreateConstantBufferView(&myBufferDesc, cbvHandle);
    }

    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    //D3D_SHADER_MACRO macros[] = { "TEST", "1", NULL, NULL };
    D3D_SHADER_MACRO macros[] = { NULL, NULL };

    ID3DBlob* errorBlob = nullptr;
    ThrowIfFailed(D3DCompileFromFile(L"resources/shaders/frameAccumulation.hlsl", macros, nullptr, "main", "cs_5_0", compileFlags, 0, &accumulationShader, &globalErrorBlob));

    CD3DX12_DESCRIPTOR_RANGE1 myRanges[3];
    myRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    myRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
    myRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 myRootParameters[3];
    myRootParameters[0].InitAsDescriptorTable(1, &myRanges[0], D3D12_SHADER_VISIBILITY_ALL); // input texture
    myRootParameters[1].InitAsDescriptorTable(1, &myRanges[1], D3D12_SHADER_VISIBILITY_ALL); // output texture
    myRootParameters[2].InitAsDescriptorTable(1, &myRanges[2], D3D12_SHADER_VISIBILITY_ALL); // accumulation const buffer

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
    ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&accumulationRootSignature)));

    // Create compute pipeline state
    D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
    descComputePSO.pRootSignature = accumulationRootSignature.Get();
    descComputePSO.CS.pShaderBytecode = accumulationShader.Get()->GetBufferPointer();
    descComputePSO.CS.BytecodeLength = accumulationShader.Get()->GetBufferSize();

    ThrowIfFailed(device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(accumulationPipelineState.ReleaseAndGetAddressOf())));
    accumulationPipelineState->SetName(L"accumulation PSO");
}

void Graphics::initImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
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
