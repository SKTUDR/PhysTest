#pragma once

// DirectXTK
#include <Model.h>          // DirectX::Model

namespace Graphics
{

    using ModelID = uint32_t;
    static constexpr ModelID INVALID_MODEL_ID = UINT32_MAX;

    // ---- ModelRegistry ----------------------------------------------------------
    // DirectXTK (DX11) の DirectX::Model の unique_ptr を一元管理する。
    // ECS コンポーネントは ModelID (uint32_t) だけ保持し、
    // RenderSystem がこの Registry 経由で Model* を取得して描画する。
    //
    // ライフタイム: Game オブジェクトが所有し、World より長生きすること。
    // ----------------------------------------------------------------------------
    class ModelRegistry
    {
    public:
        // .sdkmesh をロード。同じパスを2回渡すとキャッシュから返す。
        ModelID Load(ID3D11Device* device, const wchar_t* path, DirectX::EffectFactory& fx, DirectX::ModelLoaderFlags flags = DirectX::ModelLoaderFlags::ModelLoader_CounterClockwise)
        {
            std::wstring key(path);
            auto it = m_pathToId.find(key);
            if (it != m_pathToId.end())
                return it->second;

            auto model = DirectX::Model::CreateFromSDKMESH(device, path, fx, flags);
            return RegisterModel(std::move(model), key);
        }

        // .cmo をロード
        ModelID LoadCMO(ID3D11Device* device, const wchar_t* path, DirectX::ModelLoaderFlags flags = DirectX::ModelLoaderFlags::ModelLoader_CounterClockwise)
        {
            std::wstring key(path);
            auto it = m_pathToId.find(key);
            if (it != m_pathToId.end())
                return it->second;

            DirectX::EffectFactory fx(device);
            fx.SetDirectory(L"Resources/Models");


            auto model = DirectX::Model::CreateFromCMO(device, path, fx, flags);
            return RegisterModel(std::move(model), key);
        }

        // 生成済み unique_ptr を登録（プロシージャル生成など）
        ModelID RegisterModel(std::unique_ptr<DirectX::Model> model, const std::wstring& debugName = L"")
        {
            ModelID id = static_cast<ModelID>(m_models.size());
            m_pathToId[debugName] = id;
            m_models.push_back(std::move(model));
            return id;
        }

        // RenderSystem が毎フレーム呼ぶ
        DirectX::Model* Get(ModelID id) const noexcept
        {
            if (id >= m_models.size())
                return nullptr;
            return m_models[id].get();
        }

        bool IsValid(ModelID id) const noexcept
        {
            return id < m_models.size() && m_models[id] != nullptr;
        }

        std::size_t Count() const noexcept
        {
            return m_models.size();
        }

    private:
        std::vector<std::unique_ptr<DirectX::Model>> m_models;
        std::unordered_map<std::wstring, ModelID> m_pathToId;
    };

} // namespace Graphics
