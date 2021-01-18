#include "../../define.as"


namespace Economy {

void AiOpenStrategy(const CCircuitDef@ facDef, const AIFloat3& in pos)
{
}

/*
 * struct SResourceInfo {
 *   const float current;
 *   const float storage;
 *   const float pull;
 *   const float income;
 * }
 */
void AiUpdateEconomy()
{
	const SResourceInfo@ metal = aiEconomyMgr.metal;
	const SResourceInfo@ energy = aiEconomyMgr.energy;
	aiEconomyMgr.isMetalEmpty = metal.current < metal.storage * 0.2f;
	aiEconomyMgr.isMetalFull = metal.current > metal.storage * 0.8f;
	aiEconomyMgr.isEnergyEmpty = energy.current < energy.storage * 0.2f;
//	aiEconomyMgr.isEnergyStalling = aiMin(metal.income - metal.pull, .0f)/* * 0.98f*/ > aiMin(energy.income - energy.pull, .0f);
	const float percent = (ai.frame < 3 * MINUTE) ? 0.2f : 0.6f;
	aiEconomyMgr.isEnergyStalling = aiEconomyMgr.isEnergyEmpty || ((energy.income < energy.pull) && (energy.current < energy.storage * percent));
}

}  // namespace Economy
