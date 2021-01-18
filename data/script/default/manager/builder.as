#include "../role.as"


namespace Builder {

IUnitTask@ AiMakeTask(CCircuitUnit@ unit)
{
	return aiBuilderMgr.DefaultMakeTask(unit);
}

void AiTaskCreated(IUnitTask@ task)
{
}

void AiTaskClosed(IUnitTask@ task, bool done)
{
}

}  // namespace Builder
