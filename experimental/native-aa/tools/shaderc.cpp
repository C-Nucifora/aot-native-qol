#include <windows.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <cstdio>
#include <vector>
int main(int argc, char** argv) {
    if (argc != 5) { std::fprintf(stderr, "usage: shaderc source entry profile output\n"); return 2; }
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[1], -1, nullptr, 0);
    if (!count) return 2;
    std::vector<WCHAR> source(count);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[1], -1, source.data(), count);
    ID3DBlob *code = nullptr, *errors = nullptr;
    HRESULT hr = D3DCompileFromFile(source.data(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        argv[2], argv[3], D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (errors) { std::fwrite(errors->GetBufferPointer(), 1, errors->GetBufferSize(), stderr); errors->Release(); }
    if (FAILED(hr)) { std::fprintf(stderr, "D3DCompile failed %08x\n", unsigned(hr)); return 1; }
    ID3D11ShaderReflection* reflection = nullptr;
    if (FAILED(D3DReflect(code->GetBufferPointer(), code->GetBufferSize(), IID_ID3D11ShaderReflection,
                         reinterpret_cast<void**>(&reflection)))) { code->Release(); return 1; }
    D3D11_SHADER_DESC desc{};
    reflection->GetDesc(&desc);
    for (UINT i = 0; i < desc.BoundResources; ++i) {
        D3D11_SHADER_INPUT_BIND_DESC resource{};
        reflection->GetResourceBindingDesc(i, &resource);
        std::printf("binding %s type=%u slot=%u count=%u\n", resource.Name, unsigned(resource.Type), resource.BindPoint, resource.BindCount);
    }
    reflection->Release();
    HANDLE output = CreateFileA(argv[4], GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) { code->Release(); return 1; }
    DWORD written = 0;
    const DWORD size = static_cast<DWORD>(code->GetBufferSize());
    bool ok = WriteFile(output, code->GetBufferPointer(), size, &written, nullptr) && written == size;
    ok = CloseHandle(output) && ok;
    code->Release();
    return ok ? 0 : 1;
}
