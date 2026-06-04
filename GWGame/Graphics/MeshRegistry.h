#pragma once
#include "MeshData.h"
#include "MeshLoader.h"

#include <d3d11.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cstring>

namespace Graphics
{

    // ---- MeshRegistry -----------------------------------------------------------
    // MeshData を一元管理する。ModelRegistry の置き換え。
    // ECS の RenderComp は MeshID だけを持ち、ここから MeshData* を引く。
    //
    // ライフタイム: Game が所有・InstancedRenderSystem より長生き。
    // ----------------------------------------------------------------------------
    class MeshRegistry
    {
    public:
        MeshRegistry() = default;
        MeshRegistry(const MeshRegistry&) = delete;
        MeshRegistry& operator=(const MeshRegistry&) = delete;
        MeshRegistry(MeshRegistry&&) = default;
        MeshRegistry& operator=(MeshRegistry&&) = default;

        // ---- ロード（.obj）-----------------------------------------------------
        // 同じパスを2回渡すとキャッシュから返す
        MeshID Load(ID3D11Device* device, const wchar_t* path)
        {
            std::wstring key(path);
            auto it = m_pathToId.find(key);
            if (it != m_pathToId.end())
                return it->second;

            MeshData mesh = MeshLoader::LoadObj(device, path);
            return Register(std::move(mesh), key);
        }

        // 生成済み MeshData を直接登録（プロシージャル生成など）
        MeshID Register(MeshData mesh, const std::wstring& debugName = L"")
        {
            MeshID id = static_cast<MeshID>(m_meshes.size());
            m_pathToId[debugName] = id;
            m_meshes.push_back(std::move(mesh));
            return id;
        }

        // ---- アクセス -----------------------------------------------------------
        MeshData* Get(MeshID id) noexcept
        {
            if (id >= m_meshes.size())
                return nullptr;
            return &m_meshes[id];
        }
        const MeshData* Get(MeshID id) const noexcept
        {
            if (id >= m_meshes.size())
                return nullptr;
            return &m_meshes[id];
        }

        bool IsValid(MeshID id) const noexcept
        {
            return id < m_meshes.size() && m_meshes[id].IsValid();
        }

        std::size_t Count() const noexcept
        {
            return m_meshes.size();
        }

        // ---- インスタンスバッファ更新 -------------------------------------------
        // InstancedRenderSystem が毎フレーム呼ぶ。
        // worldMatrices: カリング済みワールド行列の配列
        // 戻り値: 実際に書き込んだインスタンス数（MAX_INSTANCES でクランプ）
        uint32_t UpdateInstanceBuffer(ID3D11DeviceContext* context, MeshID id,
                                      const std::vector<InstanceData>& instances)
        {
            MeshData* mesh = Get(id);
            if (!mesh || !mesh->instanceBuffer || instances.empty())
                return 0;

            const uint32_t count =
                static_cast<uint32_t>(std::min(instances.size(), static_cast<std::size_t>(MAX_INSTANCES)));

            D3D11_MAPPED_SUBRESOURCE mapped = {};
            HRESULT hr = context->Map(mesh->instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            if (FAILED(hr))
                return 0;

            std::memcpy(mapped.pData, instances.data(), sizeof(InstanceData) * count);
            context->Unmap(mesh->instanceBuffer.Get(), 0);

            return count;
        }

    private:
        std::vector<MeshData> m_meshes;
        std::unordered_map<std::wstring, MeshID> m_pathToId;
    };

} // namespace Graphics