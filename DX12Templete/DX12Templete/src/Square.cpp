#include "Square.h"
#include "DX12Renderer.h"

void Square::Initialize()
{
	ComPtr<ID3D12Device5> device = DX12Renderer::GetDevice();
	mRadian = 0.0f;

	HRESULT hr;
	UINT count = 3;
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
	  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	  count,
	  D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	  0
	};
	hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_heapSrvCbv));
	if (FAILED(hr)) {
		return;
	}
	m_srvcbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


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

	_vertex_buffer = CreateBuffer(sizeof(vertices_array), vertices_array);
	_index_buffer = CreateBuffer(sizeof(indices), indices);
	_index_count = _countof(indices);
	_vertex_count = _countof(vertices_array);

	UINT bufferSize = sizeof(ShaderParameters) + 255 & ~255;
	_constant_buffer = CreateBuffer(bufferSize, nullptr);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc{};
	cbDesc.BufferLocation = _constant_buffer->GetGPUVirtualAddress();
	cbDesc.SizeInBytes = bufferSize;
	CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(m_heapSrvCbv->GetCPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase, m_srvcbvDescriptorSize);
	device->CreateConstantBufferView(&cbDesc, handleCBV);

	m_cbViews = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSrvCbv->GetGPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase, m_srvcbvDescriptorSize);

}

void Square::draw()
{
	ComPtr<ID3D12GraphicsCommandList> _command_list = DX12Renderer::GetCmdList();

	mRadian += 1.0f;
	ShaderParameters shaderParams;
	XMStoreFloat4x4(&shaderParams.mtxWorld, XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(mRadian)));
	XMMATRIX mtxView = XMMatrixLookAtLH(
		XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), // Eye Position
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),  // Eye Direction
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)   // Eye Up
	);
	XMMATRIX mtxProj = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(45.0f),
		720 / 480,
		0.1f,
		100.0f
	);

	XMStoreFloat4x4(&shaderParams.mtxView, XMMatrixTranspose(mtxView));
	XMStoreFloat4x4(&shaderParams.mtxProj, XMMatrixTranspose(mtxProj));

	auto& constantBuffer = _constant_buffer;
	{
		void* p;
		CD3DX12_RANGE range(0, 0);
		constantBuffer->Map(0, &range, &p);
		memcpy(p, &shaderParams, sizeof(shaderParams));
		constantBuffer->Unmap(0, nullptr);
	}

	D3D12_VERTEX_BUFFER_VIEW    _vertex_buffer_view{};
	_vertex_buffer_view.BufferLocation = _vertex_buffer->GetGPUVirtualAddress();
	_vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	_vertex_buffer_view.SizeInBytes = sizeof(Vertex) * _vertex_count;

	D3D12_INDEX_BUFFER_VIEW   _index_buffer_view{};
	_index_buffer_view.BufferLocation = _index_buffer->GetGPUVirtualAddress();
	_index_buffer_view.SizeInBytes = sizeof(uint32_t) * _index_count;
	_index_buffer_view.Format = DXGI_FORMAT_R32_UINT;

	ID3D12DescriptorHeap* heaps[] = { m_heapSrvCbv.Get() };
	_command_list->SetDescriptorHeaps(_countof(heaps), heaps);
	_command_list->SetGraphicsRootDescriptorTable(0, m_cbViews);

	_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_command_list->IASetVertexBuffers(0, 1, &_vertex_buffer_view);
	_command_list->IASetIndexBuffer(&_index_buffer_view);

	_command_list->DrawIndexedInstanced(_index_count, 1, 0, 0, 0);
}

ComPtr<ID3D12Resource1> Square::CreateBuffer(UINT bufferSize, const void* initialData)
{
	HRESULT hr;
	ComPtr<ID3D12Resource1> buffer;
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