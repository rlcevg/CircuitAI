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
#include <vector>
#include <unordered_map>
#include <string.h>

namespace springai {
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
#define ERROR_UNIT_CREATED		(ERROR_UNKNOWN + EVENT_UNIT_CREATED)
#define ERROR_UNIT_FINISHED		(ERROR_UNKNOWN + EVENT_UNIT_FINISHED)
#define ERROR_UNIT_IDLE			(ERROR_UNKNOWN + EVENT_UNIT_IDLE)
#define ERROR_UNIT_MOVE_FAILED	(ERROR_UNKNOWN + EVENT_UNIT_MOVE_FAILED)
#define ERROR_UNIT_DAMAGED		(ERROR_UNKNOWN + EVENT_UNIT_DAMAGED)
#define ERROR_UNIT_DESTROYED	(ERROR_UNKNOWN + EVENT_UNIT_DESTROYED)
#define ERROR_UNIT_GIVEN		(ERROR_UNKNOWN + EVENT_UNIT_GIVEN)
#define ERROR_UNIT_CAPTURED		(ERROR_UNKNOWN + EVENT_UNIT_CAPTURED)
#define ERROR_ENEMY_ENTER_LOS	(ERROR_UNKNOWN + EVENT_ENEMY_ENTER_LOS)
#define ERROR_ENEMY_DESTROYED	(ERROR_UNKNOWN + EVENT_ENEMY_DESTROYED)
#define LOG(fmt, ...)	GetLog()->DoLog(utils::string_format(std::string(fmt), ##__VA_ARGS__).c_str())

class CGameAttribute;
class CSetupManager;
class CMetalManager;
class CTerrainAnalyzer;
class CScheduler;
class CCircuitUnit;
class IModule;
struct Metal;

class CCircuitAI {
public:
	CCircuitAI(springai::OOAICallback* callback);
	virtual ~CCircuitAI();

// ---- AI Event handler ---- BEGIN
public:
	int HandleEvent(int topic, const void* data);
	void NotifyGameEnd();
private:
	typedef int (CCircuitAI::*EventHandlerPtr)(int topic, const void* data);
	int HandleGameEvent(int topic, const void* data);
	int HandleEndEvent(int topic, const void* data);
	EventHandlerPtr eventHandler;
// ---- AI Event handler ---- END

public:
	int Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback);
	int Release(int reason);
	int Update(int frame);
	int Message(int playerId, const char* message);
	int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	int UnitFinished(CCircuitUnit* unit);
	int UnitIdle(CCircuitUnit* unit);
	int UnitMoveFailed(CCircuitUnit* unit);
	int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);  // TODO: Use Team class?
	int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);  // TODO: Use Team class?
	int EnemyEnterLOS(CCircuitUnit* unit);
	int PlayerCommand(std::vector<CCircuitUnit*>& units);
//	int CommandFinished(CCircuitUnit* unit, int commandTopicId);
	int LuaMessage(const char* inData);

	CCircuitUnit* GetUnitById(int unitId);
	CCircuitUnit* RegisterUnit(int unitId);
	void UnregisterUnit(CCircuitUnit* unit);

// ---- UnitDefs ---- BEGIN
private:
	struct cmp_str {
	   bool operator()(char const *a, char const *b) {
	      return strcmp(a, b) < 0;
	   }
	};
public:
	using UnitDefs = std::map<const char*, springai::UnitDef*, cmp_str>;

	void InitUnitDefs(std::vector<springai::UnitDef*>&& unitDefs);
	springai::UnitDef* GetUnitDefByName(const char* name);
	springai::UnitDef* GetUnitDefById(int unitDefId);
	UnitDefs& GetUnitDefs();
	int GetUnitCount(springai::UnitDef* unitDef);
	bool IsAvailable(springai::UnitDef* unitDef);
private:
	UnitDefs defsByName;  // owner
	std::unordered_map<int, springai::UnitDef*> defsById;
	std::unordered_map<springai::UnitDef*, int> unitCounts;
// ---- UnitDefs ---- END

public:
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
	CSetupManager* GetSetupManager();
	CMetalManager* GetMetalManager();
	CTerrainAnalyzer* GetTerrainAnalyzer();

private:
	// debug
//	void DrawClusters();

	bool initialized;
	int lastFrame;
	int skirmishAIId;
	int teamId;
	int allyTeamId;
	springai::OOAICallback*               callback;
	std::unique_ptr<springai::Log>        log;
	std::unique_ptr<springai::Game>       game;
	std::unique_ptr<springai::Map>        map;
	std::unique_ptr<springai::Pathing>    pathing;
	std::unique_ptr<springai::Drawer>     drawer;
	std::unique_ptr<springai::SkirmishAI> skirmishAI;

	static std::unique_ptr<CGameAttribute> gameAttribute;
	static unsigned int gaCounter;
	void CreateGameAttribute();
	void DestroyGameAttribute();
	std::shared_ptr<CScheduler> scheduler;
	std::unique_ptr<CSetupManager> setupManager;
	std::unique_ptr<CMetalManager> metalManager;
	std::unique_ptr<CTerrainAnalyzer> terrainAnalyzer;
	std::list<IModule*> modules;

	// TODO: Make global storage?
	std::map<int, CCircuitUnit*> aliveUnits;  // owner
	// TODO: Use or delete
	std::map<int, CCircuitUnit*> teamUnits;  // owner

	// TODO: Use or delete
	std::map<int, CCircuitUnit*> friendlyUnits;  // owner
	// TODO: Use or delete
	std::map<int, CCircuitUnit*> enemyUnits;  // owner
};

} // namespace circuit

#endif // CIRCUIT_H_
