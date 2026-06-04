/**
 * @file   DebugRenderer.cpp
 *
 * @brief  デバッグ描画関連のソースファイル
 *
 * @author S.Takaki
 *
 * @date   2026/03/26
 */

// ヘッダファイルの読み込み ===================================================
#include "pch.h"

#include "DebugRenderer.h"

#include <SpriteBatch.h>    // SpriteBatch
#include <SpriteFont.h>     // SpriteFont




// usingディレクティブ ========================================================
using namespace DirectX;




// 構造体定義 =================================================================
namespace Imase
{
    /**
     * @brief 描画処理に必要なコンテキスト
     */
    struct DebugRenderer::RenderContext
    {
        ID3D11DeviceContext*      deviceContext;    ///< 描画に使用するD3D11デバイスコンテキスト
        const CommonStates*       commonStates;     ///< DirectXTKの共通ステート
        const SimpleMath::Matrix& matView;          ///< ビュー行列
        const SimpleMath::Matrix& matProjection;    ///< プロジェクション行列
    };

}    // namespace Imase




// クラス定義 =================================================================
namespace Imase
{
    /**
     * @brief 内部レンダラーのインターフェース
     * 
     * DebugRendererが管理する各描画処理の共通インターフェース
     */
    class DebugRenderer::IRenderer
    {
    public:
        /**
         * @brief デストラクタ
         */
        virtual ~IRenderer() = default;
       

        /**
         * @brief 蓄積されたテキスト描画コマンドを画面に描画
         *
         * @param[in] renderContext 描画関連のコンテキスト
         *
         * @return なし
         */
        virtual void Render(const RenderContext& ctx) = 0;
    };




    /**
     * @brief テキスト描画クラス
     */
    class DebugRenderer::TextRenderer final: public DebugRenderer::IRenderer 
    {
    // メンバ関数の宣言 -------------------------------------------------------
    // コンストラクタ / デストラクタ
    public:
        /**
         * @brief コンストラクタ
         *
         * @param[in] device        デバイス
         * @param[in] deviceContext デバイスコンテキスト
         * @param[in] fontPath      フォントのパス
         */
        TextRenderer(ID3D11Device* device, ID3D11DeviceContext* deviceContext, std::wstring_view fontPath)
            : m_spriteBatch{ std::make_unique<SpriteBatch>(deviceContext) }
            , m_spriteFont{ std::make_unique<SpriteFont>(device, fontPath.data()) }
            , m_text2DCommands{}
            , m_text3DCommands{}
        {
        }


        /**
         * @brief デストラクタ
         */
        ~TextRenderer() = default;



    // 操作
    public:
        /**
         * @brief 2Dテキスト描画コマンドの追加
         *
         * 受け取った描画情報を2Dテキスト用のコマンドバッファに格納
         *
         * @param[in] position 描画座標(スクリーン座標系・左上)
         * @param[in] color    描画色
         * @param[in] scale    描画スケール
         * @param[in] text     表示する文字列
         */
        void Add(const SimpleMath::Vector2& position, DirectX::FXMVECTOR color, float scale, std::wstring text)
        {
            m_text2DCommands.push_back(
                {
                    .text     = std::move(text),
                    .position = position,
                    .color    = color,
                    .scale    = scale
                }
            );
        }


        /**
         * @brief 3Dテキスト描画コマンドの追加
         *
         * 受け取った描画情報を3Dテキスト用のコマンドバッファに格納
         *
         * @param[in] position 描画座標(ワールド座標系・中心)
         * @param[in] color    描画色
         * @param[in] scale    描画スケール
         * @param[in] text     表示する文字列
         */
        void Add(const SimpleMath::Vector3& position, DirectX::FXMVECTOR color, float scale, std::wstring text)
        {
            m_text3DCommands.push_back(
                {
                    .text     = std::move(text),
                    .position = position,
                    .color    = color,
                    .scale    = scale
                }
            );
        }


        /**
         * @brief 蓄積されたテキスト描画コマンドを画面に描画
         *
         * @param[in] renderContext 描画関連のコンテキスト
         *
         * @return なし
         */
        void Render(const RenderContext& renderContext) override
        {
            // 3Dテキストがある時だけ呼び出す
            if (!m_text3DCommands.empty())
            {
                Render3D(renderContext);
            }


            // 2Dテキストがある時だけ呼び出す
            if (!m_text2DCommands.empty())
            {
                Render2D(renderContext);
            }
        }
    
        

    // 内部操作
    private:
        /**
         * @brief 蓄積された2Dテキスト描画コマンドを画面に描画
         *
         * @param[in] renderContext 描画関連のコンテキスト
         *
         * @return なし
         */
        void Render2D(const RenderContext& renderContext)
        {
            // コンテキストから必要なリソースを抽出
            const CommonStates* commonStates = renderContext.commonStates;


            // 深度テストを無効化して描画
            //  ※デバッグ用テキストを常に最前面に表示するため
            m_spriteBatch->Begin(SpriteSortMode_Deferred, nullptr, nullptr, commonStates->DepthNone(), commonStates->CullCounterClockwise());

            for (const Text2DCommand& cmd : m_text2DCommands)
            {
                m_spriteFont->DrawString(m_spriteBatch.get(), cmd.text.c_str(), cmd.position, cmd.color, 0.0f, SimpleMath::Vector2::Zero, cmd.scale);
            }

            m_spriteBatch->End();


            // 1フレーム分の描画コマンドをクリア
            m_text2DCommands.clear();
        }


        /**
         * @brief 蓄積された3Dテキスト描画コマンドを画面に描画
         *
         * @param[in] renderContext 描画関連のコンテキスト
         *
         * @return なし
         */
        void Render3D(const RenderContext& renderContext)
        {
            // コンテキストから必要なリソースを抽出
            const CommonStates* commonStates = renderContext.commonStates;
            const SimpleMath::Matrix& matView = renderContext.matView;
            const SimpleMath::Matrix& matProjection = renderContext.matProjection;


            // ビューポートの取得
            uint32_t numViewports = 1;
            D3D11_VIEWPORT d3dvp{};
            renderContext.deviceContext->RSGetViewports(&numViewports, &d3dvp);
            DirectX::SimpleMath::Viewport viewport{ d3dvp };


            // 深度テストを無効化して描画
            //  ※デバッグ用テキストを常に最前面に表示するため
            m_spriteBatch->Begin(SpriteSortMode_Deferred, nullptr, nullptr, commonStates->DepthNone(), commonStates->CullCounterClockwise());

            for (const Text3DCommand& cmd : m_text3DCommands)
            {
                // ワールド座標をスクリーン座標に投影
                DirectX::SimpleMath::Vector3 screenPosition = viewport.Project(cmd.position, matProjection, matView, SimpleMath::Matrix::Identity);

                // カメラの背後（Z<0またはZ>1）にある場合は描画をスキップ
                if (screenPosition.z < 0.0f || screenPosition.z > 1.0f)
                {
                    continue;
                }

                // 中心点の算出
                SimpleMath::Vector2 origin = m_spriteFont->MeasureString(cmd.text.c_str()) / 2;


                m_spriteFont->DrawString(m_spriteBatch.get(), cmd.text.c_str(), { screenPosition.x, screenPosition.y }, cmd.color, 0.0f, origin, cmd.scale);
            }

            m_spriteBatch->End();


            // 1フレーム分の描画コマンドをクリア
            m_text3DCommands.clear();
        }



    // 内部構造体の定義 -------------------------------------------------------
    private:
         /**
          * @brief 2Dテキスト描画コマンド
          */
        struct Text2DCommand
        {
            std::wstring        text;        ///< 描画する文字列
            SimpleMath::Vector2 position;    ///< 描画座標(スクリーン座標系・左上)
            SimpleMath::Color   color;       ///< 描画色
            float               scale;       ///< フォントの描画スケール
        };


        /**
         * @brief 3Dテキスト描画コマンド
         */
        struct Text3DCommand
        {
            std::wstring        text;        ///< 描画する文字列
            SimpleMath::Vector3 position;    ///< 描画座標(ワールド座標系・中心)
            SimpleMath::Color   color;       ///< 描画色
            float               scale;       ///< フォントの描画スケール
        };



	// データメンバの宣言 -----------------------------------------------------
    private:
        // 描画リソース ----------
        std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;    ///< スプライトバッチ
        std::unique_ptr<DirectX::SpriteFont>  m_spriteFont;     ///< スプライトフォント

		// 描画コマンド ----------
        std::vector<Text2DCommand> m_text2DCommands;    ///< 2Dテキスト描画コマンドのリスト
        std::vector<Text3DCommand> m_text3DCommands;    ///< 3Dテキスト描画コマンドのリスト
    };

}    // namespace Imase




// メンバ関数定義 =============================================================
namespace Imase
{
    //-----------------------------------------------------------------------------
    // DebugRenderer実装
    //-----------------------------------------------------------------------------
    /**
     * @brief コンストラクタ
     *
     * @param[in] device        デバイス
     * @param[in] deviceContext デバイスコンテキスト
     * @param[in] fontPath      フォントのパス
     */
    DebugRenderer::DebugRenderer(ID3D11Device* device, ID3D11DeviceContext* deviceContext, std::wstring_view fontPath)
        : m_textRenderer{ std::make_unique<TextRenderer>(device, deviceContext, fontPath) }
		, m_renderers{}
    {
		m_renderers.push_back(m_textRenderer.get());
    }



    /**
     * @brief デストラクタ
     */
    DebugRenderer::~DebugRenderer() = default;



    /**
     * @brief 2Dテキスト描画(書式なし)
     * 
     * 実際の描画処理はRender()が呼び出された際に行われる。
     *
     * @param[in] position 描画座標(スクリーン座標系・左上)
     * @param[in] color    描画色
     * @param[in] scale    フォントの描画スケール
     * @param[in] text     描画する文字列
     *
     * @return なし
     */
    void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector2& position, DirectX::FXMVECTOR color, float scale, std::wstring text)
    {
        m_textRenderer->Add(position, color, scale, std::move(text));
    }



    /**
     * @brief 3Dテキスト描画(書式なし)
     * 
     * 実際の描画処理はRender()が呼び出された際に行われる。
     *
     * @param[in] position 描画座標(ワールド座標系・中心)
     * @param[in] color    描画色
     * @param[in] scale    フォントの描画スケール
     * @param[in] text     描画する文字列
     *
     * @return なし
     */
    void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector3& position, DirectX::FXMVECTOR color, float scale, std::wstring text)
    {
        m_textRenderer->Add(position, color, scale, std::move(text));
    }



    /**
     * @brief 2Dテキスト描画(書式なし・スケール省略)
     * 
     * 【等倍サイズ】でテキストを描画する。
     * 実際の描画処理はRender()が呼び出された際に行われる。
     *
     * @param[in] position 描画座標(スクリーン座標系・左上)
     * @param[in] color    描画色
     * @param[in] text     描画する文字列
     *
     * @return なし
     */
    void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector2& position, DirectX::FXMVECTOR color, std::wstring text)
    {
        DrawText(position, color, 1.0f, std::move(text));
    }



    /**
     * @brief 3Dテキスト描画(書式なし・スケール省略)
     * 
     * 【等倍サイズ】でテキストを描画する。
     * 実際の描画処理はRender()が呼び出された際に行われる。
     *
     * @param[in] position 描画座標(ワールド座標系・中心)
     * @param[in] color    描画色
     * @param[in] text     描画する文字列
     *
     * @return なし
     */
    void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector3& position, DirectX::FXMVECTOR color, std::wstring text)
    {
        DrawText(position, color, 1.0f, std::move(text));
    }



    /**
     * @brief 2Dテキスト描画(書式なし・色とスケール省略)
     *
     * 【白色】【等倍サイズ】でテキストを描画する。
     * 実際の描画処理はRender()が呼び出された際に行われる。
     *
     * @param[in] position 描画座標(スクリーン座標系・左上)
     * @param[in] text     描画する文字列
     *
     * @return なし
     */
    void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector2& position, std::wstring text)
    {
        DrawText(position, DirectX::Colors::White, 1.0f, std::move(text));
    }



    /**
     * @brief 3Dテキスト描画(書式なし・色とスケール省略)
     *
     * 【白色】【等倍サイズ】でテキストを描画する。
     * 実際の描画処理はRender()が呼び出された際に行われる。
     *
     * @param[in] position 描画座標(ワールド座標系・中心)
     * @param[in] text     描画する文字列
     *
     * @return なし
     */
    void Imase::DebugRenderer::DrawText(const DirectX::SimpleMath::Vector3& position, std::wstring text)
    {
        DrawText(position, DirectX::Colors::White, 1.0f, std::move(text));
    }



    /**
     * @brief 1フレームの間に蓄積された描画コマンドを実際に描画
     * 
	 * 2Dテキスト描画用の簡易版。ビュー行列とプロジェクション行列は内部で単位行列を使用
     *
     * @param[in] deviceContext 描画に使用するD3D11デバイスコンテキスト
     * @param[in] commonStates  DirectXTKの共通ステート
     *
     * @return なし
     */
    void DebugRenderer::Render(ID3D11DeviceContext* deviceContext, DirectX::CommonStates* commonStates)
    {
		Render(deviceContext, commonStates, DirectX::SimpleMath::Matrix::Identity, DirectX::SimpleMath::Matrix::Identity);
    }



    /**
     * @brief 1フレームの間に蓄積された描画コマンドを実際に描画
     *
     * @param[in] deviceContext 描画に使用するD3D11デバイスコンテキスト
     * @param[in] commonStates  DirectXTKの共通ステート
     * @param[in] view          ビュー行列
     * @param[in] projection    プロジェクション行列
     *
     * @return なし
     */
    void DebugRenderer::Render(ID3D11DeviceContext* deviceContext, DirectX::CommonStates* commonStates, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& projection)
    {
        // 各レンダラーに渡すための描画コンテキストを構築
        RenderContext renderContext
        {
            .deviceContext = deviceContext,
            .commonStates  = commonStates,
            .matView       = view,
            .matProjection = projection
        };


		// 内部レンダラーに描画処理を実行させる
        for (IRenderer* renderer : m_renderers)
        {
            renderer->Render(renderContext);
        }
    }

}    // namespace Imase
