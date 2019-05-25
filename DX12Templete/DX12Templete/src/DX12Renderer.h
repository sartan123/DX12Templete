#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "stddef.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

typedef struct Vertex3D {
	XMFLOAT3 Position;
	XMFLOAT3 Color;
}Vertex3D;

typedef struct ConstantBufferData {
	XMFLOAT4X4 WVP;
}ConstantBufferData;

class DX12Renderer {
public:
	static constexpr int RTV_NUM = 2;

public:
	DX12Renderer(HWND hwnd, int Width, int Height);
	~DX12Renderer();
	HRESULT Render();

private:
	HWND    mHwnd;
	int     mWidth;
	int     mHeight;

	UINT64 Frames;
	UINT rtv_index_;

	HANDLE  _fence_event;

	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> _command_queue;
	ComPtr<ID3D12CommandAllocator> _command_allocator;
	ComPtr<ID3D12GraphicsCommandList> _command_list;
	ComPtr<IDXGISwapChain3> _swap_chain;
	ComPtr<ID3D12Fence>  _queue_fence;
	ComPtr<IDXGIFactory3> _factory;
	ComPtr<ID3D12DescriptorHeap> _descriptor_heap;
	ComPtr<ID3D12Resource> _render_target[2];
	ComPtr<IDXGIAdapter> adapter;
	ID3D12RootSignature *_root_signature;
	ComPtr<ID3D12PipelineState> _pipeline_state;
	ComPtr<ID3DBlob> vertex_shader;
	ComPtr<ID3DBlob> pixel_shader;
	ComPtr<ID3D12Resource>      _vertex_buffer;
	ComPtr<ID3D12Resource>      _depth_buffer;
	ComPtr<ID3D12Resource>      _index_buffer;
	ComPtr<ID3D12Resource>      _const_buffer;

	D3D12_CPU_DESCRIPTOR_HANDLE _rtv_handle[2];
	D3D12_CPU_DESCRIPTOR_HANDLE      handleDSV;
	D3D12_VERTEX_BUFFER_VIEW    _buffer_position;
	D3D12_VERTEX_BUFFER_VIEW    _vertex_view;
	D3D12_INDEX_BUFFER_VIEW     _index_view;

	D3D12_RECT                  RectScissor;
	D3D12_VIEWPORT              Viewport;
private:
	struct ShaderObject {
		void* binaryPtr;
		int   size;
	};
	ShaderObject _g_vertex_shader;
	ShaderObject _g_pixel_shader;

	BOOL	CreateFactory();
	BOOL	CreateDevice();
	BOOL	CreateCommandAllocator();
	BOOL	CreateCommandList();
	BOOL	CreateDescriptorHeap();
	BOOL	CreateCommandQueue();
	BOOL	CreateDepthStencilBuffer();
	BOOL	CreateSwapChain();
	BOOL	CreateBuffer();
	BOOL	CreatePipelineStateObject();
	BOOL    CreateRootSignature();
	BOOL	CreateRenderTarget();
	BOOL	SetVertexData();

	BOOL	WaitForPreviousFrame();

	BOOL	CompileShader();

	BOOL	PopulateCommandList();
	void	SetPlaneData();
	void	SetResourceBarrier(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
	void	WaitForCommandQueue(ID3D12CommandQueue* pCommandQueue);

	BOOL	LoadVertexShader();
	BOOL	LoadPixelShader();
};