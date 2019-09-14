#pragma once

#include "BasicRenderer.h"

#define d3d_call(a) {HRESULT hr_ = a; if(FAILED(hr_)) { d3dTraceHR( #a, hr_); }}
void d3dTraceHR(const std::string& msg, HRESULT hr);

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

class DX12Renderer: public BasicRenderer {
public:
	static constexpr int FrameBufferCount = 2;
	static constexpr UINT GpuWaitTimeout = (10 * 1000);

public:
	DX12Renderer();
	~DX12Renderer();
	void Initialize(HWND hwnd, int Width, int Height) override;
	void Update() override;
	void Render() override;
	void Destroy() override;

private:
	HWND    mHwnd;
	int     mWidth;
	int     mHeight;

	UINT mFenceIndex;
	float mRadian;

	HANDLE  mFenceEvent;
	std::vector<ID3D12FencePtr> mFrameFences;
	std::vector<UINT64> mFrameFencesVelues;

	std::vector<ID3D12CommandAllocatorPtr> mCommandAllocators;
	ID3D12Device5Ptr mDevice;
	ID3D12CommandQueuePtr mCommandQueue;
	ID3D12GraphicsCommandList4Ptr mCommandList;
	IDXGISwapChain3Ptr mSwapChain;

	ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
	ComPtr<ID3D12Resource> mRenderTarget[FrameBufferCount];
	D3D12_CPU_DESCRIPTOR_HANDLE mRTVHandle[FrameBufferCount];

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
		ShaderObject()
		: binaryPtr(nullptr)
		, size(0)
		{}
		void* binaryPtr;
		int   size;
	};
	ComPtr<ID3D12RootSignature> mRootSignature;
	ComPtr<ID3D12PipelineState> mPipelineState;
	ShaderObject mVertexShader;
	ShaderObject mPixelShader;
	ComPtr<ID3D12Resource>      mVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW    mVertexBufferView;

	ComPtr<ID3D12Resource1> mIndexBuffer;
	D3D12_INDEX_BUFFER_VIEW   mIndexBufferView;
	UINT  mIndexCount;

	ComPtr<ID3D12DescriptorHeap> mHeapSrvCbv;
	UINT  mSrvCbvDescriptorSize;
	std::vector<ComPtr<ID3D12Resource1>> mConstantBuffers;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> mCbViews;

	D3D12_VIEWPORT mViewPort;

	void LoadPipeline();
	BOOL    LoadAssets();

	BOOL	LoadVertexShader();
	BOOL	LoadPixelShader();

	void d3dTraceHR(const std::string& msg, HRESULT hr);

	void DX12Renderer::msgBox(const std::string& msg);
};
