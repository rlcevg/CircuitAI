#include "manager/military.as"
#include "manager/builder.as"
#include "manager/factory.as"
#include "manager/economy.as"


namespace Main {

void AiMain()
{
	// NOTE: Initialize config params
// 	aiTerrainMgr.SetAllyZoneRange(600);  // returns 576: (multiples of 128) div 2
// 	aiEconomyMgr.reclConvertEff = 2.f;
// 	aiEconomyMgr.reclEnergyEff = 20.f;
//
// 	for (Id defId = 1, count = ai.GetDefCount(); defId <= count; ++defId) {
// 		CCircuitDef@ cdef = ai.GetCircuitDef(defId);
// 		AiLog(cdef.GetName() + " | threat = " + cdef.threat + " | power = " + cdef.power +
// 			" | air = " + cdef.GetAirThreat() + " | surf = " + cdef.GetSurfThreat() + " | water = " + cdef.GetWaterThreat());
// 		cdef.SetThreatKernel((cdef.costM + cdef.costE * 0.02f) * 0.001f);
//
// 		if (ai.teamId == ai.GetLeadTeamId()) {
// 			AiLog("minRange = " + cdef.minRange +
// 				", maxRange = " + cdef.GetMaxRange() +
// 				", maxRange[AIR] = " + cdef.GetMaxRange(Unit::RangeType::AIR) +
// 				", maxRange[SURF] = " + cdef.GetMaxRange(Unit::RangeType::SURF) +
// 				", maxRange[WATER] = " + cdef.GetMaxRange(Unit::RangeType::WATER));
// 			cdef.SetRange(cdef.GetMaxRange() * 0.5f);
// 		}
// 	}

	for (Id defId = 1, count = ai.GetDefCount(); defId <= count; ++defId) {
		CCircuitDef@ cdef = ai.GetCircuitDef(defId);
		if (cdef.costM >= 200.f && !cdef.IsMobile() && aiEconomyMgr.GetEnergyMake(cdef) > 1.f)
			cdef.AddAttribute(Unit::Attr::BASE.type);  // Build heavy energy at base
	}

	// Example of user-assigned custom attributes
	array<string> names = {Factory::armalab, Factory::coralab, Factory::armavp, Factory::coravp,
		Factory::armaap, Factory::coraap, Factory::armasy, Factory::corasy};
	for (uint i = 0; i < names.length(); ++i)
		Factory::userData[ai.GetCircuitDef(names[i]).id].attr |= Factory::Attr::T2;
	names = {Factory::armshltx, Factory::corgant};
	for (uint i = 0; i < names.length(); ++i)
		Factory::userData[ai.GetCircuitDef(names[i]).id].attr |= Factory::Attr::T3;
}

void AiUpdate()  // SlowUpdate, every 30 frames with initial offset of skirmishAIId
{
}

void AiLuaMessage(const string& in data)  // Spring.SendSkirmishAIMessage(teamID, msg) from unsynced lua
{
	if (data.findLast("DISABLE_CONTROL:", 0) == 0)
		UnitControl(data.substr(16), false);
	else if (data.findLast("ENABLE_CONTROL:", 0) == 0)
		UnitControl(data.substr(15), true);
}

void AiMessage(const string& in data, int fromTeamId)  // AiSendMessage(msg, toTeamId = -1)
{
}

void AiUnitFinished(CCircuitUnit@ unit)
{
	// NOTE: Experimental. May be deprecated.
	// Duplicates module's AiUnitAdded, but for any unit.
}

void AiUnitDestroyed(CCircuitUnit@ unit)
{
	// NOTE: Experimental. May be deprecated
	// Duplicates module's AiUnitRemoved, but for any unit.
}

}  // namespace Main


void UnitControl(const string& in idList, bool isEnable)
{
	const array<string>@ ids = idList.split(",");
	for (uint i = 0; i < ids.length(); ++i)
		ai.UnitControl(parseInt(ids[i]), isEnable);
}
