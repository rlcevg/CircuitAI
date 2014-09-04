/*
 * GameAttribute.cpp
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#include "GameAttribute.h"
#include "SetupManager.h"
#include "MetalManager.h"
#include "utils.h"
#include "json/json.h"

#include "GameRulesParam.h"

#include <map>
#include <regex>

namespace circuit {

using namespace springai;

CGameAttribute::CGameAttribute() :
		setupManager(nullptr),
		metalManager(nullptr)
{
	srand(time(nullptr));
}

CGameAttribute::~CGameAttribute()
{
	printf("<DEBUG> Entering: %s\n", __PRETTY_FUNCTION__);
}

void CGameAttribute::ParseSetupScript(const char* setupScript, int width, int height)
{
	std::map<int, Box> boxesMap;
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
		Box startbox;
		for (int i = 0; iter != end && i < 4; ++iter, i++) {
			startbox.edge[i] = utils::string_to_float(*iter);
		}

		float mapWidth = SQUARE_SIZE * width;
		float mapHeight = SQUARE_SIZE * height;
		startbox.bottom *= mapHeight;
		startbox.left   *= mapWidth;
		startbox.right  *= mapWidth;
		startbox.top    *= mapHeight;
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

	std::vector<Box> startBoxes;
	// Remap start boxes
	// @see rts/Game/GameSetup.cpp CGameSetup::Init
//	for (const std::map<int, Box>::value_type& kv : boxesMap) {
//	for (const std::pair<const int, std::array<float, 4>>& kv : boxesMap) {
	for (const auto& kv : boxesMap) {
		startBoxes.push_back(kv.second);
	}

	setupManager = std::make_shared<CSetupManager>(startBoxes, startPosType);
}

void CGameAttribute::ParseMetalSpots(const char* metalJson)
{
	Json::Value root;
	Json::Reader json;

	if (!json.parse(metalJson, root, false)) {
		return;
	}

	std::vector<Metal> spots;
	for (const Json::Value& object : root) {
		Metal spot;
		spot.income = object["metal"].asFloat();
		spot.position = AIFloat3(object["x"].asFloat(),
								 object["y"].asFloat(),
								 object["z"].asFloat());
		spots.push_back(spot);
	}

	metalManager = std::make_shared<CMetalManager>(spots);
}

void CGameAttribute::ParseMetalSpots(const std::vector<springai::GameRulesParam*>& gameParams)
{
	int mexCount = 0;
	for (auto& param : gameParams) {
		if (strcmp(param->GetName(), "mex_count") == 0) {
			mexCount = param->GetValueFloat();
			break;
		}
	}

	if (mexCount <= 0) {
		return;
	}

	std::vector<Metal> spots(mexCount);
	int i = 0;
	for (auto& param : gameParams) {
		const char* name = param->GetName();
		if (strncmp(name, "mex_", 4) == 0) {
			if (strncmp(name + 4, "x", 1) == 0) {
				int idx = std::atoi(name + 5);
				spots[idx - 1].position.x = param->GetValueFloat();
				i++;
			} else if (strncmp(name + 4, "y", 1) == 0) {
				int idx = std::atoi(name + 5);
				spots[idx - 1].position.y = param->GetValueFloat();
				i++;
			} else if (strncmp(name + 4, "z", 1) == 0) {
				int idx = std::atoi(name + 5);
				spots[idx - 1].position.z = param->GetValueFloat();
				i++;
			} else if (strncmp(name + 4, "metal", 5) == 0) {
				int idx = std::atoi(name + 9);
				spots[idx - 1].income = param->GetValueFloat();
				i++;
			}

			if (i >= mexCount * 4) {
				break;
			}
		}
	}

	metalManager = std::make_shared<CMetalManager>(spots);
}

bool CGameAttribute::HasStartBoxes(bool checkEmpty)
{
	if (checkEmpty) {
		return (setupManager != nullptr && !setupManager->IsEmpty());
	}
	return setupManager != nullptr;
}

bool CGameAttribute::HasMetalSpots(bool checkEmpty)
{
	if (checkEmpty) {
		return (metalManager != nullptr && !metalManager->IsEmpty());
	}
	return metalManager != nullptr;
}

CSetupManager& CGameAttribute::GetSetupManager()
{
	return *setupManager;
}

CMetalManager& CGameAttribute::GetMetalManager()
{
	return *metalManager;
}

} // namespace circuit
