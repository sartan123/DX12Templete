#define _CRT_SECURE_NO_WARNINGS
#include "DX12Renderer.h"
#include "utility.h"

ComPtr<ID3D12Device5> DX12Renderer::device = nullptr;
ComPtr<ID3D12GraphicsCommandList> DX12Renderer::_command_list = nullptr;

DX12Renderer::~DX12Renderer() {
	if (_g_vertex_shader.binaryPtr) {
		free(_g_vertex_shader.binaryPtr);
	}
	if (_g_pixel_shader.binaryPtr) {
		free(_g_pixel_shader.binaryPtr);
	}
	Destroy();
}

void DX12Renderer::Update()
{

}

void DX12Renderer::WaitForCommandQueue()
{
	UINT nextIndex = (_frame_index + 1) % FrameBufferCount;
	UINT64 currentValue = ++_frame_fence_values;
	UINT64 finishExpected = _frame_fence_values;
	UINT64 nextFenceValue = _frame_fences->GetCompletedValue();

	_command_queue->Signal(_frame_fences.Get(), currentValue);
	if (nextFenceValue < finishExpected)
	{
		_frame_fences->SetEventOnCompletion(finishExpected, _fence_event);
		WaitForSingleObject(_fence_event, GpuWaitTimeout);
	}
}

void DX12Renderer::Destroy()
{
	WaitForCommandQueue();
	CloseHandle(_fence_event);
}

// Render
void DX12Renderer::Render()
{
	PopulateCommandList();

	// 積んだコマンドの実行.
	ID3D12CommandList* pCommandList = _command_list.Get();
	_command_queue->ExecuteCommandLists(1, &pCommandList);

	WaitForCommandQueue();

	_command_allocators->Reset();
	_command_list->Reset(_command_allocators.Get(), _pipeline_state.Get());

	_swap_chain->Present(1, 0);

	_frame_index = _swap_chain->GetCurrentBackBufferIndex();
}

void DX12Renderer::PopulateCommandList()
{
	float clearColor[4] = { 0.2f, 0.5f, 0.7f, 0.0f };

	SetResourceBarrier(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// レンダーターゲットのクリア処理.
	_command_list->ClearRenderTargetView(_rtv_handle[_frame_index], clearColor, 0, nullptr);

	_command_list->SetGraphicsRootSignature(_root_signature.Get());
	_command_list->SetPipelineState(_pipeline_state.Get());

	D3D12_RECT rect = { 0, 0, mWidth, mHeight };
	_command_list->RSSetViewports(1, &_viewport);
	_command_list->RSSetScissorRects(1, &rect);

	_command_list->OMSetRenderTargets(1, &_rtv_handle[_frame_index], TRUE, nullptr);

	object->draw();

	SetResourceBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	_command_list->Close();
}

void DX12Renderer::SetResourceBarrier(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER descBarrier;
	ZeroMemory(&descBarrier, sizeof(descBarrier));
	descBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	descBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	descBarrier.Transition.pResource = _render_target[_frame_index].Get();
	descBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	descBarrier.Transition.StateBefore = before;
	descBarrier.Transition.StateAfter = after;
	_command_list->ResourceBarrier(1, &descBarrier);
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

	hr = CreateDXGIFactory2(flag, IID_PPV_ARGS(&factory));

	return hr;
}

HRESULT DX12Renderer::CreateDevice()
{
	HRESULT hr;
	IDXGIAdapter1Ptr adapter;
	hr = factory->EnumAdapters1(0, &adapter);
	if (SUCCEEDED(hr))
	{
		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
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
	hr = device->CreateCommandQueue(&desc_command_queue, IID_PPV_ARGS(&_command_queue));

	_frame_index = 0;
	_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_frame_fences));
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
	hr = factory->CreateSwapChain(_command_queue.Get(), &desc_swap_chain, &swapChain);
	hr = swapChain.As(&_swap_chain);

	hr = _swap_chain->QueryInterface(_swap_chain.GetAddressOf());
	if (FAILED(hr)) {
		return hr;
	}

	_frame_index = _swap_chain->GetCurrentBackBufferIndex();
	return hr;
}

HRESULT DX12Renderer::CreateCommandList()
{
	HRESULT hr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_command_allocators));
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed CreateCommandAllocator");
	}

	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocators.Get(), nullptr, IID_PPV_ARGS(&_command_list));

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
	hr = device->CreateDescriptorHeap(&desc_heap, IID_PPV_ARGS(&_descriptor_heap));
	if (FAILED(hr)) {
		return FALSE;
	}

	UINT strideHandleBytes = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (UINT i = 0; i < FrameBufferCount; ++i) {
		hr = _swap_chain->GetBuffer(i, IID_PPV_ARGS(_render_target[i].GetAddressOf()));
		if (FAILED(hr)) {
			return FALSE;
		}
		_rtv_handle[i] = _descriptor_heap->GetCPUDescriptorHandleForHeapStart();
		_rtv_handle[i].ptr += i * strideHandleBytes;
		device->CreateRenderTargetView(_render_target[i].Get(), nullptr, _rtv_handle[i]);
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

	CD3DX12_ROOT_SIGNATURE_DESC  desc_root_signature{};
	desc_root_signature.Init(
		1,
		&rootParams,
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	desc_root_signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ComPtr<ID3DBlob> root_sig_blob, error_blob;
	hr = D3D12SerializeRootSignature(&desc_root_signature, D3D_ROOT_SIGNATURE_VERSION_1, &root_sig_blob, &error_blob);
	hr = device->CreateRootSignature(0, root_sig_blob->GetBufferPointer(), root_sig_blob->GetBufferSize(), IID_PPV_ARGS(&_root_signature));

	return hr;
}

void DX12Renderer::SetViewPort()
{
	_viewport.TopLeftX = 0;
	_viewport.TopLeftY = 0;
	_viewport.Width = (FLOAT)mWidth;
	_viewport.Height = (FLOAT)mHeight;
	_viewport.MinDepth = 0;
	_viewport.MaxDepth = 1;
}

// Assets
BOOL DX12Renderer::LoadAssets() {

	LoadVertexShader();
	LoadPixelShader();

	CreatePipelineObject();

	object = new Square();
	object->Initialize();
	
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
	_g_vertex_shader.size = ftell(fpVS);
	rewind(fpVS);
	_g_vertex_shader.binaryPtr = malloc(_g_vertex_shader.size);
	fread(_g_vertex_shader.binaryPtr, 1, _g_vertex_shader.size, fpVS);
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
	_g_pixel_shader.size = ftell(fpPS);
	rewind(fpPS);
	_g_pixel_shader.binaryPtr = malloc(_g_pixel_shader.size);
	fread(_g_pixel_shader.binaryPtr, 1, _g_pixel_shader.size, fpPS);
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_pipeline_state;
	ZeroMemory(&desc_pipeline_state, sizeof(desc_pipeline_state));
	desc_pipeline_state.VS.pShaderBytecode = _g_vertex_shader.binaryPtr;
	desc_pipeline_state.VS.BytecodeLength = _g_vertex_shader.size;
	desc_pipeline_state.PS.pShaderBytecode = _g_pixel_shader.binaryPtr;
	desc_pipeline_state.PS.BytecodeLength = _g_pixel_shader.size;
	desc_pipeline_state.SampleDesc.Count = 1;
	desc_pipeline_state.SampleMask = UINT_MAX;
	desc_pipeline_state.InputLayout = { desc_input_elements, _countof(desc_input_elements) };
	desc_pipeline_state.pRootSignature = _root_signature.Get();
	desc_pipeline_state.NumRenderTargets = 1;
	desc_pipeline_state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc_pipeline_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc_pipeline_state.RasterizerState = rasterDesc;
	desc_pipeline_state.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc_pipeline_state.DepthStencilState.DepthEnable = FALSE;
	hr = device->CreateGraphicsPipelineState(&desc_pipeline_state, IID_PPV_ARGS(_pipeline_state.GetAddressOf()));
	
	return hr;
}
