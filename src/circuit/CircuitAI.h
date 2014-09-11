/*
 * Circuit.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include <memory>
#include <map>
#include <list>

namespace springai {
	class AIFloat3;
	class OOAICallback;
	class Log;
	class Game;
	class Map;
	class Pathing;
	class Drawer;
	class SkirmishAI;
	class UnitDef;
}
struct SSkirmishAICallback;

namespace circuit {

#define ERROR_UNKNOWN			200
#define ERROR_INIT				(ERROR_UNKNOWN + EVENT_INIT)
#define ERROR_RELEASE			(ERROR_UNKNOWN + EVENT_RELEASE)
#define ERROR_UPDATE			(ERROR_UNKNOWN + EVENT_UPDATE)
#define ERROR_UNIT_DESTROYED	(ERROR_UNKNOWN + EVENT_UNIT_DESTROYED)
#define LOG(fmt, ...)	GetLog()->DoLog(utils::string_format(std::string(fmt), ##__VA_ARGS__).c_str())

class CGameAttribute;
class CScheduler;
class CCircuitUnit;
class IModule;
struct Metal;

class CCircuitAI {
public:
	CCircuitAI(springai::OOAICallback* callback);
	virtual ~CCircuitAI();

	int HandleEvent(int topic, const void* data);

	int Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback);
	int Release(int reason);
	int Update(int frame);
	int Message(int playerId, const char* message);
	int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	int UnitFinished(CCircuitUnit* unit);
	int UnitIdle(CCircuitUnit* unit);
	int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);  // TODO: Use Team class?
	int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);  // TODO: Use Team class?
	int LuaMessage(const char* inData);

	CCircuitUnit* GetUnitById(int unitId);
	CCircuitUnit* RegisterUnit(int unitId);
	void UnregisterUnit(int unitId);

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

	springai::AIFloat3 FindBuildSiteMindMex(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing);

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
	std::list<std::unique_ptr<IModule>> modules;

	std::map<int, CCircuitUnit*> aliveUnits;  // owner
	std::map<int, CCircuitUnit*> teamUnits;  // owner
	std::map<int, CCircuitUnit*> friendlyUnits;  // owner
	std::map<int, CCircuitUnit*> enemyUnits;  // owner

	void ClusterizeMetal();
	// debug
	void DrawClusters();
};

} // namespace circuit

#endif // CIRCUIT_H_
