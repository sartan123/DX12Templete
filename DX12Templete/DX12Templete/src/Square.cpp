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

// Acceleration Structure
ID3D12ResourcePtr createBuffer(ID3D12Device5Ptr pDevice, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
{
	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Alignment = 0;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Flags = flags;
	bufDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufDesc.Height = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.SampleDesc.Quality = 0;
	bufDesc.Width = size;

	ID3D12ResourcePtr pBuffer;
	pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer));
	return pBuffer;
}

AccelerationStructureBuffers Square::createBottomLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12Resource1Ptr pVB)
{
	D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
	geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geomDesc.Triangles.VertexBuffer.StartAddress = pVB->GetGPUVirtualAddress();
	geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(XMFLOAT3);
	geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geomDesc.Triangles.VertexCount = 3;
	geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get the size requirements for the scratch and AS buffers
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	inputs.NumDescs = 1;
	inputs.pGeometryDescs = &geomDesc;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	// Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
	AccelerationStructureBuffers buffers;
	buffers.pScratch = createBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
	buffers.pResult = createBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);

	// Create the bottom-level AS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

	pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = buffers.pResult;
	pCmdList->ResourceBarrier(1, &uavBarrier);

	return buffers;
}

AccelerationStructureBuffers Square::createTopLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12Resource1Ptr pBottomLevelAS, uint64_t& tlasSize)
{
	// First, get the size of the TLAS buffers and create them
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	inputs.NumDescs = 1;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	// Create the buffers
	AccelerationStructureBuffers buffers;
	buffers.pScratch = createBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
	buffers.pResult = createBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
	tlasSize = info.ResultDataMaxSizeInBytes;

	// The instance desc should be inside a buffer, create and map the buffer
	buffers.pInstanceDesc = createBuffer(pDevice, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
	buffers.pInstanceDesc->Map(0, nullptr, (void**)& pInstanceDesc);

	// Initialize the instance desc. We only have a single instance
	pInstanceDesc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
	pInstanceDesc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
	pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	XMMATRIX m; // Identity matrix
	memcpy(pInstanceDesc->Transform, &m, sizeof(pInstanceDesc->Transform));
	pInstanceDesc->AccelerationStructure = pBottomLevelAS->GetGPUVirtualAddress();
	pInstanceDesc->InstanceMask = 0xFF;

	// Unmap
	buffers.pInstanceDesc->Unmap(0, nullptr);

	// Create the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.Inputs.InstanceDescs = buffers.pInstanceDesc->GetGPUVirtualAddress();
	asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

	pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = buffers.pResult;
	pCmdList->ResourceBarrier(1, &uavBarrier);

	return buffers;
}

void Square::CreateAccelerationStructure()
{
	auto device = DX12Renderer::GetDevice();
	auto cmd_list = DX12Renderer::GetCmdList();

	mBottomLevelBuffers = createBottomLevelAS(device, cmd_list, mVertexBuffer);
	mTopLevelBuffers = createTopLevelAS(device, cmd_list, mBottomLevelBuffers.pResult, mTlasSize);

}

void Square::SetAccelerationStructures()
{
	mTopLevelAS = mTopLevelBuffers.pResult;
	mBottomLevelAS = mBottomLevelBuffers.pResult;
}
