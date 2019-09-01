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
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

class BasicRenderer
{
public:
	BasicRenderer(HWND hwnd, int Width, int Height);
	~BasicRenderer();
	virtual void Initialize();
	virtual void Update();
	virtual void Render();
	virtual void Destroy();
protected:
	// Initialize
	virtual void InitPipeline();
	virtual void EnableDegugLayer();
	virtual void CreateDevice();
	virtual void CreateCommandQueue();
	virtual void CreateSwapChain();
	virtual void CreateRTVDescHeap();
	virtual void CreateFrameResource();
	virtual void CreateCommandAllocator();
	virtual void WaitForGPU();

	// Assets
	virtual void LoadAssets();
	virtual void CreateRootSignature();
	virtual void CompileShader() = 0;
	virtual void CreatePipelineStateObject() = 0;
	virtual void CreateCommandList();
	virtual void CloseCommandList();
	virtual void LoadVertexBuffer() = 0;
	virtual void CreateFence();
	virtual void CreateEventHandle();

	// Render
	virtual void ResetCommandAllocator();
	virtual void ResetCommandList();
	virtual void SetCommandList() = 0;
	virtual void SetGraphicsRootSignature();
	virtual void SetViewPort();
	virtual void SetScissoringRect();
	virtual void RecordCommand() = 0;
	virtual void ExecuteCommandList();
	virtual void PresentFrame();

	ComPtr<ID3D12Resource1> CreateBuffer(const void* initialData, UINT bufferSize);

	static constexpr int FrameBufferCount = 2;

	UINT mCurrentFrameIndex;
	
	ComPtr<ID3D12RootSignature> mRootSignature;
	ComPtr<ID3D12PipelineState> mPipelineState;
	ComPtr<ID3D12Device> mDevice;
	ComPtr<ID3D12CommandQueue> mCommandQueue;
	ComPtr<IDXGIFactory4> mFactory;
	ComPtr<IDXGIAdapter> mAdapter;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;
	ComPtr<IDXGISwapChain3> mSwapChain;
	ComPtr<ID3D12DescriptorHeap> mRTVDescriptorHeap;
	ComPtr<ID3D12Resource> mRenderTarget[FrameBufferCount];
	ComPtr<ID3D12CommandAllocator> mCommandAllocator;

	ComPtr<ID3D12Resource> mVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

	UINT mRTVDescriptorSize;

	HANDLE  mFenceEvent;
	ComPtr<ID3D12Fence1> mFrameFence;
	UINT64 mFrameFenceValue;
	D3D12_CPU_DESCRIPTOR_HANDLE mRTVhandle[FrameBufferCount];

	D3D12_VIEWPORT mViewport;
	CD3DX12_RECT   mScissorRect;
	HWND    mHwnd;
	int     mWidth;
	int     mHeight;
};

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw HrException(hr);
	}
}