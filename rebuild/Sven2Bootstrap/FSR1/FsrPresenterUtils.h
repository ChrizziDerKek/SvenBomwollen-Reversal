#pragma once
#include "Types.h"

void Check(HRESULT result, cstr operation)
{
	if (FAILED(result))
		throw std::runtime_error(str(operation) + " failed (HRESULT " + std::to_string(static_cast<u32>(result)) + ").");
}

fpath ShaderPath()
{
	wchar_t file[32768]{};
	GetModuleFileNameW(nullptr, file, 32768);
	return fpath(file).parent_path() / L"shaders" / L"Fsr1.hlsl";
}

ComPtr<ID3DBlob> Compile(cstr entry, cstr target, cstr pass = nullptr)
{
	D3D_SHADER_MACRO macros[] = {{pass, "1"}, {nullptr, nullptr}};
	ComPtr<ID3DBlob> code, errors;
	const HRESULT result = D3DCompileFromFile(ShaderPath().c_str(), pass ? macros : nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
	if (FAILED(result))
		throw std::runtime_error("FSR1 shader compilation: " + (errors ? str(static_cast<cstr>(errors->GetBufferPointer()), errors->GetBufferSize()) : "Cannot read shaders/Fsr1.hlsl or its AMD headers."));
	return code;
}