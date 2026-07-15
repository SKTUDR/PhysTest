#pragma once

namespace ECS
{

    // ---- AudioListenerComp ------------------------------------------------------
    // Unity の AudioListener に相当する空タグコンポーネント。
    // このコンポーネントを持つエンティティが「耳」の位置になる。
    // TransformComp::position / rotation から位置・向きを取得する。
    //
    // シーンに 1 つだけ付けること（複数ある場合は最初に見つかったものを使う）。
    // 通常はカメラや PlayerTagComp を持つエンティティに付ける。
    // ----------------------------------------------------------------------------
    struct AudioListenerComp
    {
    };

} // namespace ECS