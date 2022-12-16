#pragma once

#include <Windows.h>
#include <exception>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>

#include "rendering/d3dx12.h"

#ifndef IID_GRAPHICS_PPV_ARGS
#define IID_GRAPHICS_PPV_ARGS(x) IID_PPV_ARGS(x)
#endif


inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

// Helper for resource barrier.
inline void TransitionResource(
    _In_ ID3D12GraphicsCommandList* commandList,
    _In_ ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter) noexcept
{
    assert(commandList != nullptr);
    assert(resource != nullptr);

    if (stateBefore == stateAfter)
        return;

    D3D12_RESOURCE_BARRIER desc = {};
    desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    desc.Transition.pResource = resource;
    desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    desc.Transition.StateBefore = stateBefore;
    desc.Transition.StateAfter = stateAfter;

    commandList->ResourceBarrier(1, &desc);
}

// Helper sets a D3D resource name string (used by PIX and debug layer leak reporting).
#if !defined(NO_D3D12_DEBUG_NAME) && (defined(_DEBUG) || defined(PROFILE))
template<UINT TNameLength>
inline void SetDebugObjectName(_In_ ID3D12DeviceChild* resource, _In_z_ const char(&name)[TNameLength]) noexcept
{
    wchar_t wname[MAX_PATH];
    int result = MultiByteToWideChar(CP_UTF8, 0, name, TNameLength, wname, MAX_PATH);
    if (result > 0)
    {
        resource->SetName(wname);
    }
}

template<UINT TNameLength>
inline void SetDebugObjectName(_In_ ID3D12DeviceChild* resource, _In_z_ const wchar_t(&name)[TNameLength]) noexcept
{
    resource->SetName(name);
}
#else
template<UINT TNameLength>
inline void SetDebugObjectName(_In_ ID3D12DeviceChild*, _In_z_ const char(&)[TNameLength]) noexcept
{
}

template<UINT TNameLength>
inline void SetDebugObjectName(_In_ ID3D12DeviceChild*, _In_z_ const wchar_t(&)[TNameLength]) noexcept
{
}
#endif