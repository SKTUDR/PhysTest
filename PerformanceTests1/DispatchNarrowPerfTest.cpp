#include "pch.h"
#include "CppUnitTest.h"
#include "../GWGame/Systems/CollisionSystem.h"
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(DispatchNarrowPerfTest)
{
public:
    TEST_METHOD(DispatchNarrowHotPath)
    {
        using namespace ECS;

        ColliderComp a, b;
        ContactPoint out;

        // 初期形状を設定（OBB vs OBB を中心にホットパスを叩く）
        a.shape = OBB{{0.5f, 0.5f, 0.5f}, DirectX::SimpleMath::Quaternion::Identity};
        b.shape = OBB{{0.5f, 0.5f, 0.5f}, DirectX::SimpleMath::Quaternion::Identity};

        DirectX::SimpleMath::Vector3 pa{0.f, 0.f, 0.f};
        DirectX::SimpleMath::Vector3 pb{0.6f, 0.f, 0.f};

        const int ITER = 200000; // 実行時間の許容範囲に応じて調整
        for (int i = 0; i < ITER; ++i)
        {
            // 微小に位置を変えてパス条件を変化させる
            pb.x += 1e-5f;
            if (pb.x > 1.0f) pb.x = 0.6f;
            volatile bool hit = CollisionSystem::DispatchNarrow(a, pa, b, pb, out);
            (void)hit;
        }
    }
};
