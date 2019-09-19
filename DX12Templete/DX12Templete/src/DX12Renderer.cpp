#define _CRT_SECURE_NO_WARNINGS
#include "DX12Renderer.h"
#include "utility.h"
#include "dxcapi.use.h"

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

	CreateRayTracingPipelineStateObject();

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
	mSquareList[1]->SetPositionX(-0.525f);
	mSquareList[2]->SetPositionX(0.525f);

	InitializeAccelarationStructure();
	
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

void DX12Renderer::InitializeAccelarationStructure()
{
	for (int i = 0; i < mSquareList.size(); i++)
	{
		mSquareList[i]->CreateAccelerationStructure();
	}

	mCmdList->Close();

	ID3D12CommandList* pCommandList = mCmdList.GetInterfacePtr();
	mCmdQueue->ExecuteCommandLists(1, &pCommandList);

	WaitForCommandQueue();

	mCmdAllocator->Reset();
	mCmdList->Reset(mCmdAllocator, mPipelineState);

	mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();

	for (int i = 0; i < mSquareList.size(); i++)
	{
		mSquareList[i]->SetAccelerationStructures();
	}
}

static dxc::DxcDllSupport gDxcDllHelper;
// Ray Tracing
ID3DBlobPtr compileLibrary(const WCHAR* filename, const WCHAR* targetString)
{
	// Initialize the helper
	gDxcDllHelper.Initialize();
	IDxcCompilerPtr pCompiler;
	IDxcLibraryPtr pLibrary;
	gDxcDllHelper.CreateInstance(CLSID_DxcCompiler, &pCompiler);
	gDxcDllHelper.CreateInstance(CLSID_DxcLibrary, &pLibrary);

	// Open and read the file
	std::ifstream shaderFile(filename);
	if (shaderFile.good() == false)
	{
		return nullptr;
	}
	std::stringstream strStream;
	strStream << shaderFile.rdbuf();
	std::string shader = strStream.str();

	// Create blob from the string
	IDxcBlobEncodingPtr pTextBlob;
	pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)shader.c_str(), (uint32_t)shader.size(), 0, &pTextBlob);

	// Compile
	IDxcOperationResultPtr pResult;
	pCompiler->Compile(pTextBlob, filename, L"", targetString, nullptr, 0, nullptr, 0, nullptr, &pResult);

	// Verify the result
	HRESULT resultCode;
	pResult->GetStatus(&resultCode);
	if (FAILED(resultCode))
	{
		IDxcBlobEncodingPtr pError;
		pResult->GetErrorBuffer(&pError);
		return nullptr;
	}

	MAKE_SMART_COM_PTR(IDxcBlob);
	IDxcBlobPtr pBlob;
	pResult->GetResult(&pBlob);
	return pBlob;
}

ID3D12RootSignaturePtr createRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
{
	ID3DBlobPtr pSigBlob;
	ID3DBlobPtr pErrorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		return nullptr;
	}
	ID3D12RootSignaturePtr pRootSig;
	pDevice->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
	return pRootSig;
}

struct RootSignatureDesc
{
	D3D12_ROOT_SIGNATURE_DESC desc = {};
	std::vector<D3D12_DESCRIPTOR_RANGE> range;
	std::vector<D3D12_ROOT_PARAMETER> rootParams;
};

RootSignatureDesc createRayGenRootDesc()
{
	// Create the root-signature
	RootSignatureDesc desc;
	desc.range.resize(2);
	// gOutput
	desc.range[0].BaseShaderRegister = 0;
	desc.range[0].NumDescriptors = 1;
	desc.range[0].RegisterSpace = 0;
	desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	desc.range[0].OffsetInDescriptorsFromTableStart = 0;

	// gRtScene
	desc.range[1].BaseShaderRegister = 0;
	desc.range[1].NumDescriptors = 1;
	desc.range[1].RegisterSpace = 0;
	desc.range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	desc.range[1].OffsetInDescriptorsFromTableStart = 1;

	desc.rootParams.resize(1);
	desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	desc.rootParams[0].DescriptorTable.NumDescriptorRanges = 2;
	desc.rootParams[0].DescriptorTable.pDescriptorRanges = desc.range.data();

	// Create the desc
	desc.desc.NumParameters = 1;
	desc.desc.pParameters = desc.rootParams.data();
	desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	return desc;
}

struct DxilLibrary
{
	DxilLibrary(ID3DBlobPtr pBlob, const WCHAR* entryPoint[], uint32_t entryPointCount) : pShaderBlob(pBlob)
	{
		stateSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		stateSubobject.pDesc = &dxilLibDesc;

		dxilLibDesc = {};
		exportDesc.resize(entryPointCount);
		exportName.resize(entryPointCount);
		if (pBlob)
		{
			dxilLibDesc.DXILLibrary.pShaderBytecode = pBlob->GetBufferPointer();
			dxilLibDesc.DXILLibrary.BytecodeLength = pBlob->GetBufferSize();
			dxilLibDesc.NumExports = entryPointCount;
			dxilLibDesc.pExports = exportDesc.data();

			for (uint32_t i = 0; i < entryPointCount; i++)
			{
				exportName[i] = entryPoint[i];
				exportDesc[i].Name = exportName[i].c_str();
				exportDesc[i].Flags = D3D12_EXPORT_FLAG_NONE;
				exportDesc[i].ExportToRename = nullptr;
			}
		}
	};

	DxilLibrary() : DxilLibrary(nullptr, nullptr, 0) {}

	D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
	D3D12_STATE_SUBOBJECT stateSubobject{};
	ID3DBlobPtr pShaderBlob;
	std::vector<D3D12_EXPORT_DESC> exportDesc;
	std::vector<std::wstring> exportName;
};

static const WCHAR* kRayGenShader = L"rayGen";
static const WCHAR* kMissShader = L"miss";
static const WCHAR* kClosestHitShader = L"chs";
static const WCHAR* kHitGroup = L"HitGroup";

DxilLibrary createDxilLibrary()
{
	// Compile the shader
	ID3DBlobPtr pDxilLib = compileLibrary(L"Data/04-Shaders.hlsl", L"lib_6_3");
	const WCHAR* entryPoints[] = { kRayGenShader, kMissShader, kClosestHitShader };
	return DxilLibrary(pDxilLib, entryPoints, arraysize(entryPoints));
}

struct HitProgram
{
	HitProgram(LPCWSTR ahsExport, LPCWSTR chsExport, const std::wstring& name) : exportName(name)
	{
		desc = {};
		desc.AnyHitShaderImport = ahsExport;
		desc.ClosestHitShaderImport = chsExport;
		desc.HitGroupExport = exportName.c_str();

		subObject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		subObject.pDesc = &desc;
	}

	std::wstring exportName;
	D3D12_HIT_GROUP_DESC desc;
	D3D12_STATE_SUBOBJECT subObject;
};

struct ExportAssociation
{
	ExportAssociation(const WCHAR* exportNames[], uint32_t exportCount, const D3D12_STATE_SUBOBJECT* pSubobjectToAssociate)
	{
		association.NumExports = exportCount;
		association.pExports = exportNames;
		association.pSubobjectToAssociate = pSubobjectToAssociate;

		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		subobject.pDesc = &association;
	}

	D3D12_STATE_SUBOBJECT subobject = {};
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association = {};
};

struct LocalRootSignature
{
	LocalRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
	{
		pRootSig = createRootSignature(pDevice, desc);
		pInterface = pRootSig.GetInterfacePtr();
		subobject.pDesc = &pInterface;
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	}
	ID3D12RootSignaturePtr pRootSig;
	ID3D12RootSignature* pInterface = nullptr;
	D3D12_STATE_SUBOBJECT subobject = {};
};

struct GlobalRootSignature
{
	GlobalRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
	{
		pRootSig = createRootSignature(pDevice, desc);
		pInterface = pRootSig.GetInterfacePtr();
		subobject.pDesc = &pInterface;
		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	}
	ID3D12RootSignaturePtr pRootSig;
	ID3D12RootSignature* pInterface = nullptr;
	D3D12_STATE_SUBOBJECT subobject = {};
};

struct ShaderConfig
{
	ShaderConfig(uint32_t maxAttributeSizeInBytes, uint32_t maxPayloadSizeInBytes)
	{
		shaderConfig.MaxAttributeSizeInBytes = maxAttributeSizeInBytes;
		shaderConfig.MaxPayloadSizeInBytes = maxPayloadSizeInBytes;

		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		subobject.pDesc = &shaderConfig;
	}

	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
	D3D12_STATE_SUBOBJECT subobject = {};
};

struct PipelineConfig
{
	PipelineConfig(uint32_t maxTraceRecursionDepth)
	{
		config.MaxTraceRecursionDepth = maxTraceRecursionDepth;

		subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		subobject.pDesc = &config;
	}

	D3D12_RAYTRACING_PIPELINE_CONFIG config = {};
	D3D12_STATE_SUBOBJECT subobject = {};
};

void DX12Renderer::CreateRayTracingPipelineStateObject()
{
	std::array<D3D12_STATE_SUBOBJECT, 10> subobjects;
	uint32_t index = 0;

	// Create the DXIL library
	DxilLibrary dxilLib = createDxilLibrary();
	subobjects[index++] = dxilLib.stateSubobject; // 0 Library

	HitProgram hitProgram(nullptr, kClosestHitShader, kHitGroup);
	subobjects[index++] = hitProgram.subObject; // 1 Hit Group

	// Create the ray-gen root-signature and association
	LocalRootSignature rgsRootSignature(mDevice, createRayGenRootDesc().desc);
	subobjects[index] = rgsRootSignature.subobject; // 2 RayGen Root Sig

	uint32_t rgsRootIndex = index++; // 2
	ExportAssociation rgsRootAssociation(&kRayGenShader, 1, &(subobjects[rgsRootIndex]));
	subobjects[index++] = rgsRootAssociation.subobject; // 3 Associate Root Sig to RGS

	// Create the miss- and hit-programs root-signature and association
	D3D12_ROOT_SIGNATURE_DESC emptyDesc = {};
	emptyDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	LocalRootSignature hitMissRootSignature(mDevice, emptyDesc);
	subobjects[index] = hitMissRootSignature.subobject; // 4 Root Sig to be shared between Miss and CHS

	uint32_t hitMissRootIndex = index++; // 4
	const WCHAR* missHitExportName[] = { kMissShader, kClosestHitShader };
	ExportAssociation missHitRootAssociation(missHitExportName, arraysize(missHitExportName), &(subobjects[hitMissRootIndex]));
	subobjects[index++] = missHitRootAssociation.subobject; // 5 Associate Root Sig to Miss and CHS

	// Bind the payload size to the programs
	ShaderConfig shaderConfig(sizeof(float) * 2, sizeof(float) * 1);
	subobjects[index] = shaderConfig.subobject; // 6 Shader Config

	uint32_t shaderConfigIndex = index++; // 6
	const WCHAR* shaderExports[] = { kMissShader, kClosestHitShader, kRayGenShader };
	ExportAssociation configAssociation(shaderExports, arraysize(shaderExports), &(subobjects[shaderConfigIndex]));
	subobjects[index++] = configAssociation.subobject; // 7 Associate Shader Config to Miss, CHS, RGS

	// Create the pipeline config
	PipelineConfig config(0);
	subobjects[index++] = config.subobject; // 8

	// Create the global root signature and store the empty signature
	GlobalRootSignature root(mDevice, {});
	mRayTraceRootSignature = root.pRootSig;
	subobjects[index++] = root.subobject; // 9

	// Create the state
	D3D12_STATE_OBJECT_DESC desc;
	desc.NumSubobjects = index; // 10
	desc.pSubobjects = subobjects.data();
	desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	mDevice->CreateStateObject(&desc, IID_PPV_ARGS(&mRayTracePipelineState));
}
