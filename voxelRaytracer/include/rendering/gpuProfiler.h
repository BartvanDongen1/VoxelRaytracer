#pragma once
#include <vector>
#include <memory>
#include <array>
#include <string>
#include <unordered_map>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "d3dx12.h"

constexpr uint64_t MAX_PROFILES = 64;

struct Timestamp
{
	float timeMS{ 0.f };
	std::string name{ "" };
};

struct QueryData
{
	bool queryStarted{ false };
	bool queryFinished{ false };
	ID3D12GraphicsCommandList* commandList{ nullptr };
};

class GPUProfiler
{
public:
	GPUProfiler() {};
	~GPUProfiler() {};

	void init(ID3D12Device* aDevice, ID3D12CommandQueue* aQueue, IDXGISwapChain3* aSwapChain);
	void destroy();

	void NewFrame();
	void EndFrame();

	void BeginProfileScope(ID3D12GraphicsCommandList* aCommandList, char const* aName);
	void EndProfileScope(char const* aName);
	std::vector<Timestamp> GetProfilerResults();

private:
	Microsoft::WRL::ComPtr<ID3D12Device> device{ nullptr };
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue{ nullptr };
	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain{ nullptr };

	Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap{ nullptr };
	Microsoft::WRL::ComPtr<ID3D12Resource> queryReadbackBuffer{ nullptr };

	std::array<QueryData, MAX_PROFILES> queryData;
	std::unordered_map<std::string, uint32_t> nameToIndexMap;

	uint32_t scopeCounter{ 0 };

	std::vector<Timestamp> savedProfilingResults;
};