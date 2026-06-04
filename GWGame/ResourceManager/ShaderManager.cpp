#include "ShaderManager.h"

#pragma comment(lib, "d3dcompiler.lib")

namespace ResourceManager
{
    void ShaderManager::Initialize(ID3D11Device* device)
    {
        m_device = device;
    }

    void ShaderManager::Finalize()
    {
        m_cache.clear();
    }

    const ShaderSet* ShaderManager::Load(const std::string& name, const wchar_t* vsFile, const wchar_t* psFile,
                                         const D3D11_INPUT_ELEMENT_DESC* layout, UINT layoutCount)
    {
        // キャッシュ済みなら即返す（二重ロード防止）
        auto it = m_cache.find(name);
        if (it != m_cache.end())
            return &it->second;

        ShaderSet set;
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;

        if (!CompileShader(vsFile, "main", "vs_5_0", &vsBlob))
            return nullptr;
        if (!CompileShader(psFile, "main", "ps_5_0", &psBlob))
        {
            if (vsBlob) vsBlob->Release();
            return nullptr;
        }

        m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr,
                                     set.vs.ReleaseAndGetAddressOf());

        m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
                                    set.ps.ReleaseAndGetAddressOf());

        m_device->CreateInputLayout(layout, layoutCount, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                    set.inputLayout.ReleaseAndGetAddressOf());

        if (vsBlob) vsBlob->Release();
        if (psBlob) psBlob->Release();

        m_cache[name] = std::move(set);
        return &m_cache[name];
    }

    const ShaderSet* ShaderManager::Get(const std::string& name) const
    {
        auto it = m_cache.find(name);
        return (it != m_cache.end()) ? &it->second : nullptr;
    }

    bool ShaderManager::CompileShader(const wchar_t* file, const char* entry, const char* target, ID3DBlob** blobOut)
    {
        ID3DBlob* errBlob = nullptr;
        HRESULT hr = D3DCompileFromFile(file, nullptr, nullptr, entry, target,
                                        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, blobOut, &errBlob);

        if (FAILED(hr))
        {
            if (errBlob)
            {
                OutputDebugStringA((char*)errBlob->GetBufferPointer());
                errBlob->Release();
            }
            return false;
        }
        return true;
    }

   


} // namespace ResourceManager