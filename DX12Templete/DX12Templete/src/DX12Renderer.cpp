#define _CRT_SECURE_NO_WARNINGS
#include "DX12Renderer.h"
#include "utility.h"

ID3D12Device5Ptr DX12Renderer::mDevice = nullptr;
ID3D12GraphicsCommandList4Ptr DX12Renderer::mCmdList = nullptr;

DX12Renderer::~DX12Renderer() {
	if (mVertexShader.binaryPtr) {
		free(mVertexShader.binaryPtr);
	}
	if (mPixelShader.binaryPtr) {
		free(mPixelShader.binaryPtr);
	}
	Destroy();
}

void DX12Renderer::Update()
{
	for (int i = 0; i < mSquareList.size(); i++)
	{
		mSquareList[i]->update();
	}
}

void DX12Renderer::WaitForCommandQueue()
{
	UINT nextIndex = (mFrameIndex + 1) % FrameBufferCount;
	UINT64 currentValue = ++mFrameFenceValue;
	UINT64 finishExpected = mFrameFenceValue;
	UINT64 nextFenceValue = mFrameFence->GetCompletedValue();

	mCmdQueue->Signal(mFrameFence, currentValue);
	if (nextFenceValue < finishExpected)
	{
		mFrameFence->SetEventOnCompletion(finishExpected, mFenceEvent);
		WaitForSingleObject(mFenceEvent, GpuWaitTimeout);
	}
}

void DX12Renderer::Destroy()
{
	WaitForCommandQueue();
	CloseHandle(mFenceEvent);
}

// Render
void DX12Renderer::Render()
{
	PopulateCommandList();

	// 積んだコマンドの実行.
	ID3D12CommandList* pCommandList = mCmdList.GetInterfacePtr();
	mCmdQueue->ExecuteCommandLists(1, &pCommandList);

	WaitForCommandQueue();

	mCmdAllocator->Reset();
	mCmdList->Reset(mCmdAllocator, mPipelineState);

	mSwapChain->Present(1, 0);

	mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
}

void DX12Renderer::PopulateCommandList()
{
	float clearColor[4] = { 0.2f, 0.5f, 0.7f, 0.0f };

	SetResourceBarrier(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// レンダーターゲットのクリア処理.
	mCmdList->ClearRenderTargetView(mRTVHandle[mFrameIndex], clearColor, 0, nullptr);

	mCmdList->SetGraphicsRootSignature(mRootSignature);
	mCmdList->SetPipelineState(mPipelineState);

	D3D12_RECT rect = { 0, 0, mWidth, mHeight };
	mCmdList->RSSetViewports(1, &mViewPort);
	mCmdList->RSSetScissorRects(1, &rect);

	mCmdList->OMSetRenderTargets(1, &mRTVHandle[mFrameIndex], TRUE, nullptr);

	for (int i = 0; i < mSquareList.size(); i++)
	{
		mSquareList[i]->draw();
	}

	SetResourceBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	mCmdList->Close();
}

void DX12Renderer::SetResourceBarrier(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER descBarrier;
	ZeroMemory(&descBarrier, sizeof(descBarrier));
	descBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	descBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	descBarrier.Transition.pResource = mRenderTarget[mFrameIndex];
	descBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	descBarrier.Transition.StateBefore = before;
	descBarrier.Transition.StateAfter = after;
	mCmdList->ResourceBarrier(1, &descBarrier);
}

// Initialize
void DX12Renderer::Initialize(HWND hwnd, int Width, int Height)
{
	mHwnd = hwnd;
	mWidth = Width;
	mHeight = Height;

	LoadPipeline();
	LoadAssets();
}

// Pipeline State
void DX12Renderer::LoadPipeline()
{
#if defined(_DEBUG)
	CreateDebugInterface();
#endif

	CreateFactory();

	CreateDevice();

	CreateCommandQueue();

	CreateSwapChain();

	CreateRenderTargetView();

	CreateCommandList();

	CreateRootSignature();

	SetViewPort();

}

void DX12Renderer::CreateDebugInterface()
{
	ComPtr<ID3D12Debug> debugController;
	ComPtr<ID3D12Debug3> debugController3;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
	debugController.As(&debugController3);
	if (debugController3)
	{
		debugController3->SetEnableGPUBasedValidation(true);
	}
}

HRESULT DX12Renderer::CreateFactory() {
	HRESULT hr;
	UINT flag;

#if defined(_DEBUG)
	flag |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	hr = CreateDXGIFactory2(flag, IID_PPV_ARGS(&mFactory));

	return hr;
}

HRESULT DX12Renderer::CreateDevice()
{
	HRESULT hr;
	IDXGIAdapter1Ptr adapter;
	hr = mFactory->EnumAdapters1(0, &adapter);
	if (SUCCEEDED(hr))
	{
		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDevice));
	}
	return hr;
}

HRESULT DX12Renderer::CreateCommandQueue()
{
	HRESULT hr;
	D3D12_COMMAND_QUEUE_DESC desc_command_queue;
	ZeroMemory(&desc_command_queue, sizeof(desc_command_queue));
	desc_command_queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	desc_command_queue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	hr = mDevice->CreateCommandQueue(&desc_command_queue, IID_PPV_ARGS(&mCmdQueue));

	mFrameIndex = 0;
	mFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	hr = mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFrameFence));
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed CreateFence");
	}
	return hr;
}

HRESULT DX12Renderer::CreateSwapChain()
{
	HRESULT hr;
	ComPtr<IDXGISwapChain> swapChain;
	DXGI_SWAP_CHAIN_DESC desc_swap_chain;
	ZeroMemory(&desc_swap_chain, sizeof(desc_swap_chain));
	desc_swap_chain.BufferCount = 2;
	desc_swap_chain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc_swap_chain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc_swap_chain.OutputWindow = mHwnd;
	desc_swap_chain.SampleDesc.Count = 1;
	desc_swap_chain.Windowed = TRUE;
	desc_swap_chain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	desc_swap_chain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	hr = mFactory->CreateSwapChain(mCmdQueue, &desc_swap_chain, &swapChain);
	hr = swapChain.As(&mSwapChain);

	hr = mSwapChain->QueryInterface(mSwapChain.GetAddressOf());
	if (FAILED(hr)) {
		return hr;
	}

	mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
	return hr;
}

HRESULT DX12Renderer::CreateCommandList()
{
	HRESULT hr;
	hr = mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdAllocator));
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed CreateCommandAllocator");
	}

	hr = mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator, nullptr, IID_PPV_ARGS(&mCmdList));

	return hr;
}

HRESULT DX12Renderer::CreateRenderTargetView()
{
	HRESULT hr;
	D3D12_DESCRIPTOR_HEAP_DESC desc_heap;
	ZeroMemory(&desc_heap, sizeof(desc_heap));
	desc_heap.NumDescriptors = 2;
	desc_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = mDevice->CreateDescriptorHeap(&desc_heap, IID_PPV_ARGS(&mDescriptorHeap));
	if (FAILED(hr)) {
		return FALSE;
	}

	UINT strideHandleBytes = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (UINT i = 0; i < FrameBufferCount; ++i) {
		hr = mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mRenderTarget[i]));
		if (FAILED(hr)) {
			return FALSE;
		}
		mRTVHandle[i] = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		mRTVHandle[i].ptr += i * strideHandleBytes;
		mDevice->CreateRenderTargetView(mRenderTarget[i], nullptr, mRTVHandle[i]);
	}
	return hr;
}

HRESULT DX12Renderer::CreateRootSignature()
{
	HRESULT hr;

	CD3DX12_DESCRIPTOR_RANGE cbv;
	cbv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0 レジスタ

	CD3DX12_ROOT_PARAMETER rootParams;
	rootParams.InitAsDescriptorTable(1, &cbv, D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_ROOT_SIGNATURE_DESC  descmRootSignature{};
	descmRootSignature.Init(
		1,
		&rootParams,
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	descmRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ComPtr<ID3DBlob> root_sig_blob, error_blob;
	hr = D3D12SerializeRootSignature(&descmRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &root_sig_blob, &error_blob);
	hr = mDevice->CreateRootSignature(0, root_sig_blob->GetBufferPointer(), root_sig_blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));

	return hr;
}

void DX12Renderer::SetViewPort()
{
	mViewPort.TopLeftX = 0;
	mViewPort.TopLeftY = 0;
	mViewPort.Width = (FLOAT)mWidth;
	mViewPort.Height = (FLOAT)mHeight;
	mViewPort.MinDepth = 0;
	mViewPort.MaxDepth = 1;
}

// Assets
BOOL DX12Renderer::LoadAssets() {

	LoadVertexShader();
	LoadPixelShader();

	CreatePipelineObject();

	mSquareList.clear();
	mSquareList.push_back(new Square());
	mSquareList.push_back(new Square());
	mSquareList.push_back(new Square());
	for (int i = 0; i < mSquareList.size(); i++)
	{
		mSquareList[i]->Initialize();
	}
	mSquareList[1]->SetPositionX(-0.525);
	mSquareList[2]->SetPositionX(0.525);
	
	return TRUE;
}

BOOL DX12Renderer::LoadVertexShader()
{
	FILE* fpVS = nullptr;
	std::wstring path = GetExecutionDirectory();
	path += std::wstring(L"\\");
	path += L"VertexShader.cso";

	char input[1024];
	const wchar_t *inputw = path.c_str();
	wcstombs(input, inputw, sizeof(wchar_t)*int(path.size()));

	fopen_s(&fpVS, input, "rb");
	if (!fpVS) {
		return FALSE;
	}
	fseek(fpVS, 0, SEEK_END);
	mVertexShader.size = ftell(fpVS);
	rewind(fpVS);
	mVertexShader.binaryPtr = malloc(mVertexShader.size);
	fread(mVertexShader.binaryPtr, 1, mVertexShader.size, fpVS);
	fclose(fpVS);
	fpVS = nullptr;

	return TRUE;
}

BOOL DX12Renderer::LoadPixelShader()
{
	FILE* fpPS = nullptr;

	std::wstring path = GetExecutionDirectory();
	path += std::wstring(L"\\");
	path += L"PixelShader.cso";

	char input[1024];
	const wchar_t *inputw = path.c_str();
	wcstombs(input, inputw, sizeof(wchar_t)*int(path.size()));

	fopen_s(&fpPS, input, "rb");
	if (!fpPS) {
		return FALSE;
	}
	fseek(fpPS, 0, SEEK_END);
	mPixelShader.size = ftell(fpPS);
	rewind(fpPS);
	mPixelShader.binaryPtr = malloc(mPixelShader.size);
	fread(mPixelShader.binaryPtr, 1, mPixelShader.size, fpPS);
	fclose(fpPS);
	fpPS = nullptr;

	return TRUE;
}

HRESULT DX12Renderer::CreatePipelineObject()
{
	HRESULT hr;

	D3D12_INPUT_ELEMENT_DESC desc_input_elements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",	  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.ForcedSampleCount = 0;
	rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC descmPipelineState;
	ZeroMemory(&descmPipelineState, sizeof(descmPipelineState));
	descmPipelineState.VS.pShaderBytecode = mVertexShader.binaryPtr;
	descmPipelineState.VS.BytecodeLength = mVertexShader.size;
	descmPipelineState.PS.pShaderBytecode = mPixelShader.binaryPtr;
	descmPipelineState.PS.BytecodeLength = mPixelShader.size;
	descmPipelineState.SampleDesc.Count = 1;
	descmPipelineState.SampleMask = UINT_MAX;
	descmPipelineState.InputLayout = { desc_input_elements, _countof(desc_input_elements) };
	descmPipelineState.pRootSignature = mRootSignature;
	descmPipelineState.NumRenderTargets = 1;
	descmPipelineState.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descmPipelineState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descmPipelineState.RasterizerState = rasterDesc;
	descmPipelineState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descmPipelineState.DepthStencilState.DepthEnable = FALSE;
	hr = mDevice->CreateGraphicsPipelineState(&descmPipelineState, IID_PPV_ARGS(&mPipelineState));
	
	return hr;
}
