#define _CRT_SECURE_NO_WARNINGS

#include "DX12Renderer.h"
#include "utility.h"

DX12Renderer::DX12Renderer(HWND hwnd, int Width, int Height) :
	mHwnd(hwnd),
	mWidth(Width),
	mHeight(Height),
	Frames(0)
{
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif
	CreateFactory();
	CreateDevice();
	CreateCommandAllocator();
	CreateCommandQueue();
	CreateSwapChain();
	CreateDescriptorHeap();
	CreateRenderTarget();
	CreateDepthStencilBuffer();
	CreateCommandList();
	CreateBuffer();
	CreateRootSignature();
	CompileShader();
	LoadVertexShader();
	LoadPixelShader();;
	CreatePipelineStateObject();

	SetVertexData();

	Viewport.TopLeftX = 0.f;
	Viewport.TopLeftY = 0.f;
	Viewport.Width = (FLOAT)mWidth;
	Viewport.Height = (FLOAT)mHeight;
	Viewport.MinDepth = 0.f;
	Viewport.MaxDepth = 1.f;

	RectScissor.top = 0;
	RectScissor.left = 0;
	RectScissor.right = mWidth;
	RectScissor.bottom = mHeight;
}

DX12Renderer::~DX12Renderer() {
	CloseHandle(_fence_event);
}

BOOL DX12Renderer::CreateFactory()
{
	UINT flagsDXGI = 0;
	HRESULT hr;

	hr = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(_factory.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		return FALSE;
	}
	hr = _factory->EnumAdapters(0, adapter.GetAddressOf());
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL DX12Renderer::CreateDevice()
{
	HRESULT hr;
	hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()));
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL DX12Renderer::CreateCommandAllocator()
{
	HRESULT hr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_command_allocator.GetAddressOf()));
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL DX12Renderer::CreateCommandList()
{
	HRESULT hr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocator.Get(), nullptr, IID_PPV_ARGS(_command_list.GetAddressOf()));
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL DX12Renderer::CreateDescriptorHeap()
{
	HRESULT hr;
	D3D12_DESCRIPTOR_HEAP_DESC desc_heap;
	ZeroMemory(&desc_heap, sizeof(desc_heap));
	desc_heap.NumDescriptors = 2;
	desc_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&desc_heap, IID_PPV_ARGS(_descriptor_heap.GetAddressOf()));
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL DX12Renderer::CreateRenderTarget()
{
	HRESULT hr;
	UINT strideHandleBytes = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (UINT i = 0; i < RTV_NUM; ++i) {
		hr = _swap_chain->GetBuffer(i, IID_PPV_ARGS(_render_target[i].GetAddressOf()));
		if (FAILED(hr)) {
			return FALSE;
		}
		_rtv_handle[i] = _descriptor_heap->GetCPUDescriptorHandleForHeapStart();
		_rtv_handle[i].ptr += i * strideHandleBytes;
		device->CreateRenderTargetView(_render_target[i].Get(), nullptr, _rtv_handle[i]);
	}
	return TRUE;
}

BOOL DX12Renderer::CreateCommandQueue()
{
	HRESULT hr;
	D3D12_COMMAND_QUEUE_DESC desc_command_queue;
	ZeroMemory(&desc_command_queue, sizeof(desc_command_queue));
	desc_command_queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	desc_command_queue.Priority = 0;
	desc_command_queue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc_command_queue.NodeMask = 0;
	hr = device->CreateCommandQueue(&desc_command_queue, IID_PPV_ARGS(_command_queue.GetAddressOf()));
	if (FAILED(hr)) {
		return FALSE;
	}
	_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_queue_fence.GetAddressOf()));
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL DX12Renderer::CreateSwapChain()
{
	HRESULT hr;
	DXGI_SWAP_CHAIN_DESC desc_swap_chain;
	ZeroMemory(&desc_swap_chain, sizeof(desc_swap_chain));
	desc_swap_chain.BufferCount = 2;
	desc_swap_chain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc_swap_chain.BufferDesc.Width = mWidth;
	desc_swap_chain.BufferDesc.Height = mHeight;
	desc_swap_chain.OutputWindow = mHwnd;
	desc_swap_chain.Windowed = TRUE;
	desc_swap_chain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	desc_swap_chain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	desc_swap_chain.BufferDesc.RefreshRate.Numerator = 60;
	desc_swap_chain.BufferDesc.RefreshRate.Denominator = 1;
	desc_swap_chain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc_swap_chain.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc_swap_chain.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	desc_swap_chain.SampleDesc.Count = 1;
	desc_swap_chain.SampleDesc.Quality = 0;
	hr = _factory->CreateSwapChain(_command_queue.Get(), &desc_swap_chain, (IDXGISwapChain**)_swap_chain.GetAddressOf());
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL DX12Renderer::CreateBuffer()
{
	HRESULT hr;

	D3D12_HEAP_PROPERTIES HeapPropeties;
	D3D12_RESOURCE_DESC   ResourceDesc;
	ZeroMemory(&HeapPropeties, sizeof(HeapPropeties));
	ZeroMemory(&ResourceDesc, sizeof(ResourceDesc));
	HeapPropeties.Type = D3D12_HEAP_TYPE_UPLOAD;
	HeapPropeties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapPropeties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapPropeties.CreationNodeMask = 0;
	HeapPropeties.VisibleNodeMask = 0;

	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Width = 256;
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;

	//定数バッファの作成
	hr = device->CreateCommittedResource(&HeapPropeties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_const_buffer));
	if (FAILED(hr)) {
		return FALSE;
	}

	//インデックスバッファの作成
	hr = device->CreateCommittedResource(&HeapPropeties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_index_buffer));
	if (FAILED(hr)) {
		return FALSE;
	}

	//頂点バッファの作成
	hr = device->CreateCommittedResource(&HeapPropeties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_vertex_buffer));
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL DX12Renderer::CreateDepthStencilBuffer()
{
	HRESULT hr;

	D3D12_DESCRIPTOR_HEAP_DESC descDescriptorHeapDSB;
	ZeroMemory(&descDescriptorHeapDSB, sizeof(descDescriptorHeapDSB));
	descDescriptorHeapDSB.NumDescriptors = 1;
	descDescriptorHeapDSB.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	descDescriptorHeapDSB.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descDescriptorHeapDSB.NodeMask = 0;
	hr = device->CreateDescriptorHeap(&descDescriptorHeapDSB, IID_PPV_ARGS(&_descriptor_heap));
	if (FAILED(hr)) {
		return FALSE;
	}

	D3D12_RESOURCE_DESC DepthDesc;
	D3D12_HEAP_PROPERTIES HeapProps;
	D3D12_CLEAR_VALUE ClearValue;
	ZeroMemory(&DepthDesc, sizeof(DepthDesc));
	ZeroMemory(&HeapProps, sizeof(HeapProps));
	ZeroMemory(&ClearValue, sizeof(ClearValue));

	DepthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DepthDesc.Width = mWidth;
	DepthDesc.Height = mHeight;
	DepthDesc.DepthOrArraySize = 1;
	DepthDesc.MipLevels = 0;
	DepthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	DepthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.SampleDesc.Quality = 0;
	DepthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 0;
	HeapProps.VisibleNodeMask = 0;

	ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	ClearValue.DepthStencil.Depth = 1.0f;
	ClearValue.DepthStencil.Stencil = 0;

	hr = device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &DepthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &ClearValue, IID_PPV_ARGS(&_depth_buffer));
	if (FAILED(hr)) {
		return FALSE;
	}


	D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc;
	ZeroMemory(&DSVDesc, sizeof(DSVDesc));
	DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;
	DSVDesc.Flags = D3D12_DSV_FLAG_NONE;

	handleDSV = _descriptor_heap->GetCPUDescriptorHandleForHeapStart();

	device->CreateDepthStencilView(_depth_buffer.Get(), &DSVDesc, handleDSV);

	return TRUE;
}

void DX12Renderer::SetResourceBarrier(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER descBarrier;
	ZeroMemory(&descBarrier, sizeof(descBarrier));
	descBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	descBarrier.Transition.pResource = _render_target[rtv_index_].Get();
	descBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	descBarrier.Transition.StateBefore = before;
	descBarrier.Transition.StateAfter = after;
	_command_list->ResourceBarrier(1, &descBarrier);
}

BOOL DX12Renderer::CreateRootSignature()
{
	HRESULT hr;
	// PipelineStateのための RootSignature の作成.
	D3D12_ROOT_SIGNATURE_DESC desc_root_signature;
	ZeroMemory(&desc_root_signature, sizeof(desc_root_signature));

	desc_root_signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ID3DBlob *root_sig_blob, *error_blob;
	hr = D3D12SerializeRootSignature(&desc_root_signature, D3D_ROOT_SIGNATURE_VERSION_1, &root_sig_blob, &error_blob );
	if (FAILED(hr)) {
		return FALSE;
	}
	hr = device->CreateRootSignature(0, root_sig_blob->GetBufferPointer(), root_sig_blob->GetBufferSize(), IID_PPV_ARGS(&_root_signature));
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

void DX12Renderer::WaitForCommandQueue(ID3D12CommandQueue* pCommandQueue) {
	static UINT64 frames = 0;
	_queue_fence->SetEventOnCompletion(frames, _fence_event);
	pCommandQueue->Signal(_queue_fence.Get(), frames);
	WaitForSingleObject(_fence_event, INFINITE);
	frames++;
}

BOOL DX12Renderer::CreatePipelineStateObject()
{	
	HRESULT hr;

	// 頂点レイアウト.
	D3D12_INPUT_ELEMENT_DESC InputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc;
	ZeroMemory(&pipeline_state_desc, sizeof(pipeline_state_desc));

	//シェーダーの設定
	pipeline_state_desc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
	pipeline_state_desc.VS.BytecodeLength = vertex_shader->GetBufferSize();
	pipeline_state_desc.HS.pShaderBytecode = nullptr;
	pipeline_state_desc.HS.BytecodeLength = 0;
	pipeline_state_desc.DS.pShaderBytecode = nullptr;
	pipeline_state_desc.DS.BytecodeLength = 0;
	pipeline_state_desc.GS.pShaderBytecode = nullptr;
	pipeline_state_desc.GS.BytecodeLength = 0;
	pipeline_state_desc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
	pipeline_state_desc.PS.BytecodeLength = pixel_shader->GetBufferSize();


	//インプットレイアウトの設定
	pipeline_state_desc.InputLayout.pInputElementDescs = InputElementDesc;
	pipeline_state_desc.InputLayout.NumElements = _countof(InputElementDesc);


	//サンプル系の設定
	pipeline_state_desc.SampleDesc.Count = 1;
	pipeline_state_desc.SampleDesc.Quality = 0;
	pipeline_state_desc.SampleMask = UINT_MAX;

	//レンダーターゲットの設定
	pipeline_state_desc.NumRenderTargets = 1;
	pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;

	//三角形に設定
	pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;


	//ルートシグネチャ
	pipeline_state_desc.pRootSignature = _root_signature;


	//ラスタライザステートの設定
	pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//D3D12_CULL_MODE_BACK;
	pipeline_state_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pipeline_state_desc.RasterizerState.FrontCounterClockwise = FALSE;
	pipeline_state_desc.RasterizerState.DepthBias = 0;
	pipeline_state_desc.RasterizerState.DepthBiasClamp = 0;
	pipeline_state_desc.RasterizerState.SlopeScaledDepthBias = 0;
	pipeline_state_desc.RasterizerState.DepthClipEnable = TRUE;
	pipeline_state_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	pipeline_state_desc.RasterizerState.AntialiasedLineEnable = FALSE;
	pipeline_state_desc.RasterizerState.MultisampleEnable = FALSE;


	//ブレンドステートの設定
	int count = _countof(pipeline_state_desc.BlendState.RenderTarget);
	for (int i = 0; i < count; ++i) {
		pipeline_state_desc.BlendState.RenderTarget[i].BlendEnable = FALSE;
		pipeline_state_desc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		pipeline_state_desc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
		pipeline_state_desc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		pipeline_state_desc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		pipeline_state_desc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		pipeline_state_desc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		pipeline_state_desc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pipeline_state_desc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
		pipeline_state_desc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
	}
	pipeline_state_desc.BlendState.AlphaToCoverageEnable = FALSE;
	pipeline_state_desc.BlendState.IndependentBlendEnable = FALSE;


	//デプスステンシルステートの設定
	pipeline_state_desc.DepthStencilState.DepthEnable = TRUE;								//深度テストあり
	pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pipeline_state_desc.DepthStencilState.StencilEnable = FALSE;							//ステンシルテストなし
	pipeline_state_desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	pipeline_state_desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	pipeline_state_desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	pipeline_state_desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	pipeline_state_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	hr = device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_pipeline_state));
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL DX12Renderer::SetVertexData()
{
	HRESULT hr;
	void *Mapped;

	//頂点データをVertexBufferに書き込む
	Vertex3D Vertices[] = {
		{XMFLOAT3(-1.f,  1.f, 0.f), XMFLOAT3(0.f,  0.f, 1.f)},
		{XMFLOAT3(1.f,  1.f, 0.f), XMFLOAT3(0.f,  0.f, 1.f)},
		{XMFLOAT3(1.f, -1.f, 0.f), XMFLOAT3(0.f,  0.f, 1.f)},
		{XMFLOAT3(-1.f, -1.f, 0.f), XMFLOAT3(0.f,  0.f, 1.f)},
	};
	hr = _vertex_buffer->Map(0, nullptr, &Mapped);
	if (SUCCEEDED(hr)) {
		CopyMemory(Mapped, Vertices, sizeof(Vertices));
		_vertex_buffer->Unmap(0, nullptr);
		Mapped = nullptr;
	}
	else {
		return FALSE;
	}
	_vertex_view.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
	_vertex_view.StrideInBytes = sizeof(Vertex3D);
	_vertex_view.SizeInBytes = sizeof(Vertices);


	//インデックスデータをIndexBufferに書き込む
	uint16_t Index[] = { 0, 1, 3, 1, 2, 3 };
	hr = _index_buffer->Map(0, nullptr, &Mapped);
	if (SUCCEEDED(hr)) {
		CopyMemory(Mapped, Index, sizeof(Index));
		_index_buffer->Unmap(0, nullptr);
		Mapped = nullptr;
	}
	else {
		return FALSE;
	}
	_index_view.BufferLocation = _index_buffer->GetGPUVirtualAddress();
	_index_view.SizeInBytes = sizeof(Index);
	_index_view.Format = DXGI_FORMAT_R16_UINT;
	return TRUE;
}

BOOL DX12Renderer::CompileShader()
{
	HRESULT hr;
#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	hr = D3DCompileFromFile(L"Shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertex_shader, nullptr);
	if (FAILED(hr)) {
		return FALSE;
	}

	hr = D3DCompileFromFile(L"Shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixel_shader, nullptr);
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

//  描画
HRESULT DX12Renderer::Render() {
	HRESULT hr;

	SetPlaneData();
	
	PopulateCommandList();

	ID3D12CommandList *const ppCommandList = _command_list.Get();
	_command_queue->ExecuteCommandLists(1, &ppCommandList);

	WaitForPreviousFrame();

	hr = _command_allocator->Reset();
	if (FAILED(hr)) {
		return hr;
	}

	hr = _command_list->Reset(_command_allocator.Get(), nullptr);
	if (FAILED(hr)) {
		return hr;
	}

	hr = _swap_chain->Present(1, 0);
	if (FAILED(hr)) {
		return hr;
	}
	rtv_index_ = _swap_chain->GetCurrentBackBufferIndex();
	return S_OK;
}

void DX12Renderer::SetPlaneData()
{
	HRESULT hr;
	static int Cnt{};
	++Cnt;

	//カメラの設定
	XMMATRIX view = XMMatrixLookAtLH({ 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 640.0f / 480.0f, 1.0f, 20.0f);

	//オブジェクトの回転の設定
	XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(static_cast<float>(Cnt % 360)));
	rotate *= XMMatrixRotationX(XMConvertToRadians(static_cast<float>((Cnt % 1080)) / 3.0f));
	rotate *= XMMatrixRotationZ(XMConvertToRadians(static_cast<float>(Cnt % 1800)) / 5.0f);

	XMFLOAT4X4 Mat;
	XMStoreFloat4x4(&Mat, XMMatrixTranspose(rotate * view * projection));

	XMFLOAT4X4 *buffer{};
	hr = _const_buffer->Map(0, nullptr, (void**)&buffer);

	//行列を定数バッファに書き込み
	*buffer = Mat;

	_const_buffer->Unmap(0, nullptr);
	buffer = nullptr;

	D3D12_VERTEX_BUFFER_VIEW	 vertex_view{};
	vertex_view.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
	vertex_view.StrideInBytes = sizeof(Vertex3D);
	vertex_view.SizeInBytes = sizeof(Vertex3D) * 4;

	//定数バッファをシェーダのレジスタにセット
	_command_list->SetGraphicsRootConstantBufferView(0, _const_buffer->GetGPUVirtualAddress());

	//インデックスを使用しないトライアングルストリップで描画
	_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_command_list->IASetVertexBuffers(0, 1, &vertex_view);

	//描画
	_command_list->DrawInstanced(4, 1, 0, 0);
}

BOOL DX12Renderer::PopulateCommandList()
{
	HRESULT hr;

	FLOAT ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	SetResourceBarrier(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	_command_list->ClearDepthStencilView(handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	_command_list->ClearRenderTargetView(_rtv_handle[rtv_index_], ClearColor, 0, nullptr);

	_command_list->SetGraphicsRootSignature(_root_signature);
	_command_list->SetPipelineState(_pipeline_state.Get());
	_command_list->SetGraphicsRootConstantBufferView(0, _const_buffer->GetGPUVirtualAddress());

	_command_list->RSSetViewports(1, &Viewport);
	_command_list->RSSetScissorRects(1, &RectScissor);

	_command_list->OMSetRenderTargets(1, &_rtv_handle[rtv_index_], TRUE, &handleDSV);

	_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_command_list->IASetVertexBuffers(0, 1, &_vertex_view);
	_command_list->IASetIndexBuffer(&_index_view);

	_command_list->DrawIndexedInstanced(6, 1, 0, 0, 0);

	SetResourceBarrier(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	hr = _command_list->Close();
	if (FAILED(hr)) {
		return FALSE;
	}

	return TRUE;
}

BOOL DX12Renderer::WaitForPreviousFrame()
{
	HRESULT hr;

	const UINT64 fence = Frames;
	hr = _command_queue->Signal(_queue_fence.Get(), fence);
	if (FAILED(hr)) {
		return FALSE;
	}
	++Frames;

	if (_queue_fence->GetCompletedValue() < fence) {
		hr = _queue_fence->SetEventOnCompletion(fence, _fence_event);
		if (FAILED(hr)) {
			return FALSE;
		}
		WaitForSingleObject(_fence_event, INFINITE);
	}
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
