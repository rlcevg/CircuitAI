#include "../../default/role.as"


namespace Factory {

string armlab ("armlab");
string armalab("armalab");
string armvp  ("armvp");
string armavp ("armavp");

Id LAB  = ai.GetCircuitDef(armlab).id;
Id ALAB = ai.GetCircuitDef(armalab).id;
Id VP   = ai.GetCircuitDef(armvp).id;
Id AVP  = ai.GetCircuitDef(armavp).id;

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

}  // namespace Factory
