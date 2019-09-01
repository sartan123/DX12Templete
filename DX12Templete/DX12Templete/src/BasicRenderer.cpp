#include "BasicRenderer.h"

BasicRenderer::BasicRenderer(HWND hwnd, int Width, int Height)
 :  mHwnd(hwnd),
	mWidth(Width),
	mHeight(Height)
{
	Initialize();
}

BasicRenderer::~BasicRenderer()
{
	Destroy();
}

////////////////////////////////////////////
//         Initialize                     //
////////////////////////////////////////////
void BasicRenderer::Initialize()
{
	InitPipeline();
	LoadAssets();
}

void BasicRenderer::InitPipeline()
{
#if defined(_DEBUG)
	EnableDegugLayer();
#endif
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain();
	CreateRTVDescHeap();
	CreateFrameResource();
	CreateCommandAllocator();
}

void BasicRenderer::EnableDegugLayer()
{
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
}

void BasicRenderer::CreateDevice()
{
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mFactory)));

	ThrowIfFailed(mFactory->EnumAdapters(0, &mAdapter));

	ThrowIfFailed(D3D12CreateDevice(mAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice)));
}

void BasicRenderer::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
}

void BasicRenderer::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = FrameBufferCount;
	swapChainDesc.BufferDesc.Width = mWidth;
	swapChainDesc.BufferDesc.Height = mHeight;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = mHwnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	ComPtr<IDXGISwapChain> swapChain;
	ThrowIfFailed(mFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&swapChainDesc,
		&swapChain
	));

	ThrowIfFailed(swapChain.As(&mSwapChain));
}

void BasicRenderer::CreateRTVDescHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = FrameBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRTVDescriptorHeap)));

	mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void BasicRenderer::CreateFrameResource()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT n = 0; n < FrameBufferCount; n++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTarget[n])));
		mDevice->CreateRenderTargetView(mRenderTarget[n].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, mRTVDescriptorSize);
	}
}

void BasicRenderer::CreateCommandAllocator()
{
	ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator)));
}

void BasicRenderer::WaitForGPU()
{
	const UINT64 fence = mFrameFenceValue;
	ThrowIfFailed(mCommandQueue->Signal(mFrameFence.Get(), fence));
	mFrameFenceValue++;

	if (mFrameFence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(mFrameFence->SetEventOnCompletion(fence, mFenceEvent));
		WaitForSingleObject(mFenceEvent, INFINITE);
	}

	mCurrentFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
}

void BasicRenderer::LoadAssets()
{
	CreateRootSignature();

	CompileShader();

	CreatePipelineStateObject();

	CreateCommandList();

	CloseCommandList();

	LoadVertexBuffer();

	CreateFence();

	CreateEventHandle();

	WaitForGPU();
}

void BasicRenderer::CreateRootSignature()
{
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	ThrowIfFailed(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void BasicRenderer::CreateCommandList()
{
	ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), mPipelineState.Get(), IID_PPV_ARGS(&mCommandList)));
}

void BasicRenderer::CloseCommandList()
{
	ThrowIfFailed(mCommandList->Close());
}

void BasicRenderer::CreateFence()
{
	ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFrameFence)));
	mFrameFenceValue = 1;
}

void BasicRenderer::CreateEventHandle()
{
	mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (mFenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

////////////////////////////////////////////
//         Render                         //
////////////////////////////////////////////
void BasicRenderer::Render()
{
	ResetCommandAllocator();

	ResetCommandList();

	SetCommandList();

	ExecuteCommandList();
	
	PresentFrame();
	
	WaitForGPU();
}

void BasicRenderer::ResetCommandAllocator()
{
	ThrowIfFailed(mCommandAllocator->Reset());
}

void BasicRenderer::ResetCommandList()
{
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), mPipelineState.Get()));
}

void BasicRenderer::SetGraphicsRootSignature()
{
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
}

void BasicRenderer::SetViewPort()
{
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = (FLOAT)mWidth;
	mViewport.Height = (FLOAT)mHeight;
	mViewport.MinDepth = 0;
	mViewport.MaxDepth = 1;
	mCommandList->RSSetViewports(1, &mViewport);
}

void BasicRenderer::SetScissoringRect()
{
	mScissorRect.left = 0;
	mScissorRect.top = 0;
	mScissorRect.right = (LONG)mWidth;
	mScissorRect.bottom = (LONG)mHeight;
	mCommandList->RSSetScissorRects(1, &mScissorRect);
}

void BasicRenderer::ExecuteCommandList()
{
	ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

void BasicRenderer::PresentFrame()
{
	ThrowIfFailed(mSwapChain->Present(1, 0));
}

ComPtr<ID3D12Resource1> BasicRenderer::CreateBuffer(const void* initialData, UINT bufferSize)
{
	HRESULT hr;
	ComPtr<ID3D12Resource1> buffer;
	hr = mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&buffer)
	);

	if (SUCCEEDED(hr) && initialData != nullptr)
	{
		void* mapped;
		CD3DX12_RANGE range(0, 0);
		hr = buffer->Map(0, &range, &mapped);
		if (SUCCEEDED(hr))
		{
			memcpy(mapped, initialData, bufferSize);
			buffer->Unmap(0, nullptr);
		}
	}

	return buffer;
}

////////////////////////////////////////////
//         Update                         //
////////////////////////////////////////////
void BasicRenderer::Update()
{

}
////////////////////////////////////////////
//         Destroy                        //
////////////////////////////////////////////
void BasicRenderer::Destroy()
{
	WaitForGPU();

	CloseHandle(mFenceEvent);
}
