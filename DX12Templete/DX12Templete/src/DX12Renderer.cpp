#define _CRT_SECURE_NO_WARNINGS

#include "DX12Renderer.h"
#include "utility.h"

DX12Renderer::DX12Renderer()
: mHeight(0)
, mWidth(0)
, mHwnd(nullptr)
, mFenceEvent(nullptr)
, mVertexShader()
, mFenceIndex(0)
, mIndexCount(0)
, mSrvCbvDescriptorSize(0)
, mRadian(0.0f)
{

	mRTVHandle->ptr = 0;

	mIndexBufferView.BufferLocation = 0;
	mIndexBufferView.Format = DXGI_FORMAT_UNKNOWN;
	mIndexBufferView.SizeInBytes = 0;

	mVertexBufferView.BufferLocation = 0;
	mVertexBufferView.SizeInBytes = 0;
	mVertexBufferView.StrideInBytes = 0;

	mViewPort.TopLeftX = 0;
	mViewPort.TopLeftY = 0;
	mViewPort.Width = (FLOAT)0;
	mViewPort.Height = (FLOAT)0;
	mViewPort.MinDepth = 0;
	mViewPort.MaxDepth = 0;
}

DX12Renderer::~DX12Renderer() {
	if (mVertexShader.binaryPtr) {
		free(mVertexShader.binaryPtr);
	}
	if (mPixelShader.binaryPtr) {
		free(mPixelShader.binaryPtr);
	}
	Destroy();
}

void DX12Renderer::Initialize(HWND hwnd, int Width, int Height)
{
	mHwnd = hwnd;
	mWidth = Width;
	mHeight = Height;

	LoadPipeline();
	LoadAssets();
}

void DX12Renderer::Update()
{
	mRadian += 1.0f;
}

void DX12Renderer::Render() {

	ShaderParameters shaderParams;
	XMStoreFloat4x4(&shaderParams.mtxWorld, XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(mRadian)));
	XMMATRIX mtxView = XMMatrixLookAtLH(
		XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), // Eye Position
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),  // Eye Direction
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)   // Eye Up
	);
	XMMATRIX mtxProj = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(45.0f), 
		mViewPort.Width / mViewPort.Height, 
		0.1f, 
		100.0f
	);

	XMStoreFloat4x4(&shaderParams.mtxView, XMMatrixTranspose(mtxView));
	XMStoreFloat4x4(&shaderParams.mtxProj, XMMatrixTranspose(mtxProj));

	auto& constantBuffer = mConstantBuffers[mFenceIndex];
	{
		void* p;
		CD3DX12_RANGE range(0, 0);
		constantBuffer->Map(0, &range, &p);
		memcpy(p, &shaderParams, sizeof(shaderParams));
		constantBuffer->Unmap(0, nullptr);
	}


	float clearColor[4] = { 0.2f, 0.5f, 0.7f, 0.0f };

	mFenceIndex = mSwapChain->GetCurrentBackBufferIndex();

	mCommandAllocators[mFenceIndex]->Reset();
	mCommandList->Reset(mCommandAllocators[mFenceIndex], mPipelineState.Get());

	SetResourceBarrier(
		mCommandList,
		mRenderTarget[mFenceIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	// レンダーターゲットのクリア処理.
	mCommandList->RSSetViewports(1, &mViewPort);
	mCommandList->ClearRenderTargetView(mRTVHandle[mFenceIndex], clearColor, 0, nullptr);

	D3D12_RECT rect = { 0, 0, mWidth, mHeight };
	mCommandList->RSSetScissorRects(1, &rect);
	mCommandList->OMSetRenderTargets(1, &mRTVHandle[mFenceIndex], TRUE, nullptr);

	ID3D12DescriptorHeap* heaps[] = {
	  mHeapSrvCbv.Get()
	};
	mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	//  シェーダー設定
	mCommandList->SetPipelineState(mPipelineState.Get());
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	mCommandList->IASetIndexBuffer(&mIndexBufferView);
	mCommandList->SetGraphicsRootDescriptorTable(0, mCbViews[mFenceIndex]);
	mCommandList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);

	SetResourceBarrier(
		mCommandList,
		mRenderTarget[mFenceIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	mCommandList->Close();

	// 積んだコマンドの実行.
	ID3D12CommandList* pCommandList = mCommandList;
	mCommandQueue->ExecuteCommandLists(1, &pCommandList);
	mSwapChain->Present(1, 0);

	WaitForCommandQueue();
}

void DX12Renderer::Destroy()
{
	WaitForCommandQueue();
	CloseHandle(mFenceEvent);
}

void DX12Renderer::LoadPipeline()
{
	HRESULT hr;

#if defined(_DEBUG)
	CreateDebugInterface();
#endif

	IDXGIFactory4Ptr factory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
	if (FAILED(hr)) {
		return;
	}

	IDXGIAdapter1Ptr adapter;
	hr = factory->EnumAdapters1(0, &adapter);
	if (FAILED(hr)) {
		return;
	}

	hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDevice));
	if (FAILED(hr)) {
		return;
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5;
	hr = mDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
	if (FAILED(hr) || features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
	{
		return;
	}

	D3D12_COMMAND_QUEUE_DESC descmCommandQueue;
	ZeroMemory(&descmCommandQueue, sizeof(descmCommandQueue));
	descmCommandQueue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	descmCommandQueue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	hr = mDevice->CreateCommandQueue(&descmCommandQueue, IID_PPV_ARGS(&mCommandQueue));
	if (FAILED(hr)) {
		return;
	}

	ComPtr<IDXGISwapChain> swapChain;
	DXGI_SWAP_CHAIN_DESC descmSwapChain;
	ZeroMemory(&descmSwapChain, sizeof(descmSwapChain));
	descmSwapChain.BufferCount = 2;
	descmSwapChain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	descmSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	descmSwapChain.OutputWindow = mHwnd;
	descmSwapChain.SampleDesc.Count = 1;
	descmSwapChain.Windowed = TRUE;
	descmSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	descmSwapChain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	hr = factory->CreateSwapChain(mCommandQueue, &descmSwapChain, &swapChain);
	hr = swapChain->QueryInterface(IID_PPV_ARGS(&mSwapChain));
	if (FAILED(hr)) {
		return;
	}

	CreateCommandAllocators();

	PrepareDescriptorHeaps();

	PrepareRenderTargetView();

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

void DX12Renderer::SetResourceBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER descBarrier;
	ZeroMemory(&descBarrier, sizeof(descBarrier));
	descBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	descBarrier.Transition.pResource = resource;
	descBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	descBarrier.Transition.StateBefore = before;
	descBarrier.Transition.StateAfter = after;
	commandList->ResourceBarrier(1, &descBarrier);
}

void DX12Renderer::WaitForCommandQueue() 
{
	UINT nextIndex = (mFenceIndex + 1) % FrameBufferCount;
	UINT64 currentValue = ++mFrameFencesVelues[mFenceIndex];
	UINT64 finishExpected = mFrameFencesVelues[nextIndex];
	UINT64 nextFenceValue = mFrameFences[nextIndex]->GetCompletedValue();

	mCommandQueue->Signal(mFrameFences[mFenceIndex], currentValue);
	if (nextFenceValue < finishExpected)
	{
		mFrameFences[nextIndex]->SetEventOnCompletion(finishExpected, mFenceEvent);
		WaitForSingleObject(mFenceEvent, GpuWaitTimeout);
	}
}

BOOL DX12Renderer::LoadAssets() {

	CreateRootSignature();

	LoadVertexShader();
	LoadPixelShader();

	CreatePipelineObject();

	CreateCommandList();

	PrepareDescriptorHeapForCubeApp();

	//  頂点情報
	const float k = 0.25;
	Vertex vertices_array[] = {
		{ { -k, -k, 0.0f }, { 1.0f, 0.0f,0.0f,1.0f} },
		{ { -k,  k, 0.0f }, { 0.0f, 1.0f,0.0f,1.0f} },
		{ {  k,  k, 0.0f }, { 0.0f, 0.0f,1.0f,1.0f} },
		{ {  k, -k, 0.0f }, { 0.0f, 1.0f,1.0f,1.0f} },
	};
	uint32_t indices[] = {
		0, 1, 2,
		3, 0, 2
	};

	mVertexBuffer = CreateBuffer(sizeof(vertices_array), vertices_array);
	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.StrideInBytes = sizeof(Vertex);
	mVertexBufferView.SizeInBytes = sizeof(vertices_array);

	mIndexBuffer = CreateBuffer(sizeof(indices), indices);
	mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
	mIndexBufferView.SizeInBytes = sizeof(indices);
	mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;

	mIndexCount = _countof(indices);

	mConstantBuffers.resize(FrameBufferCount);
	mCbViews.resize(FrameBufferCount);
	for (UINT i = 0; i < FrameBufferCount; ++i)
	{
		UINT bufferSize = sizeof(ShaderParameters) + 255 & ~255;
		mConstantBuffers[i] = CreateBuffer(bufferSize, nullptr);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc{};
		cbDesc.BufferLocation = mConstantBuffers[i]->GetGPUVirtualAddress();
		cbDesc.SizeInBytes = bufferSize;
		CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(mHeapSrvCbv->GetCPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase + i, mSrvCbvDescriptorSize);
		mDevice->CreateConstantBufferView(&cbDesc, handleCBV);

		mCbViews[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(mHeapSrvCbv->GetGPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase + i, mSrvCbvDescriptorSize);
	}

	CreateFence();

	SetViewPort();

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

	if (mVertexShader.binaryPtr != 0)
	{
		fread(mVertexShader.binaryPtr, 1, mVertexShader.size, fpVS);
	}
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
	if (mPixelShader.binaryPtr != 0)
	{
		fread(mPixelShader.binaryPtr, 1, mPixelShader.size, fpPS);
	}
	fclose(fpPS);
	fpPS = nullptr;

	return TRUE;
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
	descmPipelineState.pRootSignature = mRootSignature.Get();
	descmPipelineState.NumRenderTargets = 1;
	descmPipelineState.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descmPipelineState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descmPipelineState.RasterizerState = rasterDesc;
	descmPipelineState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descmPipelineState.DepthStencilState.DepthEnable = FALSE;
	hr = mDevice->CreateGraphicsPipelineState(&descmPipelineState, IID_PPV_ARGS(mPipelineState.GetAddressOf()));
	
	return hr;
}

HRESULT DX12Renderer::CreateCommandList()
{
	HRESULT hr;
	hr = mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators[0], mPipelineState.Get(), IID_PPV_ARGS(&mCommandList));
	mCommandList->Close();

	return hr;
}

HRESULT DX12Renderer::CreateCommandAllocators()
{
	HRESULT hr;
	mCommandAllocators.resize(FrameBufferCount);
	for (UINT i = 0; i < FrameBufferCount; ++i)
	{
		hr = mDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&mCommandAllocators[i])
		);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed CreateCommandAllocator");
		}
	}
	return hr;
}

HRESULT DX12Renderer::PrepareDescriptorHeaps()
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
	return hr;
}

HRESULT DX12Renderer::PrepareRenderTargetView()
{
	HRESULT hr;
	UINT strideHandleBytes = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (UINT i = 0; i < FrameBufferCount; ++i) {
		hr = mSwapChain->GetBuffer(i, IID_PPV_ARGS(mRenderTarget[i].GetAddressOf()));
		if (FAILED(hr)) {
			return FALSE;
		}
		mRTVHandle[i] = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		mRTVHandle[i].ptr += SIZE_T(i) * SIZE_T(strideHandleBytes);
		mDevice->CreateRenderTargetView(mRenderTarget[i].Get(), nullptr, mRTVHandle[i]);
	}
	return hr;
}

HRESULT DX12Renderer::PrepareDescriptorHeapForCubeApp()
{
	HRESULT hr;
	UINT count = FrameBufferCount + 1;
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
	  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	  count,
	  D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	  0
	};
	hr = mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mHeapSrvCbv));
	if (FAILED(hr)) {
		return FALSE;
	}
	mSrvCbvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	return hr;
}

HRESULT DX12Renderer::CreateFence()
{
	HRESULT hr;
	mFenceIndex = 0;
	mFrameFences.resize(FrameBufferCount);
	mFrameFencesVelues.resize(FrameBufferCount);
	mFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	for (UINT i = 0; i < FrameBufferCount; ++i)
	{
		hr = mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFrameFences[i]));
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed CreateFence");
		}
	}
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

ComPtr<ID3D12Resource1> DX12Renderer::CreateBuffer(UINT bufferSize, const void* initialData)
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

void DX12Renderer::d3dTraceHR(const std::string& msg, HRESULT hr)
{
	char hr_msg[512];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, hr_msg, ARRAYSIZE(hr_msg), nullptr);

	std::string error_msg = msg + ".\nError! " + hr_msg;
	msgBox(error_msg);
}

void DX12Renderer::msgBox(const std::string& msg)
{
	MessageBoxA(mHwnd, msg.c_str(), "Error", MB_OK);
}