/*
 * Circuit.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include "ExternalAI/Interface/AISEvents.h"
#include "ExternalAI/Interface/AISCommands.h"

#include "OOAICallback.h"			// C++ wrapper
#include "SSkirmishAICallback.h"	// "direct" C API

#include <memory>
#include <map>
#include <vector>

namespace circuit {

#define ERROR_UNKNOWN		200
#define ERROR_INIT			(ERROR_UNKNOWN + EVENT_INIT)
#define ERROR_RELEASE		(ERROR_UNKNOWN + EVENT_RELEASE)
#define ERROR_UPDATE		(ERROR_UNKNOWN + EVENT_UPDATE)
#define LOG(fmt, ...)	GetLog()->DoLog(utils::string_format(std::string(fmt), ##__VA_ARGS__).c_str())

class CGameAttribute;
class CScheduler;
class CCircuitUnit;
class IModule;
struct Metal;

class CCircuit {
public:
	CCircuit(springai::OOAICallback* callback);
	virtual ~CCircuit();

	int Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback);
	int Release(int reason);
	int Update(int frame);
	int Message(int playerId, const char* message);
	int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	int UnitFinished(CCircuitUnit* unit);
	int LuaMessage(const char* inData);

	CCircuitUnit* RegisterUnit(int unitId);
	CCircuitUnit* GetUnitById(int unitId);

	CGameAttribute* GetGameAttribute();
	CScheduler* GetScheduler();
	int GetLastFrame();
	int GetSkirmishAIId();
	int GetTeamId();
	int GetAllyTeamId();
	springai::OOAICallback* GetCallback();
	springai::Log*          GetLog();
	springai::Game*         GetGame();
	springai::Map*          GetMap();
	springai::Pathing*      GetPathing();
	springai::Drawer*       GetDrawer();
	springai::SkirmishAI*   GetSkirmishAI();

private:
	bool initialized;
	int lastFrame;
	int skirmishAIId;
	int teamId;
	int allyTeamId;
	// TODO: move these into gameAttribute?
	springai::OOAICallback*               callback;
	std::unique_ptr<springai::Log>        log;
	std::unique_ptr<springai::Game>       game;
	std::unique_ptr<springai::Map>        map;
	std::unique_ptr<springai::Pathing>    pathing;
	std::unique_ptr<springai::Drawer>     drawer;
	std::unique_ptr<springai::SkirmishAI> skirmishAI;

	static std::unique_ptr<CGameAttribute> gameAttribute;
	static unsigned int gaCounter;

	static void CreateGameAttribute();
	static void DestroyGameAttribute();

	std::shared_ptr<CScheduler> scheduler;
	std::vector<std::unique_ptr<IModule>> modules;

	std::map<int, CCircuitUnit*> aliveUnits;
	std::vector<CCircuitUnit*>   teamUnits;
	std::vector<CCircuitUnit*>   friendlyUnits;
	std::vector<CCircuitUnit*>   enemyUnits;

	void ClusterizeMetal();
	// debug
	void DrawClusters();
};

} // namespace circuit

#endif // CIRCUIT_H_
