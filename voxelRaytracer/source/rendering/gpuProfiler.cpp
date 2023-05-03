#include "rendering/GPUProfiler.h"
#include "helper.h"

#include <cassert>

#define FRAME_COUNT 2

void GPUProfiler::init(ID3D12Device* aDevice, ID3D12CommandQueue* aQueue, IDXGISwapChain3* aSwapChain)
{
	device = aDevice;
	commandQueue = aQueue;
	swapChain = aSwapChain;

	// create timing query 
	{
		D3D12_QUERY_HEAP_DESC heapDesc = { };
		heapDesc.Count = MAX_PROFILES * 2;
		heapDesc.NodeMask = 0;
		heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		ThrowIfFailed(device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(queryHeap.GetAddressOf())));
	}

	// create profiling resource
	{
		D3D12_HEAP_PROPERTIES myDefaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		myDefaultHeapProperties.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = MAX_PROFILES * 2 * FRAME_COUNT * sizeof(UINT64);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Alignment = 0;

		ThrowIfFailed(
			device->CreateCommittedResource(
				&myDefaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(queryReadbackBuffer.ReleaseAndGetAddressOf())));
	}
}

void GPUProfiler::destroy()
{
	device = nullptr;
	commandQueue = nullptr;
	swapChain = nullptr;

	queryReadbackBuffer.Reset();
	queryHeap.Reset();
}

void GPUProfiler::NewFrame()
{
	for (auto& profileData : queryData)
	{
		profileData.queryStarted = false;
		profileData.queryFinished = false;
		profileData.commandList = nullptr;
	}

	nameToIndexMap.clear();
	scopeCounter = 0;
}

void GPUProfiler::EndFrame()
{
	savedProfilingResults.clear();

	uint64_t myGpuFrequency = 0;
	commandQueue->GetTimestampFrequency(&myGpuFrequency);

	uint64_t myCurrentBackbufferIndex = swapChain.Get()->GetCurrentBackBufferIndex();

	for (auto const& [_, index] : nameToIndexMap)
	{
		assert(index < MAX_PROFILES);

		QueryData& myProfileData = queryData[index];
		if (myProfileData.queryStarted && myProfileData.queryFinished)
		{
			uint32_t myBeginQueryIndex = static_cast<uint32_t>(index * 2);
			
			uint64_t myReadbackOffset = ((myCurrentBackbufferIndex * MAX_PROFILES * 2) + myBeginQueryIndex) * sizeof(uint64_t);
			assert(myProfileData.commandList);
			myProfileData.commandList->ResolveQueryData(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, myBeginQueryIndex, 2, queryReadbackBuffer.Get(), myReadbackOffset);
		}
	}

	//map data here
	void* myQueryTimestamps;

	ThrowIfFailed(queryReadbackBuffer->Map(0, nullptr, &myQueryTimestamps));

	//unmap
	queryReadbackBuffer->Unmap(0, nullptr);

	uint64_t const* myFrameQueryTimestamps = reinterpret_cast<uint64_t*>(myQueryTimestamps) + (myCurrentBackbufferIndex * MAX_PROFILES * 2);

	savedProfilingResults.reserve(nameToIndexMap.size());
	for (auto const& [myName, myIndex] : nameToIndexMap)
	{
		assert(myIndex < MAX_PROFILES);

		QueryData& myProfileData = queryData[myIndex];
		if (myProfileData.queryStarted && myProfileData.queryFinished)
		{
			uint64_t myStartTime = myFrameQueryTimestamps[myIndex * 2 + 0];
			uint64_t myEndTime = myFrameQueryTimestamps[myIndex * 2 + 1];

			uint64_t delta = myEndTime - myStartTime;
			float myTimeMs = static_cast<float>((delta / static_cast<double>(myGpuFrequency)) * 1000.0f);

			Timestamp myTimestamp;
			myTimestamp.timeMS = myTimeMs;
			myTimestamp.name = myName;

			savedProfilingResults.emplace_back(myTimestamp);
		}
	}
}

void GPUProfiler::BeginProfileScope(ID3D12GraphicsCommandList* aCommandList, char const* name)
{
	uint32_t myProfileIndex = scopeCounter++;

	nameToIndexMap[name] = myProfileIndex;

	QueryData& myProfileData = queryData[myProfileIndex];

	assert(myProfileData.queryStarted == false);
	assert(myProfileData.queryFinished == false);
	
	uint32_t myBeginQueryIndex = uint32_t(myProfileIndex * 2);

	aCommandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, myBeginQueryIndex);
	myProfileData.queryStarted = true;
	myProfileData.commandList = aCommandList;
}

void GPUProfiler::EndProfileScope(char const* name)
{
	uint32_t myProfileIndex = -1;

	myProfileIndex = nameToIndexMap[name];

	assert(myProfileIndex != -1);

	QueryData& myProfileData = queryData[myProfileIndex];
	
	assert(myProfileData.queryStarted == true);
	assert(myProfileData.queryFinished == false);
	
	uint32_t myEndQueryIndex = static_cast<uint32_t>(myProfileIndex * 2 + 1);
	
	myProfileData.commandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, myEndQueryIndex);
	myProfileData.queryFinished = true;
}

std::vector<Timestamp> GPUProfiler::GetProfilerResults()
{
	return savedProfilingResults;
}