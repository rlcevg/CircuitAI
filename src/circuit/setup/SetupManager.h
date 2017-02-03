/*
 * SetupManager.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_SETUPMANAGER_H_
#define SRC_CIRCUIT_STATIC_SETUPMANAGER_H_

#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "json/json-forwards.h"

#include "AIFloat3.h"

namespace circuit {

namespace setup {
	constexpr char easy[]{"easy"};
	constexpr char normal[]{"normal"};
	constexpr char hard[]{"hard"};
}

class CCircuitAI;
class CSetupData;
class CAllyTeam;

class CSetupManager {
public:
	enum class StartPosType: char {METAL_SPOT = 0, MIDDLE = 1, RANDOM = 2};
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
		} hide;
	};

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

	bool PickCommander();
	CCircuitDef* GetCommChoice() const { return commChoice; }
	void SetCommander(CCircuitUnit* unit) { commanderId = unit->GetId(); }
	CCircuitUnit* GetCommander() const;

	CAllyTeam* GetAllyTeam() const;

	void ReadConfig();
	float GetEmptyShield() const { return emptyShield; }
	float GetFullShield() const { return fullShield; }

	bool HasModules(const CCircuitDef* cdef, unsigned level) const;
	const std::vector<float>& GetModules(const CCircuitDef* cdef, unsigned level) const;
	int GetMorphFrame(const CCircuitDef* cdef) const;
	const std::vector<CCircuitDef::RoleType>* GetOpener(const CCircuitDef* facDef) const;
	const SCommInfo::SHide* GetHide(const CCircuitDef* cdef) const;

	void Welcome() const;

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

	float emptyShield;
	float fullShield;

	CCircuitDef* commChoice;
	std::map<std::string, SCommInfo> commInfos;

	struct SOpener {
		SOpener(float p, const std::vector<CCircuitDef::RoleType>& q) : prob(p), queue(q) {}
		float prob;
		std::vector<CCircuitDef::RoleType> queue;
	};
	struct SStart {
		std::map<CCircuitDef::Id, std::vector<SOpener>> openers;  // fac: openers
		std::vector<CCircuitDef::RoleType> defaultStart;
	};
	std::map<CCircuitDef::Id, SStart> start;  // comm: start
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_SETUPMANAGER_H_
