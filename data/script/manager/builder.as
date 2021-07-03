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
//			if ((ai.GetLastFrame() < hide.frame) || (aiBuilderMgr.GetWorkerCount() <= 2)) {
//				return aiBuilderMgr.MakeBuilderTask(unit);
//			}
//			if (enemyMgr.GetMobileThreat()/* / ai.GetAllySize()*/ >= hide.threat) {
//				return aiBuilderMgr.MakeCommTask(unit);
//			}
//			const bool isHide = hide.isAir && (aiEnemyMgr.GetEnemyCost(RT::AIR) > 1.f);
//			return isHide ? aiBuilderMgr.MakeCommTask(unit) : aiBuilderMgr.MakeBuilderTask(unit);
//		}
//	}
//
//	return aiBuilderMgr.MakeBuilderTask(unit);

//	aiDelPoint(lastPos);
//	lastPos = unit.GetPos(ai.GetLastFrame());
//	aiAddPoint(lastPos, "task");
//	return aiBuilderMgr.DefaultMakeTask(unit);

	IUnitTask@ task = aiBuilderMgr.DefaultMakeTask(unit);
//	if ((task !is null) && (task.GetType() == 5) && (task.GetBuildType() == 5)) {
//		aiDelPoint(task.GetBuildPos());
//		aiAddPoint(task.GetBuildPos(), "def");
//	}
	return task;
}

}  // namespace Builder
