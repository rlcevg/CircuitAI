#include "../role.as"


namespace Builder {

IUnitTask@ AiMakeTask(CCircuitUnit@ unit)
{
	return aiBuilderMgr.DefaultMakeTask(unit);
}

}  // namespace Builder
