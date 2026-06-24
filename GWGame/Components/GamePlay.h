#pragma once

namespace ECS
{
    // --------------------------------------------------------
    //  LifetimeComponent
    //  一定時間後に自動的に破棄されるエンティティに付与
    // --------------------------------------------------------
    struct LifetimeComp
    {
        float remainingTime = 5.0f;
    };
    // --------------------------------------------------------
    //  EntitiesTagComp
    //  処理分岐用
    // --------------------------------------------------------
    struct EnemyTagComp{};
    struct PlayerTagComp{};
    
    struct BossTagComp{};
    struct GroundTagComp{};


    // --------------------------------------------------------
    //  DebugTagComponent
    //  エンティティを文字列で識別するためのタグ
    // --------------------------------------------------------
    struct DebugTagComp
    {
        std::string tag;
        DebugTagComp() = default;
        explicit DebugTagComp(const std::string& t) : tag(t)
        {
        }
    };

    // --------------------------------------------------------
    //  PlayerInputComponent
    //  プレイヤー操作を受け付けるエンティティのマーカー
    // --------------------------------------------------------
    struct PlayerInputComponent
    {
        float moveSpeed = 3.0f;
        float rotateSpeed = 2.0f;
    };
} // namespace ECS

