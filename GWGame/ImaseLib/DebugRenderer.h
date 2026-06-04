/**
 * @file   DebugRenderer.h
 *
 * @brief  デバッグ描画関連のヘッダファイル
 *
 * @author S.Takaki
 *
 * @date   2026/03/26
 */

// 多重インクルードの防止 =====================================================
#pragma once



// ヘッダファイルの読み込み ===================================================
#include <format>           // std::format
#include <memory>           // std::unique_ptr
#include <string>           // std::wstring
#include <string_view>      // std::wstring_view
#include <vector>           // std::vector

#include <d3d11.h>          // ID3D11Device, ID3D11DeviceContext

#include <CommonStates.h>   // CommonStates



// クラス定義 =================================================================
namespace Imase
{
    /**
     * @brief デバッグ描画管理クラス
     */
    class DebugRenderer final
    {
    //-------------------------------------------------------------------------
    // メンバ関数の宣言
	//-------------------------------------------------------------------------
    // コンストラクタ / デストラクタ
    public:
        // コンストラクタ
        DebugRenderer(ID3D11Device* device, ID3D11DeviceContext* deviceContext, std::wstring_view fontPath);

        // デストラクタ
        ~DebugRenderer();


    // 操作
    public:
        // 2D/3Dテキスト描画 ----------       
        // @brief 書式なし
        void DrawText(const DirectX::SimpleMath::Vector2& position, DirectX::FXMVECTOR color, float scale, std::wstring text);
        void DrawText(const DirectX::SimpleMath::Vector3& position, DirectX::FXMVECTOR color, float scale, std::wstring text);

        // @brief 書式なし・スケール省略
        void DrawText(const DirectX::SimpleMath::Vector2& position, DirectX::FXMVECTOR color, std::wstring text);
        void DrawText(const DirectX::SimpleMath::Vector3& position, DirectX::FXMVECTOR color, std::wstring text);

        // @brief 書式なし・色とスケール省略
        void DrawText(const DirectX::SimpleMath::Vector2& position, std::wstring text);
        void DrawText(const DirectX::SimpleMath::Vector3& position, std::wstring text);

        // @brief 書式付き
        template<typename... Args>
        void DrawText(const DirectX::SimpleMath::Vector2& position, DirectX::FXMVECTOR color, float scale, std::wformat_string<Args...> fmt, Args&&... args);
        template<typename... Args>
        void DrawText(const DirectX::SimpleMath::Vector3& position, DirectX::FXMVECTOR color, float scale, std::wformat_string<Args...> fmt, Args&&... args);

        // @brief 書式付き・スケール省略
        template<typename... Args>
        void DrawText(const DirectX::SimpleMath::Vector2& position, DirectX::FXMVECTOR color, std::wformat_string<Args...> fmt, Args&&... args);
        template<typename... Args>
        void DrawText(const DirectX::SimpleMath::Vector3& position, DirectX::FXMVECTOR color, std::wformat_string<Args...> fmt, Args&&... args);

        // @brief 書式付き・色とスケール省略
        template<typename... Args>
        void DrawText(const DirectX::SimpleMath::Vector2& position, std::wformat_string<Args...> fmt, Args&&... args);
        template<typename... Args>
        void DrawText(const DirectX::SimpleMath::Vector3& position, std::wformat_string<Args...> fmt, Args&&... args);


        /// @brief 描画実行
        void Render(ID3D11DeviceContext* deviceContext, DirectX::CommonStates* commonStates);
        void Render(ID3D11DeviceContext* deviceContext, DirectX::CommonStates* commonStates, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& projection);



    //-------------------------------------------------------------------------
    // 内部型の宣言
    //-------------------------------------------------------------------------
    private:
        /**
         * @brief 描画処理に必要なコンテキスト
         *
         * DebugRenderer内部の各レンダラーに渡される
         * 描画関連リソースをまとめた補助構造体
         */
        struct RenderContext;


        /**
         * @brief 内部レンダラーのインターフェース
         */
        class IRenderer;


        /**
         * @brief テキスト描画を担当するレンダラー
         */
        class TextRenderer;


    //-------------------------------------------------------------------------
    // データメンバの宣言
	//-------------------------------------------------------------------------
    private:
        // 描画を担当するレンダラー ----------
        std::unique_ptr<TextRenderer> m_textRenderer;    ///< テキスト用レンダラー


        std::vector<IRenderer*> m_renderers;    ///< 描画順制御用のレンダラー参照リスト
    };

}    // namespace Imase




// テンプレート関数定義 =================================================================
namespace Imase
{
    /**
     * @brief 2Dテキスト描画(書式付き)
     *
     * 実際の描画処理はRender()が呼び出された際に行われる 
     * 
     * @tparam Args... 書式指定文字列に渡す引数の型リスト
     * 
     * @param[in] position 描画座標(スクリーン座標系・左上)
     * @param[in] color    描画色
     * @param[in] scale    フォントの描画スケール
     * @param[in] fmt      書式指定文字列
     * @param[in] args     書式指定文字列に埋め込むデータ(可変引数)
     * 
     * @return なし
     */
    template<typename ...Args>
    inline void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector2& position, DirectX::FXMVECTOR color, float scale, std::wformat_string<Args...> fmt, Args&&... args)
    {
        DrawText(position, color, scale, std::format(fmt, std::forward<Args>(args)...));
    }



    /**
     * @brief 3Dテキスト描画(書式付き)
     *
     * 実際の描画処理はRender()が呼び出された際に行われる
     *
     * @tparam Args... 書式指定文字列に渡す引数の型リスト
     *
     * @param[in] position 描画座標(ワールド座標系・中心)
     * @param[in] color    描画色
     * @param[in] scale    フォントの描画スケール
     * @param[in] fmt      書式指定文字列
     * @param[in] args     書式指定文字列に埋め込むデータ(可変引数)
     *
     * @return なし
     */
    template<typename ...Args>
    inline void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector3& position, DirectX::FXMVECTOR color, float scale, std::wformat_string<Args...> fmt, Args&&... args)
    {
        DrawText(position, color, scale, std::format(fmt, std::forward<Args>(args)...));
    }



    /**
     * @brief 2Dテキスト描画(書式付き・スケール省略)
     *
     * 【等倍サイズ】でテキストを描画
     * 実際の描画処理はRender()が呼び出された際に行われる
     * 
     * @tparam Args... 書式指定文字列に渡す引数の型リスト
     * 
     * @param[in] position 描画座標(スクリーン座標系・左上)
     * @param[in] color    描画色
     * @param[in] fmt      書式指定文字列
     * @param[in] args     書式指定文字列に埋め込むデータ(可変引数)
     * 
     * @return なし
     */
    template<typename ...Args>
    inline void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector2& position, DirectX::FXMVECTOR color, std::wformat_string<Args...> fmt, Args&&... args)
    {
        DrawText(position, color, 1.0f, std::format(fmt, std::forward<Args>(args)...));
    }



    /**
     * @brief 3Dテキスト描画(書式付き・スケール省略)
     *
     *【等倍サイズ】でテキストを描画
     * 実際の描画処理はRender()が呼び出された際に行われる
     * 
     * @tparam Args... 書式指定文字列に渡す引数の型リスト
     * 
     * @param[in] position 描画座標(ワールド座標系・中心)
     * @param[in] color    描画色
     * @param[in] fmt      書式指定文字列
     * @param[in] args     書式指定文字列に埋め込むデータ(可変引数)
     * 
     * @return なし
     */
    template<typename ...Args>
    inline void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector3& position, DirectX::FXMVECTOR color, std::wformat_string<Args...> fmt, Args&&... args)
    {
        DrawText(position, color, 1.0f, std::format(fmt, std::forward<Args>(args)...));
    }



    /**
     * @brief 2Dテキスト描画(書式付き・色とスケール省略)
     *
     * 【白色】【等倍サイズ】でテキストを描画
     * 実際の描画処理はRender()が呼び出された際に行われる
     * 
     * @tparam Args... 書式指定文字列に渡す引数の型リスト
     * 
     * @param[in] position 描画座標(スクリーン座標系・左上)
     * @param[in] fmt      書式指定文字列
     * @param[in] args     書式指定文字列に埋め込むデータ(可変引数)
     * 
     * @return なし
     */
    template<typename ...Args>
    inline void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector2& position, std::wformat_string<Args...> fmt, Args&&... args)
    {
        DrawText(position, DirectX::Colors::White, 1.0f, std::format(fmt, std::forward<Args>(args)...));
    }



    /**
     * @brief 3Dテキスト描画(書式付き・色とスケール省略)
     *
     * 【白色】【等倍サイズ】でテキストを描画
     * 実際の描画処理はRender()が呼び出された際に行われる
     * 
     * @tparam Args... 書式指定文字列に渡す引数の型リスト
     * 
     * @param[in] position 描画座標(スクリーン座標系・中心)
     * @param[in] fmt      書式指定文字列
     * @param[in] args     書式指定文字列に埋め込むデータ(可変引数)
     * 
     * @return なし
     */
    template<typename ...Args>
    inline void DebugRenderer::DrawText(const DirectX::SimpleMath::Vector3& position, std::wformat_string<Args...> fmt, Args&&... args)
    {
        DrawText(position, DirectX::Colors::White, 1.0f, std::format(fmt, std::forward<Args>(args)...));
    }

}    // namespace Imase