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
#include "dxcapi.use.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

#define MAKE_SMART_COM_PTR(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
MAKE_SMART_COM_PTR(ID3D12Device5);
MAKE_SMART_COM_PTR(ID3D12GraphicsCommandList4);
MAKE_SMART_COM_PTR(ID3D12CommandQueue);
MAKE_SMART_COM_PTR(IDXGISwapChain3);
MAKE_SMART_COM_PTR(IDXGIFactory4);
MAKE_SMART_COM_PTR(IDXGIAdapter1);
MAKE_SMART_COM_PTR(ID3D12Fence1);
MAKE_SMART_COM_PTR(ID3D12CommandAllocator);
MAKE_SMART_COM_PTR(ID3D12Resource);
MAKE_SMART_COM_PTR(ID3D12DescriptorHeap);
MAKE_SMART_COM_PTR(ID3D12Debug);
MAKE_SMART_COM_PTR(ID3D12PipelineState);
MAKE_SMART_COM_PTR(ID3D12StateObject);
MAKE_SMART_COM_PTR(ID3D12RootSignature);
MAKE_SMART_COM_PTR(ID3DBlob);
MAKE_SMART_COM_PTR(IDxcCompiler);
MAKE_SMART_COM_PTR(IDxcLibrary);
MAKE_SMART_COM_PTR(IDxcBlobEncoding);
MAKE_SMART_COM_PTR(IDxcOperationResult);

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

	static ID3D12Device5Ptr GetDevice() { return mDevice; }
	static ID3D12GraphicsCommandList4Ptr GetCmdList() { return mCmdList; }
private:
	HWND    mHwnd;
	int     mWidth;
	int     mHeight;

	UINT mFrameIndex;

	HANDLE  mFenceEvent;
	ID3D12Fence1Ptr mFrameFence;
	UINT64 mFrameFenceValue;

	ID3D12CommandAllocatorPtr mCmdAllocator;
	IDXGIFactory4Ptr mFactory;
	static ID3D12Device5Ptr mDevice;
	ID3D12CommandQueuePtr mCmdQueue;
	static ID3D12GraphicsCommandList4Ptr mCmdList;
	ComPtr<IDXGISwapChain3> mSwapChain;

	ID3D12DescriptorHeapPtr mDescriptorHeap;
	ID3D12ResourcePtr mRenderTarget[FrameBufferCount];
	D3D12_CPU_DESCRIPTOR_HANDLE mRTVHandle[FrameBufferCount];

	ID3D12RootSignaturePtr mRootSignature;
	ID3D12PipelineStatePtr mPipelineState;

	D3D12_VIEWPORT mViewPort;


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
	void InitializeAccelarationStructure();

	// RayTracing
	void CreateRayTracingPipelineStateObject();
	ID3D12StateObjectPtr mRayTracePipelineState;
	ID3D12RootSignaturePtr mRayTraceRootSignature;

private:
	BOOL    LoadAssets();
	
	std::vector<Square*> mSquareList;

	// Shader
	struct ShaderObject {
		void* binaryPtr;
		int   size;
	};
	ShaderObject mVertexShader;
	ShaderObject mPixelShader;

	BOOL	LoadVertexShader();
	BOOL	LoadPixelShader();
};
