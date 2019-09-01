#pragma once
#include "BasicRenderer.h"

class DXRenderer : public BasicRenderer
{
public:
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT4 color;
	};

	DXRenderer(HWND hwnd, int Width, int Height);
	~DXRenderer();
private:

	// Assets
	virtual void CompileShader();
	virtual void CreatePipelineStateObject();
	virtual void LoadVertexBuffer();

	// Render
	virtual void SetCommandList();
	virtual void RecordCommand();

	D3D12_VERTEX_BUFFER_VIEW CreateVertexBufferView(UINT bufferSize);

	ComPtr<ID3DBlob> mVertexShader;
	ComPtr<ID3DBlob> mPixelShader;

};

