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
	struct Vertex {
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	struct ShaderParameters
	{
		XMFLOAT4X4 mtxWorld;
		XMFLOAT4X4 mtxView;
		XMFLOAT4X4 mtxProj;
	};
	enum
	{
		TextureSrvDescriptorBase = 0,
		ConstantBufferDescriptorBase = 1,
		// サンプラーは別ヒープなので先頭を使用
		SamplerDescriptorBase = 0,
	};

public:
	Square() {}
	~Square() {}
	void Initialize();
	void draw();
private:
	ComPtr<ID3D12Resource>  _vertex_buffer;
	ComPtr<ID3D12Resource1> _index_buffer;
	ComPtr<ID3D12Resource1> _constant_buffer;
	UINT  m_srvcbvDescriptorSize;
	UINT  _index_count;
	UINT  _vertex_count;
	float mRadian;

	ComPtr<ID3D12DescriptorHeap> m_heapSrvCbv;
	D3D12_GPU_DESCRIPTOR_HANDLE m_cbViews;

	ComPtr<ID3D12Resource1> CreateBuffer(UINT bufferSize, const void* initialData);
};

