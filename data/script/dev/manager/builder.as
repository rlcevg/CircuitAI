#include "../../unit.as"


namespace Builder {

CCircuitUnit@ energizer1 = null;
CCircuitUnit@ energizer2 = null;

// AIFloat3 lastPos;
// int gPauseCnt = 0;

IUnitTask@ AiMakeTask(CCircuitUnit@ unit)
{
	// TODO: Add API to create any tasks
// 	AiDelPoint(lastPos);
// 	lastPos = unit.GetPos(ai.frame);
// 	AiAddPoint(lastPos, "task");

// 	IUnitTask@ task = aiBuilderMgr.DefaultMakeTask(unit);
// 	if ((task !is null) && (task.GetType() == 5)) {  // Type::BUILDER
// 		switch (task.GetBuildType()) {
// 		case 13:  // BuildType::MEX
// 			AiAddPoint(task.GetBuildPos(), task.GetBuildDef().GetName());
// 			break;
// 		case 7:  // BuildType::DEFENCE
// 			AiAddPoint(task.GetBuildPos(), task.GetBuildDef().GetName());
// 			break;
// 		default:
// 			break;
// 		}
// 	}
// 	return task;
	return aiBuilderMgr.DefaultMakeTask(unit);
}

void AiTaskCreated(IUnitTask@ task)
{
// 	if (task.GetType() != 5) {  // Type::BUILDER
// 		return;
// 	}
// 	switch (task.GetBuildType()) {
// 	case 4: {  // BuildType::ENERGY
// 		if (gPauseCnt == 0) {
// 			string name = task.GetBuildDef().GetName();
// 			if ((name == "armfus") || (name == "armafus") || (name == "corfus") || (name == "corafus")) {
// 				AiPause(true, "energy");
// 				++gPauseCnt;
// 			}
// 			AiAddPoint(task.GetBuildPos(), name);
// 		}
// 	} break;
// 	case 0:  // BuildType::FACTORY
// 	case 1:  // BuildType::NANO
// 	case 2:  // BuildType::STORE
// 	case 3:  // BuildType::PYLON
// 	case 5:  // BuildType::GEO
// 	case 6:  // BuildType::GEOUP
// 	case 7:  // BuildType::DEFENCE
// 	case 8:  // BuildType::BUNKER
// 	case 9:  // BuildType::BIG_GUN
// 	case 10:  // BuildType::RADAR
// 	case 11:  // BuildType::SONAR
// 	case 12:  // BuildType::CONVERT
// 	case 13:  // BuildType::MEX
// 	case 14:  // BuildType::MEXUP
// 		AiAddPoint(task.GetBuildPos(), task.GetBuildDef().GetName());
// 		break;
// 	case 15:  // BuildType::REPAIR
// 		AiAddPoint(task.GetBuildPos(), "rep");
// 		break;
// 	case 16:  // BuildType::RECLAIM
// 		AiAddPoint(task.GetBuildPos(), "rec");
// 		break;
// 	case 17:  // BuildType::RESURRECT
// 		AiAddPoint(task.GetBuildPos(), "res");
// 		break;
// 	case 18:  // BuildType::TERRAFORM
// 		AiAddPoint(task.GetBuildPos(), "ter");
// 		break;
// 	default:
// 		break;
// 	}
}

void AiTaskClosed(IUnitTask@ task, bool done)
{
// 	if (task.GetType() != 5) {  // Type::BUILDER
// 		return;
// 	}
// 	switch (task.GetBuildType()) {
// 	case 0:  // BuildType::FACTORY
// 	case 1:  // BuildType::NANO
// 	case 2:  // BuildType::STORE
// 	case 3:  // BuildType::PYLON
// 	case 4:  // BuildType::ENERGY
// 	case 5:  // BuildType::GEO
// 	case 6:  // BuildType::GEOUP
// 	case 7:  // BuildType::DEFENCE
// 	case 8:  // BuildType::BUNKER
// 	case 9:  // BuildType::BIG_GUN
// 	case 10:  // BuildType::RADAR
// 	case 11:  // BuildType::SONAR
// 	case 12:  // BuildType::CONVERT
// 	case 13:  // BuildType::MEX
// 	case 14:  // BuildType::MEXUP
// 	case 15:  // BuildType::REPAIR
// 	case 16:  // BuildType::RECLAIM
// 	case 17:  // BuildType::RESURRECT
// 	case 18:  // BuildType::TERRAFORM
// 		AiDelPoint(task.GetBuildPos());
// 		break;
// 	default:
// 		break;
// 	}
}

void AiBuilderCreated(CCircuitUnit@ unit)
{
	const CCircuitDef@ cdef = unit.circuitDef;
	if (cdef.IsRoleAny(Unit::Role::COMM.mask))
		return;

	// constructor with BASE attribute is assigned to tasks near base
	if (cdef.costM < 200.f) {
		if (energizer1 is null
			&& (uint(cdef.count) > aiMilitaryMgr.GetGuardTaskNum() || cdef.IsAbleToFly()))
		{
			@energizer1 = @unit;
			unit.AddAttribute(Unit::Attr::BASE.type);
		}
	} else {
		if (energizer2 is null) {
			@energizer2 = @unit;
			unit.AddAttribute(Unit::Attr::BASE.type);
		}
	}
}

void AiBuilderDestroyed(CCircuitUnit@ unit)
{
	if (energizer1 is unit) {
		@energizer1 = null;
	} else if (energizer2 is unit) {
		@energizer2 = null;
	}
}

}  // namespace Builder
