/*
 * SetupManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "setup/SetupManager.h"
#include "static/SetupData.h"
#include "static/TerrainData.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitDef.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
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
		Map* map = circuit->GetMap();
		int terrainWidth = map->GetWidth() * SQUARE_SIZE;
		int terrainHeight = map->GetHeight() * SQUARE_SIZE;
		ParseSetupScript(circuit->GetGame()->GetSetupScript(), terrainWidth, terrainHeight);
	}
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CSetupManager::FindCommander, this));
}

CSetupManager::~CSetupManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CSetupManager::ParseSetupScript(const char* setupScript, float width, float height)
{
	std::map<int, CSetupData::Box> boxesMap;
	std::regex patternAlly("\\[allyteam(\\d+)\\]\\s*\\{([^\\}]*)\\}");
	std::regex patternRect("startrect\\w+=(\\d+(\\.\\d+)?);");
	std::string script(setupScript);

	std::smatch allyteam;
	std::string::const_iterator start = script.begin();
	std::string::const_iterator end = script.end();
	while (std::regex_search(start, end, allyteam, patternAlly)) {
		int allyTeamId = utils::string_to_int(allyteam[1]);

		std::string teamDefBody = allyteam[2];
		std::sregex_token_iterator iter(teamDefBody.begin(), teamDefBody.end(), patternRect, 1);
		std::sregex_token_iterator end;
		CSetupData::Box startbox;
		for (int i = 0; iter != end && i < 4; ++iter, i++) {
			startbox.edge[i] = utils::string_to_float(*iter);
		}

		startbox.bottom *= height;
		startbox.left   *= width;
		startbox.right  *= width;
		startbox.top    *= height;
		boxesMap[allyTeamId] = startbox;

		start = allyteam[0].second;
	}

	CGameSetup::StartPosType startPosType;
	std::cmatch matchPosType;
	std::regex patternPosType("startpostype=(\\d+)");
	if (std::regex_search(setupScript, matchPosType, patternPosType)) {
		startPosType = static_cast<CGameSetup::StartPosType>(std::atoi(matchPosType[1].first));
	} else {
		startPosType = CGameSetup::StartPosType::StartPos_ChooseInGame;
	}

	std::vector<CSetupData::Box> startBoxes;
	startBoxes.reserve(boxesMap.size());
	// Remap start boxes
	// @see rts/Game/GameSetup.cpp CGameSetup::Init
//	for (const std::map<int, Box>::value_type& kv : boxesMap) {
//	for (const std::pair<const int, std::array<float, 4>>& kv : boxesMap) {
	for (const auto& kv : boxesMap) {
		startBoxes.push_back(kv.second);
	}

	setupData->Init(startBoxes, startPosType);
}

bool CSetupManager::HasStartBoxes()
{
	return (setupData->IsInitialized() && !setupData->IsEmpty());
}

bool CSetupManager::CanChooseStartPos()
{
	return setupData->CanChooseStartPos();
}

void CSetupManager::PickStartPos(CCircuitAI* circuit, StartPosType type)
{
	float x, z;
	const CSetupData::Box& box = (*setupData)[circuit->GetAllyTeamId()];

	auto random = [](const CSetupData::Box& box, float& x, float& z) {
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
				STerrainMapMobileType* mobileType = terrain->GetMobileTypeById(circuit->GetCircuitDef(circuit->GetUnitDefByName("armcom1"))->GetMobileTypeId());
				std::vector<int> filteredIndices;
				for (auto idx : inBoxIndices) {
					int iS = terrain->GetSectorIndex(spots[idx].position);
					STerrainMapArea* area = mobileType->sector[iS].area;
					if ((area != nullptr) && area->percentOfMap >= 0.1) {
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

CCircuitUnit* CSetupManager::GetCommander()
{
	return circuit->GetTeamUnitById(commanderId);
}

void CSetupManager::SetStartPos(const AIFloat3& pos)
{
	startPos = pos;
}

const AIFloat3& CSetupManager::GetStartPos()
{
	return startPos;
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
