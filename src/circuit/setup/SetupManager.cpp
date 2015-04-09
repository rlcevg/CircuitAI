/*
 * SetupManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "setup/SetupManager.h"
#include "static/SetupData.h"
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
		startPos(-RgtVector)
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
	struct SAllyData {
		CAllyTeam::SBox startBox;
		std::set<int> origTeamIds;
	};
	std::map<int, SAllyData> alliesMap;

	// Detect start boxes
	Map* map = circuit->GetMap();
	float width = map->GetWidth() * SQUARE_SIZE;
	float height = map->GetHeight() * SQUARE_SIZE;
	std::regex patternAlly("\\[allyteam(\\d+)\\]\\s*\\{([^\\}]*)\\}");
	std::regex patternRect("startrect\\w+=(\\d+(\\.\\d+)?);");
	std::smatch section;
	std::string::const_iterator start = script.begin();
	std::string::const_iterator end = script.end();
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
		alliesMap[allyTeamId].startBox = startbox;

		start = section[0].second;
	}

	// Detect start position type
	CGameSetup::StartPosType startPosType;
	std::cmatch matchPosType;
	std::regex patternPosType("startpostype=(\\d+)");
	if (std::regex_search(setupScript, matchPosType, patternPosType)) {
		startPosType = static_cast<CGameSetup::StartPosType>(std::atoi(matchPosType[1].first));
	} else {
		startPosType = CGameSetup::StartPosType::StartPos_ChooseInGame;
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
			alliesMap[allyTeamId].origTeamIds.insert(teamId);
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
	allyTeams.reserve(alliesMap.size());
	for (const auto& kv : alliesMap) {
		const SAllyData& data = kv.second;
		std::vector<int> teamIds;
		teamIds.reserve(data.origTeamIds.size());
		for (auto id : data.origTeamIds) {
			teamIds.push_back(teamIdsRemap[id]);
		}
		allyTeams.push_back(new CAllyTeam(teamIds, data.startBox));
	}

	setupData->Init(allyTeams, startPosType);
}

bool CSetupManager::HasStartBoxes()
{
	return setupData->IsInitialized();
}

bool CSetupManager::CanChooseStartPos()
{
	return setupData->CanChooseStartPos();
}

void CSetupManager::PickStartPos(CCircuitAI* circuit, StartPosType type)
{
	float x, z;
	const CAllyTeam::SBox& box = (*setupData)[circuit->GetAllyTeamId()];

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
				CTerrainManager* terrain = circuit->GetTerrainManager();
				STerrainMapMobileType* mobileType = terrain->GetMobileTypeById(circuit->GetCircuitDef("armcom1")->GetMobileId());
				std::vector<int> filteredIndices;
				for (auto idx : inBoxIndices) {
					int iS = terrain->GetSectorIndex(spots[idx].position);
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

void CSetupManager::SetStartPos(const AIFloat3& pos)
{
	startPos = basePos = pos;
}

const AIFloat3& CSetupManager::GetStartPos()
{
	return startPos;
}

void CSetupManager::SetBasePos(const springai::AIFloat3& pos)
{
	basePos = pos;
}

const springai::AIFloat3& CSetupManager::GetBasePos()
{
	return basePos;
}

CCircuitUnit* CSetupManager::GetCommander()
{
	return circuit->GetTeamUnitById(commanderId);
}

CAllyTeam* CSetupManager::GetAllyTeam()
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
			if (startPos == -RgtVector) {
				startPos = unit->GetPos();
			}
			break;
		}
	}
	utils::free_clear(units);
}

} // namespace circuit
