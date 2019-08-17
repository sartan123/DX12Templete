#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "stddef.h"
#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

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

class DX12Renderer {
public:
	static constexpr int FrameBufferCount = 2;
	static constexpr UINT GpuWaitTimeout = (10 * 1000);

public:
	DX12Renderer(HWND hwnd, int Width, int Height);
	~DX12Renderer();
	void Initialize();
	void Update();
	void Render();
	void Destroy();

private:
	HWND    mHwnd;
	int     mWidth;
	int     mHeight;

	UINT _frame_index;

	HANDLE  _fence_event;
	std::vector<ComPtr<ID3D12Fence1>> _frame_fences;
	std::vector<UINT64> _frame_fence_values;

	std::vector<ComPtr<ID3D12CommandAllocator>> _command_allocators;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> _command_queue;
	ComPtr<ID3D12GraphicsCommandList> _command_list;
	ComPtr<IDXGISwapChain3> _swap_chain;

	ComPtr<ID3D12DescriptorHeap> _descriptor_heap;
	ComPtr<ID3D12Resource> _render_target[FrameBufferCount];
	D3D12_CPU_DESCRIPTOR_HANDLE _rtv_handle[FrameBufferCount];

	void SetResourceBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
	void WaitForCommandQueue();
	void CreateDebugInterface();
	void SetViewPort();

	HRESULT CreateRootSignature();
	HRESULT CreatePipelineObject();
	HRESULT CreateCommandList();
	HRESULT CreateCommandAllocators();
	HRESULT PrepareDescriptorHeaps();
	HRESULT PrepareRenderTargetView();
	HRESULT PrepareDescriptorHeapForCubeApp();
	HRESULT CreateFence();

	ComPtr<ID3D12Resource1> CreateBuffer(UINT bufferSize, const void* initialData);

private:
	struct ShaderObject {
		void* binaryPtr;
		int   size;
	};
	ComPtr<ID3D12RootSignature> _root_signature;
	ComPtr<ID3D12PipelineState> _pipeline_state;
	ShaderObject _g_vertex_shader;
	ShaderObject _g_pixel_shader;
	ComPtr<ID3D12Resource>      _vertex_buffer;
	D3D12_VERTEX_BUFFER_VIEW    _vertex_buffer_view;

	ComPtr<ID3D12Resource1> _index_buffer;
	D3D12_INDEX_BUFFER_VIEW   _index_buffer_view;
	UINT  _index_count;

	ComPtr<ID3D12DescriptorHeap> m_heapSrvCbv;
	UINT  m_srvcbvDescriptorSize;
	std::vector<ComPtr<ID3D12Resource1>> m_constantBuffers;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_cbViews;

	D3D12_VIEWPORT _viewport;

	void LoadPipeline();
	BOOL    LoadAssets();

	BOOL	LoadVertexShader();
	BOOL	LoadPixelShader();
};
