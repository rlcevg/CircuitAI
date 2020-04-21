#include "../role.as"
#include "../commander.as"


namespace Builder {

//AIFloat3 lastPos;

IUnitTask@ MakeTask(CCircuitUnit@ unit)
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

//	aiDelPoint(lastPos);
//	lastPos = unit.GetPos(ai.GetLastFrame());
//	aiAddPoint(lastPos, "task");
	return builderMgr.DefaultMakeTask(unit);
}

}  // namespace Builder
