#pragma once
#include "camera.h"
#include "rendering\octree.h"
#include "rendering\voxelGrid.h"
#include "rendering\voxelAtlas.h"
#include "engine\texture.h"
#include "gpuProfiler.h"

#include <glm/vec4.hpp>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "d3dx12.h"

struct ImGuiIO;

#define FRAME_COUNT 3
#define SHADER_THREAD_COUNT_X 8
#define SHADER_THREAD_COUNT_Y 4

struct ConstantBuffer
{
	glm::vec4 maxThreadIter;

	glm::vec4 camPosition;
	glm::vec4 camDirection;
	
	glm::vec4 camUpperLeftCorner;
	glm::vec4 camPixelOffsetHorizontal;
	glm::vec4 camPixelOffsetVertical;

	int frameSeed;
	int sampleCount;

	int octreeLayerCount; // not used

	int octreeSize; // not used

	float padding[36];
};

constexpr size_t modulatedSize1 = sizeof(ConstantBuffer) % 256;
static_assert(modulatedSize1 == 0);

struct AcummulationBuffer
{
	int framesAccumulated;
	bool shouldAcummulate;

	float padding[62];
};

constexpr size_t modulatedSize2 = sizeof(AcummulationBuffer) % 256;
static_assert(modulatedSize2 == 0);

struct OctreeBuffer
{
	int octreeLayerCount;
	int octreeSize;

	float padding[62];
};

constexpr size_t modulatedSize3 = sizeof(OctreeBuffer) % 256;
static_assert(modulatedSize3 == 0);

struct VoxelGridBuffer
{
	glm::uvec4 voxelGridSize;

	glm::uvec4 topLevelChunkSize;

	float padding[56];
};

constexpr size_t modulatedSize4 = sizeof(VoxelGridBuffer) % 256;
static_assert(modulatedSize4 == 0);

class Graphics
{
	friend class ImguiWindowManager;
public:
	Graphics();
	~Graphics();

	void init(const unsigned int aSizeX, const unsigned int aSizeY);
	void shutdown();

	void beginFrame();
	void endFrame();

	void renderFrame();
	void renderImGui();

	void copyAccumulationBufferToBackbuffer();

	void updateCameraVariables(Camera& aCamera, bool aFocussed, int aSize);
	void updateAccumulationVariables(bool aShouldNotAccumulate);
	void updateNoiseTexture(const Texture& aTexture);
	void updateOctreeVariables(const Octree& aOctree);
	void updateVoxelGridVariables(const VoxelGrid& aGrid);

	void updateVoxelAtlasVariables(const VoxelAtlas& aAtlas);

	GPUProfiler* getProfiler() const;

private:
	void updateConstantBuffer();

	bool loadPipeline();
	void loadAssets(const unsigned int aSizeX, const unsigned int aSizeY);

	void loadComputeStage(const unsigned int aSizeX, const unsigned int aSizeY);
	void loadAccumulationStage(const unsigned int aSizeX, const unsigned int aSizeY);

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

	//profiling
	GPUProfiler* profiler;

	//compute shader stuff
	Microsoft::WRL::ComPtr<ID3D12Resource>      raytraceOutputTexture;
	Microsoft::WRL::ComPtr<ID3D12Resource>      accumulationOutputTexture;
	Microsoft::WRL::ComPtr<ID3D12Resource>      noiseTexture;
	Microsoft::WRL::ComPtr<ID3D12Resource>      octreeBuffer;

	Microsoft::WRL::ComPtr<ID3D12Resource>      voxelGridTopLevelBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource>      voxelGridLayer1Buffer;
	Microsoft::WRL::ComPtr<ID3D12Resource>      voxelGridLayer2Buffer;

	Microsoft::WRL::ComPtr<ID3D12Resource>      voxelAtlasBuffer;

	int threadGroupX;
	int threadGroupY;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;
	UINT cbvSrvUavDescriptorSize{ 0 };

	Microsoft::WRL::ComPtr<ID3DBlob> computeShader;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState;

	Microsoft::WRL::ComPtr<ID3DBlob> accumulationShader;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> accumulationRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> accumulationPipelineState;

	// constant buffer for compute shader
	ConstantBuffer* computeConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> computeConstantBufferResource;
	UINT8* pComputeCbvDataBegin = nullptr;

	// const buffer for octree traversal
	OctreeBuffer* octreeConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> octreeConstantBufferResource;
	UINT8* pOctreeCbvDataBegin = nullptr;

	// constant buffer for voxel grid
	VoxelGridBuffer* voxelGridConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> voxelGridBufferResource;
	UINT8* pVoxelGridBufferCbvDataBegin = nullptr;

	// constant buffer for frame accumalation shader
	AcummulationBuffer* accumulationConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> accumulationConstantBufferResource;
	UINT8* pAccumulationCbvDataBegin = nullptr;
};

