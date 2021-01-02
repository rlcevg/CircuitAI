#include "../role.as"


namespace Builder {

IUnitTask@ MakeTask(CCircuitUnit@ unit)
{
	return aiBuilderMgr.DefaultMakeTask(unit);
}

}  // namespace Builder
