/*
 * BigGunTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/BigGunTask.h"
#include "module/MilitaryManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CBBigGunTask::CBBigGunTask(ITaskManager* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   float cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::BIG_GUN, cost, shake, timeout)
{
}

CBBigGunTask::~CBBigGunTask()
{
}

void CBBigGunTask::Finish()
{
	manager->GetCircuit()->GetMilitaryManager()->DiceBigGun();
	IBuilderTask::Finish();
}

CAllyUnit* CBBigGunTask::FindSameAlly(CCircuitUnit* builder, const std::vector<Unit*>& friendlies)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	const int frame = circuit->GetLastFrame();

	for (Unit* au : friendlies) {
		CAllyUnit* alu = circuit->GetFriendlyUnit(au);
		if (alu == nullptr) {
			continue;
		}
		if (alu->GetCircuitDef()->IsRoleSuper() && au->IsBeingBuilt()) {
			const AIFloat3& pos = alu->GetPos(frame);
			if (terrainManager->CanBuildAtSafe(builder, pos)) {
				return alu;
			}
		}
	}
	return nullptr;
}

} // namespace circuit
