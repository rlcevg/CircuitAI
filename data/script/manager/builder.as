#include "../role.as"
#include "../commander.as"


IUnitTask@ makeTask(CCircuitUnit@ unit)
{
//	const CCircuitDef@ cdef = unit.GetCircuitDef();
//
//	if (cdef.IsRoleAny(RM::COMM)) {  // hide commander?
//		const Hide::SHide@ hide = Hide::getForUnitDef(cdef);
//		if (hide !is null) {
//			if ((ai.GetLastFrame() < hide.frame) || (builderMgr.GetWorkerCount() <= 2)) {
//				return builderMgr.MakeBuilderTask(unit);
//			}
//			if (enemyMgr.GetMobileThreat()/* / ai.GetAllySize()*/ >= hide.threat) {
//				return builderMgr.MakeCommTask(unit);
//			}
//			const bool isHide = hide.isAir && (enemyMgr.GetEnemyCost(RT::AIR) > 1.f);
//			return isHide ? builderMgr.MakeCommTask(unit) : builderMgr.MakeBuilderTask(unit);
//		}
//	}
//
//	return builderMgr.MakeBuilderTask(unit);

	const AIFloat3 pos = unit.GetPos(ai.GetLastFrame());
	aiDelPoint(pos);
	aiAddPoint(pos, "task");
	return builderMgr.DefaultMakeTask(unit);
}
