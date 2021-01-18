#include "../role.as"


namespace Military {

IUnitTask@ AiMakeTask(CCircuitUnit@ unit)
{
	return aiMilitaryMgr.DefaultMakeTask(unit);
}

void AiTaskCreated(IUnitTask@ task)
{
}

void AiTaskClosed(IUnitTask@ task, bool done)
{
}

void AiMakeDefence(int cluster, const AIFloat3& in pos)
{
	aiMilitaryMgr.DefaultMakeDefence(cluster, pos);
}

/*
 * anti-air threat threshold;
 * air factories will stop production when AA threat exceeds
 */
bool AiIsAirValid()
{
	return aiEnemyMgr.GetEnemyThreat(RT::AA) <= 80.f;
}

}  // namespace Military
