/*
 * SetupData.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "setup/SetupData.h"
#include "CircuitAI.h"
#include "util/TdfParser.h"
#include "util/utils.h"

#include "Map.h"

#include <regex>

namespace circuit {

using namespace springai;

CSetupData::CSetupData() :
		initialized(false),
		startPosType(CGameSetup::StartPosType::StartPos_Fixed)
{
}

CSetupData::~CSetupData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(allyTeams);
}

void CSetupData::ParseSetupScript(CCircuitAI* circuit, const char* setupScript)
{
	// TODO: Replace "std::string script" by "TdfParser script" as much as possible
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
	std::regex patternBox("startboxes=(.*);", std::regex::ECMAScript | std::regex::icase);
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
		std::regex patternAlly("\\[allyteam(\\d+)\\]\\s*\\{([^\\}]*)\\}", std::regex::ECMAScript | std::regex::icase);
		std::regex patternRect("startrect\\w+=(\\d+(\\.\\d+)?);", std::regex::ECMAScript | std::regex::icase);
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
	std::regex patternPosType("startpostype=(\\d+)", std::regex::ECMAScript | std::regex::icase);
	if (std::regex_search(setupScript, matchPosType, patternPosType)) {
		startPosType = static_cast<CGameSetup::StartPosType>(std::atoi(matchPosType[1].first));
	} else {
		startPosType = CGameSetup::StartPosType::StartPos_Fixed;
	}

	// Count number of alliances
	std::regex patternAlly("\\[allyteam(\\d+)\\]", std::regex::ECMAScript | std::regex::icase);
	start = script.begin();
	end = script.end();
	while (std::regex_search(start, end, section, patternAlly)) {
		int allyTeamId = utils::string_to_int(section[1]);
		allies[allyTeamId];  // create empty alliance
		start = section[0].second;
	}

	// Detect team alliances
	std::regex patternTeam("\\[team(\\d+)\\]\\s*\\{([^\\}]*)\\}", std::regex::ECMAScript | std::regex::icase);
	std::regex patternAllyId("allyteam=(\\d+);", std::regex::ECMAScript | std::regex::icase);
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
	CSetupData::AllyMap allyTeams;
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

	// @see CGameSetup::LoadSkirmishAIs
	CSetupData::ConfigMap configJsons;
	TdfParser parser(circuit, setupScript, strlen(setupScript));
	for (int a = 0; a < MAX_PLAYERS; ++a) {
		const std::string section = "GAME\\AI" + IntToString(a, "%i");
		if (!parser.SectionExist(section)) {
			continue;
		}

		std::string config = parser.SGetValueDef("", section + "\\Options\\JSON");
		configJsons.push_back(config);
	}

	Init(allyTeams, boxes, configJsons, startPosType);
}

void CSetupData::Init(const AllyMap& ats, const BoxMap& bm, const ConfigMap& cs, CGameSetup::StartPosType spt)
{
	allyTeams = ats;
	boxes = bm;
	startPosType = spt;
	configJsons = cs;

	initialized = true;
}

} // namespace circuit
