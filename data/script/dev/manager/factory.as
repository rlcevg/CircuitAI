#include "../../define.as"
#include "../../unit.as"
#include "../../task.as"
#include "../misc/commander.as"
#include "economy.as"


namespace Factory {

string armlab ("armlab");
string armalab("armalab");
string armavp ("armavp");
string armasy ("armasy");
string armap  ("armap");
string corlab ("corlab");
string coralab("coralab");
string coravp ("coravp");
string corasy ("corasy");
string corap  ("corap");

int switchInterval = MakeSwitchInterval();

IUnitTask@ AiMakeTask(CCircuitUnit@ unit)
{
	// TODO: Add API to create any tasks
	return aiFactoryMgr.DefaultMakeTask(unit);
}

void AiTaskCreated(IUnitTask@ task)
{
}

void AiTaskClosed(IUnitTask@ task, bool done)
{
}

void AiFactoryCreated(CCircuitUnit@ unit)
{
//	if (!factories.empty() || (this->circuit->GetBuilderManager()->GetWorkerCount() > 2)) return;
	if (ai.IsLoadSave()) {
		return;
	}

	const CCircuitDef@ facDef = unit.circuitDef;
	const array<Opener::SO>@ opener = Opener::GetOpener(facDef);
	if (opener is null) {
		return;
	}
	const AIFloat3 pos = unit.GetPos(ai.frame);
	for (uint i = 0, icount = opener.length(); i < icount; ++i) {
		CCircuitDef@ buildDef = aiFactoryMgr.GetRoleDef(facDef, opener[i].role);
		if ((buildDef is null) || !buildDef.IsAvailable(ai.frame)) {
			continue;
		}
		Task::Priority priotiry;
		Task::RecruitType recruit;
		if (opener[i].role == Unit::Role::BUILDER.type) {
			priotiry = Task::Priority::NORMAL;
			recruit  = Task::RecruitType::BUILDPOWER;
		} else {
			priotiry = Task::Priority::HIGH;
			recruit  = Task::RecruitType::FIREPOWER;
		}
		for (uint j = 0, jcount = opener[i].count; j < jcount; ++j) {
			aiFactoryMgr.EnqueueTask(priotiry, buildDef, pos, recruit, 64.f);
		}
	}
}

void AiFactoryDestroyed(CCircuitUnit@ unit)
{
}

/*
 * New factory switch condition; switch event is also based on eco + caretakers.
 */
bool AiIsSwitchTime(int lastSwitchFrame)
{
	if (lastSwitchFrame + switchInterval <= ai.frame) {
		switchInterval = MakeSwitchInterval();
		return true;
	}
	return false;
}

bool AiIsSwitchAllowed(CCircuitDef@ facDef)
{
	const bool isOK = (aiMilitaryMgr.armyCost > 1.2f * facDef.costM * aiFactoryMgr.GetFactoryCount())
		|| (aiEconomyMgr.metal.current > facDef.costM);
	aiFactoryMgr.isAssistRequired = Economy::isSwitchAssist = !isOK;
	return isOK;
}

/* --- Utils --- */

int MakeSwitchInterval()
{
	return AiRandom(550, 900) * SECOND;
}

}  // namespace Factory
