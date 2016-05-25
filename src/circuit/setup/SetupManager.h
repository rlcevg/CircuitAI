/*
 * SetupManager.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_SETUPMANAGER_H_
#define SRC_CIRCUIT_STATIC_SETUPMANAGER_H_

#include "unit/CircuitUnit.h"
#include "json/json-forwards.h"

#include "AIFloat3.h"

namespace circuit {

namespace setup {
	static constexpr char easy[]{"easy"};
	static constexpr char normal[]{"normal"};
	static constexpr char hard[]{"hard"};
}

class CCircuitAI;
class CSetupData;
class CAllyTeam;

class CSetupManager {
public:
	enum class StartPosType: char {METAL_SPOT = 0, MIDDLE = 1, RANDOM = 2};

	CSetupManager(CCircuitAI* circuit, CSetupData* setupData);
	virtual ~CSetupManager();
	void DisabledUnits(const char* setupScript);

	bool OpenConfig(const std::string& cfgName);
	void CloseConfig();
	const Json::Value& GetConfig() const { return *config; }
	const std::string& GetConfigName() const { return configName; }

	bool HasStartBoxes() const;
	bool CanChooseStartPos() const;

	void PickStartPos(CCircuitAI* circuit, StartPosType type);
	void SetStartPos(const springai::AIFloat3& pos) { startPos = basePos = pos; }
	const springai::AIFloat3& GetStartPos() const { return startPos; }
	void SetBasePos(const springai::AIFloat3& pos) { basePos = pos; }
	const springai::AIFloat3& GetBasePos() const { return basePos; }

	void PickCommander();
	void SetCommander(CCircuitUnit* unit) { commanderId = unit->GetId(); }
	CCircuitUnit* GetCommander() const;

	CAllyTeam* GetAllyTeam() const;

	void ReadConfig();
	float GetRetreatShield() const { return retreatShield; }

private:
	void FindCommander();
	bool LocatePath(std::string& filename);
	const char* ReadConfig(const std::string& filename);
	Json::Value* ParseConfig(const char* cfgJson);

	CCircuitAI* circuit;
	CSetupData* setupData;
	Json::Value* config;  // owner;
	std::string configName;

	CCircuitUnit::Id commanderId;
	springai::AIFloat3 startPos;
	springai::AIFloat3 basePos;

	float retreatShield;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_SETUPMANAGER_H_
