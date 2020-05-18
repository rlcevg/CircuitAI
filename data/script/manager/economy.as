#include "../commander.as"


void OpenStrategy(const CCircuitDef@ facDef, const AIFloat3& in pos)
{
	const array<Opener::SO>@ opener = Opener::GetOpener(facDef);
	if (opener is null) {
		return;
	}
	float radius = aiMax(GetTerrainWidth(), GetTerrainHeight()) / 4;
	for (uint i = 0, icount = opener.length(); i < icount; ++i) {
		CCircuitDef@ buildDef = aiFactoryMgr.GetRoleDef(facDef, opener[i].role);
		if ((buildDef is null) || !buildDef.IsAvailable(ai.GetLastFrame())) {
			continue;
		}
		for (uint j = 0, jcount = opener[i].count; j < jcount; ++j) {
			aiFactoryMgr.EnqueueTask(1, buildDef, pos, 1, radius);  // FIXME
		}
	}
// 	const std::vector<CCircuitDef::RoleType>* opener = circuit->GetSetupManager()->GetOpener(facDef);
// 	if (opener == nullptr) {
// 		return;
// 	}
// 	CFactoryManager* factoryManager = circuit->GetFactoryManager();
// 	CTerrainManager* terrainManager = circuit->GetTerrainManager();
// 	float radius = std::max(terrainManager->GetTerrainWidth(), terrainManager->GetTerrainHeight()) / 4;
// 	for (CCircuitDef::RoleType type : *opener) {
// 		CCircuitDef* buildDef = factoryManager->GetRoleDef(facDef, type);
// 		if ((buildDef == nullptr) || !buildDef->IsAvailable(circuit->GetLastFrame())) {
// 			continue;
// 		}
// 		CRecruitTask::Priority priotiry;
// 		CRecruitTask::RecruitType recruit;
// 		if (type == CCircuitDef::TBUILDER) {
// 			priotiry = CRecruitTask::Priority::NORMAL;
// 			recruit  = CRecruitTask::RecruitType::BUILDPOWER;
// 		} else {
// 			priotiry = CRecruitTask::Priority::HIGH;
// 			recruit  = CRecruitTask::RecruitType::FIREPOWER;
// 		}
// 		factoryManager->EnqueueTask(priotiry, buildDef, pos, recruit, radius);
// 	}
}
