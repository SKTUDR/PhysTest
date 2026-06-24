#include "pch.h"
#include "PhysicsWorld.h"

void ECS::PhysicsWorld::UpdateBroadPhase(const std::vector<ECS::SweepAndPrune::Entry>& entries)
{
    
    m_sap.Update(entries);
    //MarkBroadPhaseDirty();
}

const std::vector<ECS::SweepAndPrune::OverlapPair>& ECS::PhysicsWorld::GetOverlaps() const
{
    return m_sap.GetOverlaps();
}

void ECS::PhysicsWorld::MarkBroadPhaseDirty()
{
    m_sap.MarkDirty();
}
