/*
 * Profiler.cpp
 *
 *  Created on: Oct 10, 2023
 *      Author: rlcevg
 */

#include "util/Profiler.h"

namespace circuit {

CProfiler profiler;
int CProfiler::numInit = 0;

decltype(CProfiler::nameEventInit) CProfiler::nameEventInit;
decltype(CProfiler::nameEventRelease) CProfiler::nameEventRelease;
decltype(CProfiler::nameEventUpdate) CProfiler::nameEventUpdate;
decltype(CProfiler::nameEventMessage) CProfiler::nameEventMessage;

decltype(CProfiler::nameEventUnitCreated) CProfiler::nameEventUnitCreated;
decltype(CProfiler::nameEventUnitFinished) CProfiler::nameEventUnitFinished;
decltype(CProfiler::nameEventUnitIdle) CProfiler::nameEventUnitIdle;
decltype(CProfiler::nameEventUnitMoveFailed) CProfiler::nameEventUnitMoveFailed;
decltype(CProfiler::nameEventUnitDamaged) CProfiler::nameEventUnitDamaged;
decltype(CProfiler::nameEventUnitDestroyed) CProfiler::nameEventUnitDestroyed;
decltype(CProfiler::nameEventUnitGiven) CProfiler::nameEventUnitGiven;
decltype(CProfiler::nameEventUnitCaptured) CProfiler::nameEventUnitCaptured;

decltype(CProfiler::nameEventEnemyEnterLOS) CProfiler::nameEventEnemyEnterLOS;
decltype(CProfiler::nameEventEnemyLeaveLOS) CProfiler::nameEventEnemyLeaveLOS;
decltype(CProfiler::nameEventEnemyEnterRadar) CProfiler::nameEventEnemyEnterRadar;
decltype(CProfiler::nameEventEnemyLeaveRadar) CProfiler::nameEventEnemyLeaveRadar;
decltype(CProfiler::nameEventEnemyDamaged) CProfiler::nameEventEnemyDamaged;
decltype(CProfiler::nameEventEnemyDestroyed) CProfiler::nameEventEnemyDestroyed;

decltype(CProfiler::nameEventWeaponFired) CProfiler::nameEventWeaponFired;
decltype(CProfiler::nameEventPlayerCommand) CProfiler::nameEventPlayerCommand;
decltype(CProfiler::nameEventSeismicPing) CProfiler::nameEventSeismicPing;
decltype(CProfiler::nameEventCommandFinished) CProfiler::nameEventCommandFinished;
decltype(CProfiler::nameEventLoad) CProfiler::nameEventLoad;
decltype(CProfiler::nameEventSave) CProfiler::nameEventSave;
decltype(CProfiler::nameEventEnemyCreated) CProfiler::nameEventEnemyCreated;
decltype(CProfiler::nameEventEnemyFinished) CProfiler::nameEventEnemyFinished;
decltype(CProfiler::nameEventLuaMessage) CProfiler::nameEventLuaMessage;

decltype(CProfiler::nameEventReleaseEnd) CProfiler::nameEventReleaseEnd;
decltype(CProfiler::nameEventReleaseResign) CProfiler::nameEventReleaseResign;
decltype(CProfiler::nameEventUpdateResign) CProfiler::nameEventUpdateResign;

decltype(CProfiler::nameThreatUpdate) CProfiler::nameThreatUpdate;
decltype(CProfiler::nameInflUpdate) CProfiler::nameInflUpdate;

void CProfiler::InitNames(int skirmishAIId)
{
	++numInit;
	const size_t numAIs = skirmishAIId + 1;
	if (numAIs > nameEventUpdate.size()) {
		nameEventInit.resize(numAIs);
		nameEventRelease.resize(numAIs);
		nameEventUpdate.resize(numAIs);
		nameEventMessage.resize(numAIs);

		nameEventUnitCreated.resize(numAIs);
		nameEventUnitFinished.resize(numAIs);
		nameEventUnitIdle.resize(numAIs);
		nameEventUnitMoveFailed.resize(numAIs);
		nameEventUnitDamaged.resize(numAIs);
		nameEventUnitDestroyed.resize(numAIs);
		nameEventUnitGiven.resize(numAIs);
		nameEventUnitCaptured.resize(numAIs);

		nameEventEnemyEnterLOS.resize(numAIs);
		nameEventEnemyLeaveLOS.resize(numAIs);
		nameEventEnemyEnterRadar.resize(numAIs);
		nameEventEnemyLeaveRadar.resize(numAIs);
		nameEventEnemyDamaged.resize(numAIs);
		nameEventEnemyDestroyed.resize(numAIs);

		nameEventWeaponFired.resize(numAIs);
		nameEventPlayerCommand.resize(numAIs);
		nameEventSeismicPing.resize(numAIs);
		nameEventCommandFinished.resize(numAIs);
		nameEventLoad.resize(numAIs);
		nameEventSave.resize(numAIs);
		nameEventEnemyCreated.resize(numAIs);
		nameEventEnemyFinished.resize(numAIs);
		nameEventLuaMessage.resize(numAIs);

		nameEventReleaseEnd.resize(numAIs);
		nameEventReleaseResign.resize(numAIs);
		nameEventUpdateResign.resize(numAIs);

		nameThreatUpdate.resize(numAIs);
		nameInflUpdate.resize(numAIs);
	}
	snprintf(nameEventInit[skirmishAIId].data(), nameEventInit[skirmishAIId].size(), strEventInit.data(), (char)skirmishAIId);
	snprintf(nameEventRelease[skirmishAIId].data(), nameEventRelease[skirmishAIId].size(), strEventRelease.data(), (char)skirmishAIId);
	snprintf(nameEventUpdate[skirmishAIId].data(), nameEventUpdate[skirmishAIId].size(), strEventUpdate.data(), (char)skirmishAIId);
	snprintf(nameEventMessage[skirmishAIId].data(), nameEventMessage[skirmishAIId].size(), strEventMessage.data(), (char)skirmishAIId);

	snprintf(nameEventUnitCreated[skirmishAIId].data(), nameEventUnitCreated[skirmishAIId].size(), strEventUnitCreated.data(), (char)skirmishAIId);
	snprintf(nameEventUnitFinished[skirmishAIId].data(), nameEventUnitFinished[skirmishAIId].size(), strEventUnitFinished.data(), (char)skirmishAIId);
	snprintf(nameEventUnitIdle[skirmishAIId].data(), nameEventUnitIdle[skirmishAIId].size(), strEventUnitIdle.data(), (char)skirmishAIId);
	snprintf(nameEventUnitMoveFailed[skirmishAIId].data(), nameEventUnitMoveFailed[skirmishAIId].size(), strEventUnitMoveFailed.data(), (char)skirmishAIId);
	snprintf(nameEventUnitDamaged[skirmishAIId].data(), nameEventUnitDamaged[skirmishAIId].size(), strEventUnitDamaged.data(), (char)skirmishAIId);
	snprintf(nameEventUnitDestroyed[skirmishAIId].data(), nameEventUnitDestroyed[skirmishAIId].size(), strEventUnitDestroyed.data(), (char)skirmishAIId);
	snprintf(nameEventUnitGiven[skirmishAIId].data(), nameEventUnitGiven[skirmishAIId].size(), strEventUnitGiven.data(), (char)skirmishAIId);
	snprintf(nameEventUnitCaptured[skirmishAIId].data(), nameEventUnitCaptured[skirmishAIId].size(), strEventUnitCaptured.data(), (char)skirmishAIId);

	snprintf(nameEventEnemyEnterLOS[skirmishAIId].data(), nameEventEnemyEnterLOS[skirmishAIId].size(), strEventEnemyEnterLOS.data(), (char)skirmishAIId);
	snprintf(nameEventEnemyLeaveLOS[skirmishAIId].data(), nameEventEnemyLeaveLOS[skirmishAIId].size(), strEventEnemyLeaveLOS.data(), (char)skirmishAIId);
	snprintf(nameEventEnemyEnterRadar[skirmishAIId].data(), nameEventEnemyEnterRadar[skirmishAIId].size(), strEventEnemyEnterRadar.data(), (char)skirmishAIId);
	snprintf(nameEventEnemyLeaveRadar[skirmishAIId].data(), nameEventEnemyLeaveRadar[skirmishAIId].size(), strEventEnemyLeaveRadar.data(), (char)skirmishAIId);
	snprintf(nameEventEnemyDamaged[skirmishAIId].data(), nameEventEnemyDamaged[skirmishAIId].size(), strEventEnemyDamaged.data(), (char)skirmishAIId);
	snprintf(nameEventEnemyDestroyed[skirmishAIId].data(), nameEventEnemyDestroyed[skirmishAIId].size(), strEventEnemyDestroyed.data(), (char)skirmishAIId);

	snprintf(nameEventWeaponFired[skirmishAIId].data(), nameEventWeaponFired[skirmishAIId].size(), strEventWeaponFired.data(), (char)skirmishAIId);
	snprintf(nameEventPlayerCommand[skirmishAIId].data(), nameEventPlayerCommand[skirmishAIId].size(), strEventPlayerCommand.data(), (char)skirmishAIId);
	snprintf(nameEventSeismicPing[skirmishAIId].data(), nameEventSeismicPing[skirmishAIId].size(), strEventSeismicPing.data(), (char)skirmishAIId);
	snprintf(nameEventCommandFinished[skirmishAIId].data(), nameEventCommandFinished[skirmishAIId].size(), strEventCommandFinished.data(), (char)skirmishAIId);
	snprintf(nameEventLoad[skirmishAIId].data(), nameEventLoad[skirmishAIId].size(), strEventLoad.data(), (char)skirmishAIId);
	snprintf(nameEventSave[skirmishAIId].data(), nameEventSave[skirmishAIId].size(), strEventSave.data(), (char)skirmishAIId);
	snprintf(nameEventEnemyCreated[skirmishAIId].data(), nameEventEnemyCreated[skirmishAIId].size(), strEventEnemyCreated.data(), (char)skirmishAIId);
	snprintf(nameEventEnemyFinished[skirmishAIId].data(), nameEventEnemyFinished[skirmishAIId].size(), strEventEnemyFinished.data(), (char)skirmishAIId);
	snprintf(nameEventLuaMessage[skirmishAIId].data(), nameEventLuaMessage[skirmishAIId].size(), strEventLuaMessage.data(), (char)skirmishAIId);

	snprintf(nameEventReleaseEnd[skirmishAIId].data(), nameEventReleaseEnd[skirmishAIId].size(), strEventReleaseEnd.data(), (char)skirmishAIId);
	snprintf(nameEventReleaseResign[skirmishAIId].data(), nameEventReleaseResign[skirmishAIId].size(), strEventReleaseResign.data(), (char)skirmishAIId);
	snprintf(nameEventUpdateResign[skirmishAIId].data(), nameEventUpdateResign[skirmishAIId].size(), strEventUpdateResign.data(), (char)skirmishAIId);

	snprintf(nameThreatUpdate[skirmishAIId].data(), nameThreatUpdate[skirmishAIId].size(), strThreatUpdate.data(), (char)skirmishAIId);
	snprintf(nameInflUpdate[skirmishAIId].data(), nameInflUpdate[skirmishAIId].size(), strInflUpdate.data(), (char)skirmishAIId);
}

void CProfiler::ReleaseNames(int skirmishAIId)
{
	if (--numInit > 0) {
		return;
	}
	decltype(nameEventInit)().swap(nameEventInit);
	decltype(nameEventRelease)().swap(nameEventRelease);
	decltype(nameEventUpdate)().swap(nameEventUpdate);
	decltype(nameEventMessage)().swap(nameEventMessage);

	decltype(nameEventUnitCreated)().swap(nameEventUnitCreated);
	decltype(nameEventUnitFinished)().swap(nameEventUnitFinished);
	decltype(nameEventUnitIdle)().swap(nameEventUnitIdle);
	decltype(nameEventUnitMoveFailed)().swap(nameEventUnitMoveFailed);
	decltype(nameEventUnitDamaged)().swap(nameEventUnitDamaged);
	decltype(nameEventUnitDestroyed)().swap(nameEventUnitDestroyed);
	decltype(nameEventUnitGiven)().swap(nameEventUnitGiven);
	decltype(nameEventUnitCaptured)().swap(nameEventUnitCaptured);

	decltype(nameEventEnemyEnterLOS)().swap(nameEventEnemyEnterLOS);
	decltype(nameEventEnemyLeaveLOS)().swap(nameEventEnemyLeaveLOS);
	decltype(nameEventEnemyEnterRadar)().swap(nameEventEnemyEnterRadar);
	decltype(nameEventEnemyLeaveRadar)().swap(nameEventEnemyLeaveRadar);
	decltype(nameEventEnemyDamaged)().swap(nameEventEnemyDamaged);
	decltype(nameEventEnemyDestroyed)().swap(nameEventEnemyDestroyed);

	decltype(nameEventWeaponFired)().swap(nameEventWeaponFired);
	decltype(nameEventPlayerCommand)().swap(nameEventPlayerCommand);
	decltype(nameEventSeismicPing)().swap(nameEventSeismicPing);
	decltype(nameEventCommandFinished)().swap(nameEventCommandFinished);
	decltype(nameEventLoad)().swap(nameEventLoad);
	decltype(nameEventSave)().swap(nameEventSave);
	decltype(nameEventEnemyCreated)().swap(nameEventEnemyCreated);
	decltype(nameEventEnemyFinished)().swap(nameEventEnemyFinished);
	decltype(nameEventLuaMessage)().swap(nameEventLuaMessage);

	decltype(nameEventReleaseEnd)().swap(nameEventReleaseEnd);
	decltype(nameEventReleaseResign)().swap(nameEventReleaseResign);
	decltype(nameEventUpdateResign)().swap(nameEventUpdateResign);

	decltype(nameThreatUpdate)().swap(nameThreatUpdate);
	decltype(nameInflUpdate)().swap(nameInflUpdate);
}

} /* namespace circuit */
