/*
 * SetupManager.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_SETUPMANAGER_H_
#define SRC_CIRCUIT_STATIC_SETUPMANAGER_H_

#include "unit/CircuitDef.h"
#include "json/json-forwards.h"

#include "AIFloat3.h"

#include <functional>

namespace circuit {

class CCircuitAI;
class CSetupData;
class CAllyTeam;
class IMainJob;
class CCircuitUnit;

class CSetupManager {
public:
	friend class CInitScript;

	enum class StartPosType: char {METAL_SPOT = 0, RANDOM = 1};
	// TODO: class CCommander;
	struct SCommInfo {
		struct SMorph {
			std::vector<std::vector<float>> modules;
			int frame;
		} morph;
		struct SHide {
			int frame;
			float threat;
			bool isAir;
			float sqPeaceTaskRad;
			float sqDangerTaskRad;
		} hide;
	};
	using StartFunc = std::function<void (const springai::AIFloat3& pos)>;

	CSetupManager(CCircuitAI* circuit, CSetupData* setupData);
	virtual ~CSetupManager();
	void DisabledUnits(const char* setupScript);

	bool OpenConfig(const std::string& profile, const std::vector<std::string>& parts);
	void CloseConfig();
	const Json::Value& GetConfig() const { return *config; }
	const std::string& GetConfigName() const { return configName; }

	bool HasStartBoxes() const;
	bool CanChooseStartPos() const;

	void PickStartPos(StartPosType type);
	void SetStartPos(const springai::AIFloat3& pos) { startPos = basePos = pos; }
	const springai::AIFloat3& GetStartPos() const { return startPos; }
	void SetBasePos(const springai::AIFloat3& pos);
	const springai::AIFloat3& GetBasePos() const { return basePos; }
	void SetLanePos(const springai::AIFloat3& pos) { lanePos = pos; }
	const springai::AIFloat3& GetLanePos() const { return lanePos; }
	const springai::AIFloat3& GetMetalBase() const { return metalBase; }
	const springai::AIFloat3& GetEnergyBase() const { return energyBase; }
	const springai::AIFloat3& GetEnergyBase2() const { return energyBase2; }
	const springai::AIFloat3& GetSmallEnergyPos() const { return smallEnergyPos; }
	void FindNewBase(CCircuitUnit* unit);
	void ExecOnFindStart(StartFunc& func) { startFuncs.push_back(func); }

	bool PickCommander();
	CCircuitDef* GetCommChoice() const { return commChoice; }
	void SetCommander(CCircuitUnit* unit);
	CCircuitUnit* GetCommander() const { return commander; }

	CAllyTeam* GetAllyTeam() const;

	void ReadConfig();
	float GetEmptyShield() const { return emptyShield; }
	float GetFullShield() const { return fullShield; }
	int GetAssistFac() const { return assistFac; }

	bool HasModules(const CCircuitDef* cdef, unsigned level) const;
	const std::vector<float>& GetModules(const CCircuitDef* cdef, unsigned level) const;
	int GetMorphFrame(const CCircuitDef* cdef) const;
	const SCommInfo::SHide* GetHide(const CCircuitDef* cdef) const;

	void Welcome() const;

private:
	void FindStart();
	void CalcStartPos();
	void CalcLanePos();
	springai::AIFloat3 MakeStartPosOffset(const springai::AIFloat3& pos, int clusterId, float range);
	bool LoadConfig(const std::string& profile, const std::vector<std::string>& parts);
	Json::Value* ReadConfig(const std::string& dirName, const std::string& profile,
							const std::vector<std::string>& parts, const bool isVFS);
	Json::Value* ParseConfig(const std::string& cfgStr, const std::string& cfgName, Json::Value* cfg = nullptr);
	void UpdateJson(Json::Value& a, Json::Value& b);
	void OverrideConfig();

	CCircuitAI* circuit;
	CSetupData* setupData;
	Json::Value* config;  // owner;
	std::string configName;

	CCircuitUnit* commander;
	springai::AIFloat3 startPos;
	springai::AIFloat3 basePos;
	springai::AIFloat3 lanePos;
	springai::AIFloat3 metalBase;
	springai::AIFloat3 energyBase;
	springai::AIFloat3 energyBase2;
	springai::AIFloat3 smallEnergyPos;
	std::shared_ptr<IMainJob> findStart;
	std::vector<StartFunc> startFuncs;

	float emptyShield;
	float fullShield;
	int assistFac;

	std::string commPrefix;
	std::string commSuffix;
	CCircuitDef* commChoice;
	std::map<std::string, SCommInfo> commInfos;
	bool isSideSelected;  // FIXME: Random-Side workaround

	std::map<CCircuitDef::Id, std::string> sides;  // comm: side
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_SETUPMANAGER_H_
