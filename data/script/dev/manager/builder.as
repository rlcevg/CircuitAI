#include "../../default/role.as"


namespace Builder {

//AIFloat3 lastPos;
// int gPauseCnt = 0;

IUnitTask@ MakeTask(CCircuitUnit@ unit)
{
//	aiDelPoint(lastPos);
//	lastPos = unit.GetPos(ai.lastFrame);
//	aiAddPoint(lastPos, "task");

// 	IUnitTask@ task = aiBuilderMgr.DefaultMakeTask(unit);
// 	if ((task !is null) && (task.GetType() == 5)) {  // Type::BUILDER
// 		switch (task.GetBuildType()) {
// 		case 10:  // BuildType::MEX
// 			aiAddPoint(task.GetBuildPos(), task.GetBuildDef().GetName());
// 			break;
// 		case 5:  // BuildType::DEFENCE
// 			aiAddPoint(task.GetBuildPos(), task.GetBuildDef().GetName());
// 			break;
// 		default:
// 			break;
// 		}
// 	}
// 	return task;
	return aiBuilderMgr.DefaultMakeTask(unit);
}

void TaskCreated(IUnitTask@ task)
{
// 	if (task.GetType() != 5) {  // Type::BUILDER
// 		return;
// 	}
// 	switch (task.GetBuildType()) {
// 	case 4: {  // BuildType::ENERGY
// 		if (gPauseCnt == 0) {
// 			string name = task.GetBuildDef().GetName();
// 			if ((name == "armfus") || (name == "armafus") || (name == "corfus") || (name == "corafus")) {
// 				aiPause(true, "energy");
// 				aiAddPoint(task.GetBuildPos(), name);
// 				++gPauseCnt;
// 			}
// 		}
// 	} break;
// 	case 10:  // BuildType::MEX
// 	case 5:  // BuildType::DEFENCE
// 		aiAddPoint(task.GetBuildPos(), task.GetBuildDef().GetName());
// 		break;
// 	default:
// 		break;
// 	}
}

void TaskDead(IUnitTask@ task, bool done)
{
// 	if (task.GetType() != 5) {  // Type::BUILDER
// 		return;
// 	}
// 	switch (task.GetBuildType()) {
// 	case 10:  // BuildType::MEX
// 	case 5:  // BuildType::DEFENCE
// 	case 4:  // BuildType::ENERGY
// 		aiDelPoint(task.GetBuildPos());
// 		break;
// 	default:
// 		break;
// 	}
}

}  // namespace Builder
