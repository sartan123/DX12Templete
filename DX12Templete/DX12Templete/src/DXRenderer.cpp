#include "DXRenderer.h"
#include "utility.h"

DXRenderer::DXRenderer(HWND hwnd, int Width, int Height)
 :BasicRenderer(hwnd, Width, Height)
{

}
DXRenderer::~DXRenderer()
{

}

void DXRenderer::CompileShader()
{
#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif
	WCHAR tmp_path[512];
	GetAssetsPath(tmp_path, _countof(tmp_path));
	std::wstring assetsPath = tmp_path;
	LPCWSTR path = GetAssetFullPath(assetsPath, L"shaders.hlsl").c_str();

	ThrowIfFailed(D3DCompileFromFile(path, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &mVertexShader, nullptr));
	ThrowIfFailed(D3DCompileFromFile(path, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &mPixelShader, nullptr));
}

void DXRenderer::CreatePipelineStateObject()
{
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS = { reinterpret_cast<UINT8*>(mVertexShader->GetBufferPointer()), mVertexShader->GetBufferSize() };
	psoDesc.PS = { reinterpret_cast<UINT8*>(mPixelShader->GetBufferPointer()), mPixelShader->GetBufferSize() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState)));
}

void DXRenderer::LoadVertexBuffer()
{
	Vertex triangleVertices[] =
	{
		{ { 0.0f, 0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ { 0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
	};
	const UINT vertexBufferSize = sizeof(triangleVertices);

	mVertexBuffer = CreateBuffer(triangleVertices, vertexBufferSize);
	mVertexBufferView = CreateVertexBufferView(vertexBufferSize);
}

D3D12_VERTEX_BUFFER_VIEW DXRenderer::CreateVertexBufferView(UINT bufferSize)
{
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
	vertex_buffer_view.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	vertex_buffer_view.SizeInBytes = bufferSize;

	return vertex_buffer_view;
}

void DXRenderer::SetCommandList()
{
	SetGraphicsRootSignature();

	SetViewPort();

	SetScissoringRect();

	RecordCommand();
}

void DXRenderer::RecordCommand()
{
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTarget[mCurrentFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mCurrentFrameIndex, mRTVDescriptorSize);
	mCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	mCommandList->DrawInstanced(3, 1, 0, 0);
}
