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

class DX12Renderer {

	struct Vertex {
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};


public:
	static constexpr int FrameBufferCount = 2;
	static constexpr UINT GpuWaitTimeout = (10 * 1000);

public:
	DX12Renderer() {};
	~DX12Renderer();
	void Initialize(HWND hwnd, int Width, int Height);
	void Update();
	void Render();
	void Destroy();

	static ComPtr<ID3D12Device5> GetDevice() { return device; }
	static ComPtr<ID3D12GraphicsCommandList> GetCmdList() { return _command_list; }
private:
	HWND    mHwnd;
	int     mWidth;
	int     mHeight;

	UINT _frame_index;

	HANDLE  _fence_event;
	ComPtr<ID3D12Fence1> _frame_fences;
	UINT64 _frame_fence_values;

	ComPtr<ID3D12CommandAllocator> _command_allocators;
	IDXGIFactory4Ptr factory;
	static ComPtr<ID3D12Device5> device;
	ComPtr<ID3D12CommandQueue> _command_queue;
	static ComPtr<ID3D12GraphicsCommandList> _command_list;
	ComPtr<IDXGISwapChain3> _swap_chain;

	ComPtr<ID3D12DescriptorHeap> _descriptor_heap;
	ComPtr<ID3D12Resource> _render_target[FrameBufferCount];
	D3D12_CPU_DESCRIPTOR_HANDLE _rtv_handle[FrameBufferCount];

	ComPtr<ID3D12RootSignature> _root_signature;
	ComPtr<ID3D12PipelineState> _pipeline_state;

	D3D12_VIEWPORT _viewport;


	void LoadPipeline();
	void CreateDebugInterface();
	HRESULT CreateFactory();
	HRESULT CreateDevice();
	HRESULT CreateCommandQueue();
	HRESULT CreateSwapChain();
	HRESULT CreateRootSignature();
	HRESULT CreatePipelineObject();
	HRESULT CreateCommandList();
	HRESULT CreateRenderTargetView();
	void SetViewPort();
	void PopulateCommandList();
	void SetResourceBarrier(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
	void WaitForCommandQueue();

private:
	BOOL    LoadAssets();
	
	Square* object;

	// Shader
	struct ShaderObject {
		void* binaryPtr;
		int   size;
	};
	ShaderObject _g_vertex_shader;
	ShaderObject _g_pixel_shader;

	BOOL	LoadVertexShader();
	BOOL	LoadPixelShader();
};
