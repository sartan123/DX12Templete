#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <comdef.h>
#include "stddef.h"
#include "d3dx12.h"
#include "Square.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

#define MAKE_SMART_COM_PTR(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
MAKE_SMART_COM_PTR(ID3D12Device5);
MAKE_SMART_COM_PTR(ID3D12GraphicsCommandList4);
MAKE_SMART_COM_PTR(ID3D12CommandQueue);
MAKE_SMART_COM_PTR(IDXGISwapChain3);
MAKE_SMART_COM_PTR(IDXGIFactory4);
MAKE_SMART_COM_PTR(IDXGIAdapter1);
MAKE_SMART_COM_PTR(ID3D12Fence);
MAKE_SMART_COM_PTR(ID3D12CommandAllocator);
MAKE_SMART_COM_PTR(ID3D12Resource);
MAKE_SMART_COM_PTR(ID3D12DescriptorHeap);
MAKE_SMART_COM_PTR(ID3D12Debug);
MAKE_SMART_COM_PTR(ID3D12StateObject);
MAKE_SMART_COM_PTR(ID3D12RootSignature);
MAKE_SMART_COM_PTR(ID3DBlob);

class Square
{
public:
	Square() {}
	~Square() {}
	void Initialize(ID3D12Device* device);
	void draw(ID3D12GraphicsCommandList* command_list);
private:
	ComPtr<ID3D12Resource>  _vertex_buffer;
	ComPtr<ID3D12Resource1> _index_buffer;
	ComPtr<ID3D12Resource1> m_constantBuffers;
	UINT  m_srvcbvDescriptorSize;


	D3D12_VERTEX_BUFFER_VIEW    _vertex_buffer_view;
	D3D12_INDEX_BUFFER_VIEW   _index_buffer_view;

	ComPtr<ID3D12DescriptorHeap> m_heapSrvCbv;
	D3D12_GPU_DESCRIPTOR_HANDLE m_cbViews;

	ComPtr<ID3D12Resource1> CreateBuffer(ID3D12Device* device, UINT bufferSize, const void* initialData);
};

