#pragma once
#include "camera.h"
#include "rendering\octree.h"

#include <glm/vec4.hpp>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "d3dx12.h"

#include "rendering/imgui-docking/imgui.h"
#include "imgui-docking/imgui_impl_dx12.h"
#include "imgui-docking/imgui_impl_win32.h"

#define FRAME_COUNT 2
#define SHADER_THREAD_COUNT 8

struct ConstantBuffer
{
	glm::vec4 maxThreadIter;

	glm::vec4 camPosition;
	glm::vec4 camDirection;
	
	glm::vec4 camUpperLeftCorner;
	glm::vec4 camPixelOffsetHorizontal;
	glm::vec4 camPixelOffsetVertical;

	//int octreeBuffer[4681];

	float padding[40];
};

constexpr size_t modulatedSize = sizeof(ConstantBuffer) % 256;
static_assert(modulatedSize == 0);

class Graphics
{
public:
	Graphics();
	~Graphics();

	void init();
	void shutdown();

	void beginFrame();
	void endFrame();

	void renderFrame();
	void renderImGui(int aFPS);

	void copyComputeTextureToBackbuffer();

	void updateCameraVariables(Camera& aCamera);
	void updateModelVariables(const VoxelModel& aModel);
	void updateOctreeVariables(const Octree& aOctree);

private:
	void updateConstantBuffer();

	bool loadPipeline();
	void loadAssets();

	void loadComputeStage();

	void initImGui();

	void waitForGpu();

	void GetHardwareAdapter(
		IDXGIFactory1* pFactory,
		IDXGIAdapter1** ppAdapter,
		bool requestHighPerformanceAdapter = false);

	// Adapter info.
	bool useWarpDevice = false;

	// Pipeline objects.
	CD3DX12_VIEWPORT viewport;
	CD3DX12_RECT scissorRect;

	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> device;

	Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
	UINT rtvDescriptorSize{ 0 };

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[FRAME_COUNT];
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

	// Synchronization objects.
	UINT frameIndex{ 0 };
	HANDLE fenceEvent{ 0 };
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	UINT64 fenceValues[FRAME_COUNT]{ 0 };

	// ImGui stuff
	ID3D12DescriptorHeap* pd3dSrvDescHeap = NULL;
	ImGuiIO* io;

	//compute shader stuff
	Microsoft::WRL::ComPtr<ID3D12Resource>      computeTexture[2];
	Microsoft::WRL::ComPtr<ID3D12Resource>      sceneDataTexture;
	Microsoft::WRL::ComPtr<ID3D12Resource>      octreeBuffer;

	UINT8* pCbvDataBeginOctree = nullptr;

	//D3D12_RESOURCE_STATES computeTextureResourceState[2]; // current state of the compute texture, unordered or texture view

	int threadGroupX;
	int threadGroupY;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;
	UINT cbvSrvUavDescriptorSize{ 0 };

	Microsoft::WRL::ComPtr<ID3DBlob> computeShader;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState;

	// constant buffer for compute shader
	ConstantBuffer* constantBuffer;

	Microsoft::WRL::ComPtr<ID3D12Resource> computeConstantBuffer;

	UINT8* pCbvDataBegin = nullptr;
	void* computeConstantBufferData = nullptr;
	size_t computeConstantBufferSize{ 0 };
};

