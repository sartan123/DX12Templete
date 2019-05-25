#define _CRT_SECURE_NO_WARNINGS

#include "DX12Renderer.h"
#include "utility.h"

DX12Renderer::DX12Renderer(HWND hwnd, int Width, int Height) :
	mHwnd(hwnd),
	mWidth(Width),
	mHeight(Height) {

	UINT flagsDXGI = 0;
	HRESULT hr;

	hr = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(_factory.ReleaseAndGetAddressOf()));
	if (FAILED(hr)) {
		return;
	}
	ComPtr<IDXGIAdapter> adapter;
	hr = _factory->EnumAdapters(0, adapter.GetAddressOf());
	if (FAILED(hr)) {
		return;
	}
	// �f�o�C�X����.
	// D3D12�� �Œ�ł� D3D_FEATURE_LEVEL_11_0 ��v������悤��.
	hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()));
	if (FAILED(hr)) {
		return;
	}
	// �R�}���h�A���P�[�^�𐶐�.
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_command_allocator.GetAddressOf()));
	if (FAILED(hr)) {
		return;
	}
	// �R�}���h�L���[�𐶐�.
	D3D12_COMMAND_QUEUE_DESC desc_command_queue;
	ZeroMemory(&desc_command_queue, sizeof(desc_command_queue));
	desc_command_queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	desc_command_queue.Priority = 0;
	desc_command_queue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	hr = device->CreateCommandQueue(&desc_command_queue, IID_PPV_ARGS(_command_queue.GetAddressOf()));
	if (FAILED(hr)) {
		return;
	}
	// �R�}���h�L���[�p�̃t�F���X������.
	_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_queue_fence.GetAddressOf()));
	if (FAILED(hr)) {
		return;
	}

	// �X���b�v�`�F�C���𐶐�.
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
	hr = _factory->CreateSwapChain(_command_queue.Get(), &desc_swap_chain, (IDXGISwapChain**)_swap_chain.GetAddressOf());
	if (FAILED(hr)) {
		return;
	}
	// �R�}���h���X�g�̍쐬.
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _command_allocator.Get(), nullptr, IID_PPV_ARGS(_command_list.GetAddressOf()));
	if (FAILED(hr)) {
		return;
	}

	// �f�B�X�N���v�^�q�[�v(RenderTarget�p)�̍쐬.
	D3D12_DESCRIPTOR_HEAP_DESC desc_heap;
	ZeroMemory(&desc_heap, sizeof(desc_heap));
	desc_heap.NumDescriptors = 2;
	desc_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&desc_heap, IID_PPV_ARGS(_descriptor_heap.GetAddressOf()));
	if (FAILED(hr)) {
		return;
	}

	// �����_�[�^�[�Q�b�g(�v���C�}���p)�̍쐬.
	UINT strideHandleBytes = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (UINT i = 0; i < desc_swap_chain.BufferCount; ++i) {
		hr = _swap_chain->GetBuffer(i, IID_PPV_ARGS(_render_target[i].GetAddressOf()));
		if (FAILED(hr)) {
			return;
		}
		_rtv_handle[i] = _descriptor_heap->GetCPUDescriptorHandleForHeapStart();
		_rtv_handle[i].ptr += i * strideHandleBytes;
		device->CreateRenderTargetView(_render_target[i].Get(), nullptr, _rtv_handle[i]);
	}

	ResourceSetup();
}

DX12Renderer::~DX12Renderer() {
	if (_g_vertex_shader.binaryPtr) {
		free(_g_vertex_shader.binaryPtr);
	}
	if (_g_pixel_shader.binaryPtr) {
		free(_g_pixel_shader.binaryPtr);
	}
	CloseHandle(_fence_event);
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

void DX12Renderer::WaitForCommandQueue(ID3D12CommandQueue* pCommandQueue) {
	static UINT64 frames = 0;
	_queue_fence->SetEventOnCompletion(frames, _fence_event);
	pCommandQueue->Signal(_queue_fence.Get(), frames);
	WaitForSingleObject(_fence_event, INFINITE);
	frames++;
}

//  �`��
HRESULT DX12Renderer::Render() {
	float clearColor[4] = { 0.0f,0.0f,0.0f,0.0f };
	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = 0; viewport.TopLeftY = 0;
	viewport.Width = (FLOAT)mWidth;
	viewport.Height = (FLOAT)mHeight;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	int targetIndex = _swap_chain->GetCurrentBackBufferIndex();

	SetResourceBarrier(
		_command_list.Get(),
		_render_target[targetIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	// �����_�[�^�[�Q�b�g�̃N���A����.
	_command_list->RSSetViewports(1, &viewport);
	_command_list->ClearRenderTargetView(_rtv_handle[targetIndex], clearColor, 0, nullptr);
	
	D3D12_RECT rect = { 0, 0, mWidth, mHeight };
	_command_list->RSSetScissorRects(1, &rect);
	_command_list->OMSetRenderTargets(1, &_rtv_handle[targetIndex], TRUE, nullptr);

	_command_list->SetGraphicsRootSignature(_root_signature.Get());
	//  �V�F�[�_�[�ݒ�
	_command_list->SetPipelineState(_pipeline_state.Get());
	_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_command_list->IASetVertexBuffers(0, 1, &_buffer_position);
	_command_list->DrawInstanced(3, 1, 0, 0);

	// Present����O�̏���.
	SetResourceBarrier(
		_command_list.Get(),
		_render_target[targetIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	_command_list->Close();

	// �ς񂾃R�}���h�̎��s.
	ID3D12CommandList* pCommandList = _command_list.Get();
	_command_queue->ExecuteCommandLists(1, &pCommandList);
	_swap_chain->Present(1, 0);

	WaitForCommandQueue(_command_queue.Get());
	_command_allocator->Reset();
	_command_list->Reset(_command_allocator.Get(), nullptr);
	return S_OK;
}

BOOL DX12Renderer::ResourceSetup() {
	HRESULT hr;
	//  ���_���
	MyVertex vertices_array[] = {
		{ 0.0f, 1.0f, 0.0f },
		{ -1.0f,-1.0f, 0.0 },
		{ +1.0f,-1.0f, 0.0 },
	};
	// PipelineState�̂��߂� RootSignature �̍쐬.
	D3D12_ROOT_SIGNATURE_DESC desc_root_signature;
	ZeroMemory(&desc_root_signature, sizeof(desc_root_signature));

	desc_root_signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ComPtr<ID3DBlob> root_sig_blob, error_blob;
	hr = D3D12SerializeRootSignature(&desc_root_signature, D3D_ROOT_SIGNATURE_VERSION_1, root_sig_blob.GetAddressOf(), error_blob.GetAddressOf());
	hr = device->CreateRootSignature(0, root_sig_blob->GetBufferPointer(), root_sig_blob->GetBufferSize(), IID_PPV_ARGS(_root_signature.GetAddressOf()));
	if (FAILED(hr)) {
		return FALSE;
	}

	// �R���p�C���ς݃V�F�[�_�[�̓ǂݍ���.
	LoadVertexShader();
	LoadPixelShader();

	// ����̂��߂̒��_���C�A�E�g.
	D3D12_INPUT_ELEMENT_DESC desc_input_elements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	// PipelineState�I�u�W�F�N�g�̍쐬.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_pipeline_state;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_state;
	ZeroMemory(&desc_pipeline_state, sizeof(desc_pipeline_state));
	desc_pipeline_state.VS.pShaderBytecode = _g_vertex_shader.binaryPtr;
	desc_pipeline_state.VS.BytecodeLength = _g_vertex_shader.size;
	desc_pipeline_state.PS.pShaderBytecode = _g_pixel_shader.binaryPtr;
	desc_pipeline_state.PS.BytecodeLength = _g_pixel_shader.size;
	desc_pipeline_state.SampleDesc.Count = 1;
	desc_pipeline_state.SampleMask = UINT_MAX;
	desc_pipeline_state.InputLayout.pInputElementDescs = desc_input_elements;
	desc_pipeline_state.InputLayout.NumElements = _countof(desc_input_elements);
	desc_pipeline_state.pRootSignature = _root_signature.Get();
	desc_pipeline_state.NumRenderTargets = 1;
	desc_pipeline_state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc_pipeline_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc_pipeline_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc_pipeline_state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc_pipeline_state.RasterizerState.DepthClipEnable = TRUE;
	desc_pipeline_state.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	for (int i = 0; i < _countof(desc_state.BlendState.RenderTarget); ++i) {
		desc_pipeline_state.BlendState.RenderTarget[i].BlendEnable = FALSE;
		desc_pipeline_state.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		desc_pipeline_state.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
		desc_pipeline_state.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		desc_pipeline_state.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		desc_pipeline_state.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc_pipeline_state.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc_pipeline_state.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	desc_pipeline_state.DepthStencilState.DepthEnable = FALSE;

	hr = device->CreateGraphicsPipelineState(&desc_pipeline_state, IID_PPV_ARGS(_pipeline_state.GetAddressOf()));
	if (FAILED(hr)) {
		return FALSE;
	}

	// ���_�f�[�^�̍쐬.
	D3D12_HEAP_PROPERTIES heap_props;
	D3D12_RESOURCE_DESC   desc_resource;
	ZeroMemory(&heap_props, sizeof(heap_props));
	ZeroMemory(&desc_resource, sizeof(desc_resource));
	heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_props.CreationNodeMask = 0;
	heap_props.VisibleNodeMask = 0;
	desc_resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc_resource.Width = sizeof(vertices_array);
	desc_resource.Height = 1;
	desc_resource.DepthOrArraySize = 1;
	desc_resource.MipLevels = 1;
	desc_resource.Format = DXGI_FORMAT_UNKNOWN;
	desc_resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc_resource.SampleDesc.Count = 1;

	hr = device->CreateCommittedResource(
		&heap_props,
		D3D12_HEAP_FLAG_NONE,
		&desc_resource,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vertex_buffer.GetAddressOf())
	);
	if (FAILED(hr)) {
		return FALSE;
	}
	if (!_vertex_buffer) {
		return FALSE;
	}
	// ���_�f�[�^�̏�������
	void* mapped = nullptr;
	hr = _vertex_buffer->Map(0, nullptr, &mapped);
	if (SUCCEEDED(hr)) {
		memcpy(mapped, vertices_array, sizeof(vertices_array));
		_vertex_buffer->Unmap(0, nullptr);
	}
	if (FAILED(hr)) {
		return FALSE;
	}
	_buffer_position.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
	_buffer_position.StrideInBytes = sizeof(MyVertex);
	_buffer_position.SizeInBytes = sizeof(vertices_array);

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