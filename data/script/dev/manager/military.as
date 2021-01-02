#include "../../default/role.as"


namespace Military {

IUnitTask@ MakeTask(CCircuitUnit@ unit)
{
	return aiMilitaryMgr.DefaultMakeTask(unit);
}

void MakeDefence(int cluster, const AIFloat3& in pos)
{
	if ((ai.lastFrame > 5 * 60 * 30)
		|| (aiEconomyMgr.metal.income > 10.f)
		|| (aiEnemyMgr.mobileThreat > 0.f))
	{
		aiMilitaryMgr.DefaultMakeDefence(cluster, pos);
//		aiAddPoint(pos, "def");
	}
}

/*
 * anti-air threat threshold;
 * air factories will stop production when AA threat exceeds
 */
bool IsAirValid()
{
	return aiEnemyMgr.GetEnemyThreat(RT::AA) <= 80.f;
}

}  // namespace Military
