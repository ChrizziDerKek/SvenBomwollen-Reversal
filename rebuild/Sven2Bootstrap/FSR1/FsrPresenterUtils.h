#pragma once
#include "Types.h"
#include "FsrResources.h"

void Check(HRESULT result, cstr operation)
{
	if (FAILED(result))
		throw std::runtime_error(str(operation) + " failed (HRESULT " + std::to_string(static_cast<u32>(result)) + ").");
}

struct ShaderResource
{
	LPCVOID Data;
	u32 Size;
};

ShaderResource LoadShaderResource(i32 id)
{
	const HMODULE module = GetModuleHandleW(nullptr);
	const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
	if (!resource)
		throw std::runtime_error("Embedded FSR1 shader resource is missing.");
	const HGLOBAL loaded = LoadResource(module, resource);
	const LPCVOID data = loaded ? LockResource(loaded) : nullptr;
	if (!data)
		throw std::runtime_error("Cannot load embedded FSR1 shader resource.");
	return { data, SizeofResource(module, resource) };
}

class ShaderIncludes final : public ID3DInclude
{
public:
	HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR filename, LPCVOID, LPCVOID* data, UINT* size) override
	{
		try
		{
			const str name = filename;
			const i32 id = name == "ffx_a.h" ? FsrTypesResource : (name == "ffx_fsr1.h" ? FsrAlgorithmResource : 0);
			if (!id)
				return E_FAIL;
			const ShaderResource resource = LoadShaderResource(id);
			*data = resource.Data;
			*size = resource.Size;
			return S_OK;
		}
		catch (...)
		{
			return E_FAIL;
		}
	}

	HRESULT STDMETHODCALLTYPE Close(LPCVOID) override
	{
		return S_OK;
	}
};

ComPtr<ID3DBlob> Compile(cstr entry, cstr target, cstr pass = nullptr)
{
	const ShaderResource source = LoadShaderResource(FsrProgramResource);
	ShaderIncludes includes;
	D3D_SHADER_MACRO macros[] = { { pass, "1" }, { nullptr, nullptr } };
	ComPtr<ID3DBlob> code, errors;
	const HRESULT result = D3DCompile(source.Data, source.Size, "embedded/Fsr1.hlsl", pass ? macros : nullptr, &includes, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
	if (FAILED(result))
		throw std::runtime_error("FSR1 shader compilation: " + (errors ? str(static_cast<cstr>(errors->GetBufferPointer()), errors->GetBufferSize()) : "Cannot compile embedded shaders."));
	return code;
}
