// General helper code
#pragma once

#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>

#include "d3dx12.h"
#include "DDSTextureLoader.h"
#include "MathHelper.h"

extern const int gNumFrameResources;

inline void D3DSetDebugName(IDXGIObject* obj, const char* name)
{
	if (obj)
	{
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
	}
}

inline void D3DSetDebugName(ID3D12Device* obj, const char* name)
{
	if (obj)
	{
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
	}
}

inline void D3DSetDebugName(ID3D12DeviceChild* obj, const char* name)
{
	if (obj)
	{
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
	}
}

inline std::wstring AnsiToWString(const std::string& str)
{
	WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);

	return std::wstring(buffer);
}

/*
#if defined(_DEBUG)
	#ifndef Assert
	#define Assert(x, description)                                  \
	{                                                               \
		static bool ignoreAssert = false;                           \
		if(!ignoreAssert && !(x))                                   \
		{                                                           \
			Debug::AssertResult result = Debug::ShowAssertDialog(   \
			(L#x), description, AnsiToWString(__FILE__), __LINE__); \
		if(result == Debug::AssertIgnore)                           \
		{                                                           \
			ignoreAssert = true;                                    \
		}                                                           \
					else if(result == Debug::AssertBreak)           \
		{                                                           \
			__debugbreak();                                         \
		}                                                           \
		}                                                           \
	}
	#endif
#else
	#ifndef Assert
	#define Assert(x, description)
	#endif
#endif
	*/

class d3dUtil
{
public:

	static bool IsKeyDown(int vkeyCode);

	static std::string ToString(HRESULT hr);

	static UINT CalcConstantBufferByteSize(UINT byteSize, UINT alignment = 256)
	{
		// constant buffer must be a multiple of the minimum hardware
		// allocation size(usually 256 bytes). so round up to nearest
		// multiple of 256. we do this by adding 255 and then masking 
		// off the lower 2 bytes which store all bits < 256.
		// example: suppose byteSize = 300;
		// (300 + 255) & ~255
		// 555 & ~255
		// 0x022B & ~0x00ff
		// 0x022B & 0xff00
		// 0x0200
		// 512
		return (byteSize + alignment - 1) & ~(alignment - 1);
	}

	static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entryPoint,
		const std::string& target);
};

class DxException
{
public:
	DxException() = default;
	DxException(
		HRESULT hr,
		const std::wstring& functionName,
		const std::wstring& fileName,
		int lineNumber);

	std::wstring ToString() const;

	HRESULT ErrorCode = S_OK;

	std::wstring FuncionName;
	std::wstring FileName;
	int LineNumber = -1;
};


// defines a subrange of geometry in a MeshGeometry. This is for when multiple
// geometries are stored in one vertex and index buffer. It provides the offsets
// and data needed to draw a subset of geometry stores in the vertex and index 
// buffers
struct SubmeshGeometry
{
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT  BaseVertexLocation;

	// bounding box of the geometry defined by this submesh
	DirectX::BoundingBox BoundingBox;
};

struct MeshGeometry
{
	// give it a name 
	std::string m_Name;

	// system memory copies. Use blobs because the vertex/index format can be
	// generic. it is up to the client to cast appropriately
	Microsoft::WRL::ComPtr<ID3DBlob> m_VertexBufferCPU				= nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_IndexBufferCPU				= nullptr;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBufferGPU		= nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBufferGPU			= nullptr;
		
	Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBufferUploader	= nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBufferUploader	= nullptr;

	// Data about the buffers
	UINT m_VertexByteStride = 0;
	UINT m_VertexBufferByteSize = 0;
	UINT m_IndexBufferByteSize = 0;
	DXGI_FORMAT m_IndexFormat = DXGI_FORMAT_R16_UINT;

	// a MeshGeometry may store multiple geometries in on vertex/index buffer.
	// use this container to define the SubMesh geometries so we can draw the
	// SubMeshes individually
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

		vertexBufferView.BufferLocation = m_VertexBufferGPU->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes	  = m_VertexByteStride;
		vertexBufferView.SizeInBytes	= m_VertexBufferByteSize;

		return vertexBufferView;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW indexBufferView;

		indexBufferView.BufferLocation	= m_IndexBufferGPU->GetGPUVirtualAddress();
		indexBufferView.Format			= m_IndexFormat;
		indexBufferView.SizeInBytes		= m_IndexBufferByteSize;

		return indexBufferView;
	}

	// we can free this memory after we finish upload to the gpu
	void DisposeUploaders()
	{
		m_VertexBufferUploader	= nullptr;
		m_IndexBufferUploader	= nullptr;
	}
};

struct Light
{
	DirectX::XMFLOAT3 Strength	= { 0.5f, 0.5f, 0.5f };
	float FallOffStart			= 1.0f;						// point/spot light only
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };	// directional/spot light only
	float FallOffEnd			= 10.0f;					// point/spot light only
	DirectX::XMFLOAT3 Position	= { 0.0f, 0.0f, 0.0f };		// point/spot light only
	float SpotPower				= 64.0f;					// spot light only
};

#define MAXLIGHTS 16

struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0		= { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;

	// used in texture mapping
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

// simple struct to represent a material for our demos.
// A production 3d engine would likely create a class hierarchy of Materials
struct Material
{
	// Unique material name for lookup
	std::string Name;

	// index into constant buffer corresponding to this material
	int MaterialConstantBufferIndex = -1;

	// Index into SRV(shader resource view) heap for diffuse texture
	int DiffuseSRVHeapIndex = -1;

	// index into SRV Heap for normal texture
	int NormalSRVHeapIndex = -1;

	// dirty flag indicating the material has changed and we need to update the constant buffer.
	// because we have a material constant buffer for each FrameResource, we have to apply the
	// update to each FrameResource. Thus, when we modify a material we should set NumFramesDirty 
	// = gNumFrameResources so that each frame resource gets the update
	int NumFramesDirty = gNumFrameResources;

	// Material constant buffer data used for shading
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct Texture
{
	// unique material name for lookup
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x) { x-> Release(); x = 0; }}
#endif // !ReleaseCom
