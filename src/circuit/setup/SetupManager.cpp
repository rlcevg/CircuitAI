/*
 * SetupManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "setup/SetupManager.h"
#include "setup/SetupData.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "Game.h"
#include "Map.h"

#include <map>
#include <regex>

namespace circuit {

using namespace springai;

CSetupManager::CSetupManager(CCircuitAI* circuit, CSetupData* setupData) :
		circuit(circuit),
		setupData(setupData),
		commanderId(-1),
		startPos(-RgtVector),
		basePos(-RgtVector)
{
	if (!setupData->IsInitialized()) {
		ParseSetupScript(circuit->GetGame()->GetSetupScript());
	}
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CSetupManager::FindCommander, this));
}

CSetupManager::~CSetupManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CSetupManager::ParseSetupScript(const char* setupScript)
{
	std::string script(setupScript);
	std::map<int, int> teamIdsRemap;
	using OrigTeamIds = std::set<int>;
	std::map<int, OrigTeamIds> allies;
	CSetupData::BoxMap boxes;

	// Detect start boxes
	Map* map = circuit->GetMap();
	float width = map->GetWidth() * SQUARE_SIZE;
	float height = map->GetHeight() * SQUARE_SIZE;

	std::string::const_iterator start = script.begin();
	std::string::const_iterator end = script.end();
	std::regex patternBox("startboxes=(.*);");
	std::smatch section;
	bool isZkBox = std::regex_search(start, end, section, patternBox);
	if (isZkBox) {
		// zk way
		// startboxes=return { [0] = { 0, 0, 0.25, 1 }, [1] = { 0.75, 0, 1, 1 }, };
		// @see Zero-K.sdd/LuaRules/Gadgets/start_boxes.lua
		std::string lua_str = section[1];
		start = lua_str.begin();
		end = lua_str.end();
		std::regex patternAlly("\\[(\\d+)\\][^\\{]*\\{[ ,]*(\\d+\\.?\\d*)[ ,]*(\\d+\\.?\\d*)[ ,]*(\\d+\\.?\\d*)[ ,]*(\\d+\\.?\\d*)[^\\}]\\}");
		while (std::regex_search(start, end, section, patternAlly)) {
			int allyTeamId = utils::string_to_int(section[1]);

			CAllyTeam::SBox startbox;
			startbox.left   = utils::string_to_float(section[2]) * width;
			startbox.top    = utils::string_to_float(section[3]) * height;
			startbox.right  = utils::string_to_float(section[4]) * width;
			startbox.bottom = utils::string_to_float(section[5]) * height;
			boxes[allyTeamId] = startbox;

			start = section[0].second;
		}
	} else {
		// engine way
		std::regex patternAlly("\\[allyteam(\\d+)\\]\\s*\\{([^\\}]*)\\}");
		std::regex patternRect("startrect\\w+=(\\d+(\\.\\d+)?);");
		while (std::regex_search(start, end, section, patternAlly)) {
			int allyTeamId = utils::string_to_int(section[1]);

			std::string allyBody = section[2];
			std::sregex_token_iterator iter(allyBody.begin(), allyBody.end(), patternRect, 1);
			std::sregex_token_iterator end;
			CAllyTeam::SBox startbox;
			for (int i = 0; iter != end && i < 4; ++iter, i++) {
				startbox.edge[i] = utils::string_to_float(*iter);
			}

			startbox.bottom *= height;
			startbox.left   *= width;
			startbox.right  *= width;
			startbox.top    *= height;
			boxes[allyTeamId] = startbox;

			start = section[0].second;
		}
	}

	// Detect start position type
	CGameSetup::StartPosType startPosType;
	std::cmatch matchPosType;
	std::regex patternPosType("startpostype=(\\d+)");
	if (std::regex_search(setupScript, matchPosType, patternPosType)) {
		startPosType = static_cast<CGameSetup::StartPosType>(std::atoi(matchPosType[1].first));
	} else {
		startPosType = CGameSetup::StartPosType::StartPos_Fixed;
	}

	// Detect team alliances
	std::regex patternTeam("\\[team(\\d+)\\]\\s*\\{([^\\}]*)\\}");
	std::regex patternAllyId("allyteam=(\\d+);");
	start = script.begin();
	end = script.end();
	while (std::regex_search(start, end, section, patternTeam)) {
		int teamId = utils::string_to_int(section[1]);
		teamIdsRemap[teamId] = teamId;

		std::string teamBody = section[2];
		std::smatch matchAllyId;
		if (std::regex_search(teamBody, matchAllyId, patternAllyId)) {
			int allyTeamId = utils::string_to_int(matchAllyId[1]);
			allies[allyTeamId].insert(teamId);
		}

		start = section[0].second;
	}
	// Make team remapper
	int i = 0;
	for (auto& kv : teamIdsRemap) {
		kv.second = i++;
	}

	// Remap teams, create ally-teams
	// @see rts/Game/GameSetup.cpp CGameSetup::Init
	std::vector<CAllyTeam*> allyTeams;
	allyTeams.reserve(allies.size());
	for (const auto& kv : allies) {
		const OrigTeamIds& data = kv.second;

		CAllyTeam::TeamIds teamIds;
		teamIds.reserve(data.size());
		for (auto id : data) {
			teamIds.insert(teamIdsRemap[id]);
		}
		allyTeams.push_back(new CAllyTeam(teamIds, isZkBox ? boxes[0] : boxes[kv.first]));
	}

	setupData->Init(allyTeams, boxes, startPosType);
}

bool CSetupManager::HasStartBoxes() const
{
	return setupData->IsInitialized();
}

bool CSetupManager::CanChooseStartPos() const
{
	return setupData->CanChooseStartPos();
}

void CSetupManager::PickStartPos(CCircuitAI* circuit, StartPosType type)
{
	float x, z;
	const CAllyTeam::SBox& box = circuit->GetAllyTeam()->GetStartBox();

	auto random = [](const CAllyTeam::SBox& box, float& x, float& z) {
		int min, max;
		min = box.left;
		max = box.right;
		x = min + (rand() % (int)(max - min + 1));
		min = box.top;
		max = box.bottom;
		z = min + (rand() % (int)(max - min + 1));
	};

	switch (type) {
		case StartPosType::METAL_SPOT: {
			AIFloat3 posFrom(box.left, 0, box.top);
			AIFloat3 posTo(box.right, 0, box.bottom);
			CMetalManager* metalManager = circuit->GetMetalManager();
			CMetalData::MetalIndices inBoxIndices = metalManager->FindWithinRangeSpots(posFrom, posTo);
			if (!inBoxIndices.empty()) {
				const CMetalData::Metals& spots = metalManager->GetSpots();
				CTerrainManager* terrainManager = circuit->GetTerrainManager();
				STerrainMapMobileType* mobileType = terrainManager->GetMobileTypeById(circuit->GetCircuitDef("armrectr")->GetMobileId());
				std::vector<int> filteredIndices;
				for (auto idx : inBoxIndices) {
					int iS = terrainManager->GetSectorIndex(spots[idx].position);
					STerrainMapArea* area = mobileType->sector[iS].area;
					if ((area != nullptr) && area->areaUsable) {
						filteredIndices.push_back(idx);
					}
				}
				if (!filteredIndices.empty()) {
					const AIFloat3& pos = spots[filteredIndices[rand() % filteredIndices.size()]].position;
					x = pos.x;
					z = pos.z;
					break;
				}
			}
			random(box, x, z);
			break;
		}
		case StartPosType::MIDDLE: {
			x = (box.left + box.right) / 2;
			z = (box.top + box.bottom) / 2;
			break;
		}
		case StartPosType::RANDOM:
		default: {
			random(box, x, z);
			break;
		}
	}

	AIFloat3 pos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	SetStartPos(pos);
	circuit->GetGame()->SendStartPosition(false, pos);
}

CCircuitUnit* CSetupManager::GetCommander() const
{
	return circuit->GetTeamUnit(commanderId);
}

CAllyTeam* CSetupManager::GetAllyTeam() const
{
	return setupData->GetAllyTeam(circuit->GetAllyTeamId());
}

void CSetupManager::FindCommander()
{
	std::vector<Unit*> units = circuit->GetCallback()->GetTeamUnits();
	for (auto unit : units) {
		UnitDef* def = unit->GetDef();
		bool valid = def->IsBuilder();
		delete def;
		if (valid) {
			commanderId = unit->GetUnitId();
//			if (startPos == -RgtVector) {
				SetStartPos(unit->GetPos());
//			}
			break;
		}
	}
	utils::free_clear(units);
}

} // namespace circuit
