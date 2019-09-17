#include "Square.h"
#include "DX12Renderer.h"

void Square::Initialize()
{
	ID3D12Device5Ptr mDevice = DX12Renderer::GetDevice();

	mRotate = Rotate();
	mPos = XMVECTORF32();

	HRESULT hr;
	UINT count = 3;
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
	  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	  count,
	  D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	  0
	};
	hr = mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_heapSrvCbv));
	if (FAILED(hr)) {
		return;
	}
	m_srvcbvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	//  ’¸“_î•ñ
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
	mIndexBuffer = CreateBuffer(sizeof(indices), indices);
	mIndexCount = _countof(indices);
	mVertexCount = _countof(vertices_array);

	UINT bufferSize = sizeof(ShaderParameters) + 255 & ~255;
	mConstantBuffer = CreateBuffer(bufferSize, nullptr);

	mWorldMtrix = XMMatrixIdentity();

	mViewMatrix = XMMatrixLookAtLH(
		XMVectorSet(0.0f, 0.0f, -2.0f, 0.0f), // Eye Position
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),  // Eye Direction
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)   // Eye Up
	);

	mProjMatrix = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(45.0f),
		720 / 480,
		0.1f,
		100.0f
	);

	CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(m_heapSrvCbv->GetCPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase, m_srvcbvDescriptorSize);
	m_cbViews = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSrvCbv->GetGPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase, m_srvcbvDescriptorSize);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc{};
	cbDesc.BufferLocation = mConstantBuffer->GetGPUVirtualAddress();
	cbDesc.SizeInBytes = bufferSize;
	mDevice->CreateConstantBufferView(&cbDesc, handleCBV);

}

void Square::update()
{
	SetRotateX(1.0);
}

void Square::draw()
{
	ID3D12GraphicsCommandList4Ptr mCmdList = DX12Renderer::GetCmdList();

	SetConstantBuffer();

	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = CreateVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW index_buffer_view = CreateIndexBufferView();

	ID3D12DescriptorHeap* heaps[] = { m_heapSrvCbv };
	mCmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	mCmdList->SetGraphicsRootDescriptorTable(0, m_cbViews);

	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCmdList->IASetVertexBuffers(0, 1, &vertex_buffer_view);
	mCmdList->IASetIndexBuffer(&index_buffer_view);
	mCmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
}


ID3D12Resource1Ptr Square::CreateBuffer(UINT bufferSize, const void* initialData)
{
	HRESULT hr;
	ID3D12Resource1Ptr buffer;
	hr = DX12Renderer::GetDevice()->CreateCommittedResource(
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

void Square::SetConstantBuffer()
{
	ShaderParameters shaderParams;
	XMStoreFloat4x4(&shaderParams.mtxWorld, XMMatrixTranspose(mWorldMtrix));
	XMStoreFloat4x4(&shaderParams.mtxView, XMMatrixTranspose(mViewMatrix));
	XMStoreFloat4x4(&shaderParams.mtxProj, XMMatrixTranspose(mProjMatrix));

	auto& constantBuffer = mConstantBuffer;
	{
		void* p;
		CD3DX12_RANGE range(0, 0);
		constantBuffer->Map(0, &range, &p);
		memcpy(p, &shaderParams, sizeof(shaderParams));
		constantBuffer->Unmap(0, nullptr);
	}
}

D3D12_VERTEX_BUFFER_VIEW Square::CreateVertexBufferView()
{
	D3D12_VERTEX_BUFFER_VIEW    vertex_buffer_view{};
	vertex_buffer_view.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	vertex_buffer_view.SizeInBytes = sizeof(Vertex) * mVertexCount;
	return vertex_buffer_view;
}

D3D12_INDEX_BUFFER_VIEW Square::CreateIndexBufferView()
{
	D3D12_INDEX_BUFFER_VIEW   index_buffer_view{};
	index_buffer_view.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
	index_buffer_view.SizeInBytes = sizeof(uint32_t) * mIndexCount;
	index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
	return index_buffer_view;
}

void Square::SetRotateY(float rad)
{
	mRotate.y += rad;

	mWorldMtrix = XMMatrixRotationAxis(
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(mRotate.y));
}

void Square::SetRotateX(float rad)
{
	mRotate.x += rad;

	mWorldMtrix = XMMatrixRotationAxis(
		XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMConvertToRadians(mRotate.x));
}

void Square::SetRotateZ(float rad)
{
	mRotate.z += rad;

	mWorldMtrix = XMMatrixRotationAxis(
		XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMConvertToRadians(mRotate.z));
}
