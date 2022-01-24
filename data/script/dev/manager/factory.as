#include "../../define.as"


namespace Factory {

string armlab ("armlab");
string armalab("armalab");
string armvp  ("armvp");
string armavp ("armavp");

Id LAB  = ai.GetCircuitDef(armlab).id;
Id ALAB = ai.GetCircuitDef(armalab).id;
Id VP   = ai.GetCircuitDef(armvp).id;
Id AVP  = ai.GetCircuitDef(armavp).id;

int switchInterval = MakeSwitchInterval();

IUnitTask@ AiMakeTask(CCircuitUnit@ unit)
{
	return aiFactoryMgr.DefaultMakeTask(unit);
}

void AiTaskCreated(IUnitTask@ task)
{
}

void AiTaskClosed(IUnitTask@ task, bool done)
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
	const bool isOK = aiMilitaryMgr.armyCost > 3000.f;
	if (!isOK) {
		aiFactoryMgr.isAssistRequired = true;
	}
	return isOK;
}

/* --- Utils --- */

int MakeSwitchInterval()
{
	return AiRandom(550, 900) * SECOND;
}

}  // namespace Factory
