#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <comdef.h>
#include "stddef.h"
#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

#define MAKE_SMART_COM_PTR(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
MAKE_SMART_COM_PTR(ID3D12Device5);
MAKE_SMART_COM_PTR(ID3D12GraphicsCommandList4);
MAKE_SMART_COM_PTR(ID3D12CommandQueue);
MAKE_SMART_COM_PTR(IDXGISwapChain3);
MAKE_SMART_COM_PTR(IDXGIFactory4);
MAKE_SMART_COM_PTR(IDXGIAdapter1);
MAKE_SMART_COM_PTR(ID3D12Fence);
MAKE_SMART_COM_PTR(ID3D12CommandAllocator);
MAKE_SMART_COM_PTR(ID3D12Resource1);
MAKE_SMART_COM_PTR(ID3D12DescriptorHeap);
MAKE_SMART_COM_PTR(ID3D12Debug);
MAKE_SMART_COM_PTR(ID3D12StateObject);
MAKE_SMART_COM_PTR(ID3D12RootSignature);
MAKE_SMART_COM_PTR(ID3DBlob);

struct Rotate {
	Rotate(float a = 0.0, float b = 0.0f, float c = 0.0f) :x(a), y(b), z(c) {}
	float x;
	float y;
	float z;

	Rotate operator=(Rotate rotate) {
		this->x = rotate.x;
		this->y = rotate.y;
		this->z = rotate.z;
		return *this;
	}
};

struct Position {
	Position(float a = 0.0, float b = 0.0f, float c = 0.0f) :x(a), y(b), z(c) {}
	float x;
	float y;
	float z;

	Position operator=(Position rotate) {
		this->x = rotate.x;
		this->y = rotate.y;
		this->z = rotate.z;
		return *this;
	}
};

static const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
	D3D12_HEAP_TYPE_UPLOAD,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0,
	0,
};
static const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
	D3D12_HEAP_TYPE_DEFAULT,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0,
	0
};

struct AccelerationStructureBuffers
{
	ID3D12Resource1Ptr pScratch;
	ID3D12Resource1Ptr pResult;
	ID3D12Resource1Ptr pInstanceDesc;    // Used only for top-level AS
};

class Square
{
	struct Vertex {
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	struct ShaderParameters
	{
		XMFLOAT4X4 mtxWorld;
		XMFLOAT4X4 mtxView;
		XMFLOAT4X4 mtxProj;
	};
	enum
	{
		TextureSrvDescriptorBase = 0,
		ConstantBufferDescriptorBase = 1,
		// �T���v���[�͕ʃq�[�v�Ȃ̂Ő擪���g�p
		SamplerDescriptorBase = 0,
	};

public:
	Square() {}
	~Square() {}
	void Initialize();
	void update();
	void draw();

	void CreateAccelerationStructure();
	void SetAccelerationStructures();


	void SetPositionX(float pos){ mWorldMtrix = XMMatrixTranslation(pos, 0.0, 0.0); }
	void SetPositionY(float pos){}
	void SetPositionZ(float pos){}
	void SetRotateY(float rad);
	void SetRotateX(float rad);
	void SetRotateZ(float rad);

	ID3D12Resource1Ptr GetVertexBuffer() { return mVertexBuffer; }
private:
	ID3D12Resource1Ptr  mVertexBuffer;
	ID3D12Resource1Ptr mIndexBuffer;
	ID3D12Resource1Ptr mConstantBuffer;
	ID3D12DescriptorHeapPtr m_heapSrvCbv;
	D3D12_GPU_DESCRIPTOR_HANDLE m_cbViews;
	UINT  m_srvcbvDescriptorSize;
	UINT  mIndexCount;
	UINT  mVertexCount;

	XMVECTORF32 mPos;
	Rotate mRotate;
	XMMATRIX mWorldMtrix;
	XMMATRIX mViewMatrix;
	XMMATRIX mProjMatrix;


	ID3D12Resource1Ptr CreateBuffer(UINT bufferSize, const void* initialData);

	void SetConstantBuffer();
	D3D12_VERTEX_BUFFER_VIEW CreateVertexBufferView();
	D3D12_INDEX_BUFFER_VIEW CreateIndexBufferView();

	AccelerationStructureBuffers createBottomLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12Resource1Ptr pVB);
	AccelerationStructureBuffers createTopLevelAS(ID3D12Device5Ptr pDevice, ID3D12GraphicsCommandList4Ptr pCmdList, ID3D12Resource1Ptr pBottomLevelAS, uint64_t& tlasSize);
	
	AccelerationStructureBuffers mTopLevelBuffers;
	AccelerationStructureBuffers mBottomLevelBuffers;

	uint64_t mTlasSize = 0;
	ID3D12Resource1Ptr mTopLevelAS;
	ID3D12Resource1Ptr mBottomLevelAS;
};

