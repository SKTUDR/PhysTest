#pragma once

// include ----------------------------------
#include "SweepAndPrune.h"

namespace ECS
{
    /// @brief 物理空間を管理するクラス
    class PhysicsWorld
    {
    public:
        // BroadPhase更新
        void UpdateBroadPhase(const std::vector<ECS::SweepAndPrune::Entry>& entries);

        // BroadPhase結果取得
        const std::vector<ECS::SweepAndPrune::OverlapPair>& GetOverlaps() const;

        // Entity数変化通知
        void MarkBroadPhaseDirty(); 

    private:
        // BroadPhase
        ECS::SweepAndPrune m_sap;
    };
}