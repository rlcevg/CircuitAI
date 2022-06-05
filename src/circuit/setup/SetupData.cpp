/*
 * SetupData.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "setup/SetupData.h"
#include "CircuitAI.h"
#include "util/math/Region.h"
#include "util/Utils.h"

#include "spring/SpringMap.h"

#include <regex>

namespace circuit {

using namespace springai;

CSetupData::CSetupData() :
		isInitialized(false),
		startPosType(CGameSetup::StartPosType::StartPos_Fixed)
{
}

CSetupData::~CSetupData()
{
	utils::free_clear(allyTeams);
}

void CSetupData::ParseSetupScript(CCircuitAI* circuit, const char* setupScript)
{
	std::string script(setupScript);
	std::map<int, int> teamIdsRemap;
	using OrigTeamIds = std::set<int>;
	std::map<int, OrigTeamIds> allies;
	CSetupData::BoxMap boxes;

	boxes = std::move(ReadStartBoxes(script, circuit->GetMap(), circuit->GetGame()));

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
	std::string::const_iterator start = script.begin();
	std::string::const_iterator end = script.end();
	std::smatch section;
	while (std::regex_search(start, end, section, patternAlly)) {
		int allyTeamId = utils::string_to_int(section[1]);
		allies[allyTeamId];  // create empty alliance
		start = section[0].second;
	}

	// Detect team alliances
	std::regex patternTeam("\\[team(\\d+)\\]", std::regex::ECMAScript | std::regex::icase);
	std::regex patternAllyId("allyteam=(\\d+);", std::regex::ECMAScript | std::regex::icase);
	start = script.begin();
	end = script.end();
	while (std::regex_search(start, end, section, patternTeam)) {
		start = section[0].second;
		int teamId = utils::string_to_int(section[1]);
		teamIdsRemap[teamId] = teamId;

		std::string::const_iterator bodyEnd = utils::EndInBraces(start, end);
		std::smatch matchAllyId;
		if (std::regex_search(start, bodyEnd, matchAllyId, patternAllyId)) {
			int allyTeamId = utils::string_to_int(matchAllyId[1]);
			allies[allyTeamId].insert(teamId);
		}

		start = bodyEnd;
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
		allyTeams.push_back(new CAllyTeam(teamIds, (boxes.size() < allies.size()) ? boxes[0] : boxes[kv.first]));
	}

	Init(std::move(allyTeams), std::move(boxes), startPosType);
}

void CSetupData::Init(AllyMap&& ats, BoxMap&& bm, CGameSetup::StartPosType spt)
{
	allyTeams = ats;
	boxes = bm;
	startPosType = spt;

	isInitialized = true;
}

CSetupData::BoxMap CSetupData::ReadStartBoxes(const std::string& script, CMap* map, Game* game)
{
	CSetupData::BoxMap boxes;

	int boxCount = game->GetRulesParamFloat("startbox_max_n", -1);
	if (boxCount > 0) {
		// zk startbox (polygons)
		for (int boxId = 0; boxId < boxCount; ++boxId) {
			std::string sBoxId = utils::int_to_string(boxId);
			int numPolygons = game->GetRulesParamFloat((std::string("startbox_n_") + sBoxId).c_str(), -1);
			std::vector<utils::CPolygon> polys;
			polys.reserve(numPolygons);
			for (int i = 1; i <= numPolygons; ++i) {
				std::string sI = utils::int_to_string(i);
				int numVerts = game->GetRulesParamFloat((std::string("startbox_polygon_") + sBoxId + "_" + sI).c_str(), -1);
				std::vector<AIFloat3> verts;
				verts.reserve(numVerts);
				for (int j = 1; j <= numVerts; ++j) {
					std::string sPolyVert = utils::int_to_string(j, sBoxId  + "_" + sI + "_%i");
					float x = game->GetRulesParamFloat((std::string("startbox_polygon_x_") + sPolyVert).c_str(), 0.f);
					float z = game->GetRulesParamFloat((std::string("startbox_polygon_z_") + sPolyVert).c_str(), 0.f);
					verts.push_back(AIFloat3(x, 0.f, z));
				}
				polys.emplace_back(std::move(verts));
			}
			boxes[boxId] = utils::CRegion(std::move(polys));
		}

	} else {

		// engine startbox
		float width = map->GetWidth() * SQUARE_SIZE;
		float height = map->GetHeight() * SQUARE_SIZE;
		std::string::const_iterator start = script.begin();
		std::string::const_iterator end = script.end();
		std::smatch section;
		const std::map<std::string, int> rectWords = {{"left", 0}, {"right", 1}, {"top", 2}, {"bottom", 3}};
		std::regex patternAlly("\\[allyteam(\\d+)\\]", std::regex::ECMAScript | std::regex::icase);
		std::regex patternRect("startrect(\\w+)=(\\d+(\\.\\d+)?);", std::regex::ECMAScript | std::regex::icase);
		while (std::regex_search(start, end, section, patternAlly)) {
			start = section[0].second;
			int allyTeamId = utils::string_to_int(section[1]);

			utils::SBox startbox;
			std::string::const_iterator bodyStart = start;
			std::string::const_iterator bodyEnd = utils::EndInBraces(start, end);
			std::smatch rectm;
			while (std::regex_search(bodyStart, bodyEnd, rectm, patternRect)) {
				std::string word = rectm[1];
				std::for_each(word.begin(), word.end(), [](char& c){ c = std::tolower(c); });
				auto wordIt = rectWords.find(word);
				if (wordIt != rectWords.end()) {
					startbox.edge[wordIt->second] = utils::string_to_float(rectm[2]);
				}

				bodyStart = rectm[0].second;
			}

			startbox.left   *= width;
			startbox.right  *= width;
			startbox.top    *= height;
			startbox.bottom *= height;
			boxes[allyTeamId] = utils::CRegion(std::move(startbox));

			start = bodyEnd;
		}
	}

	return boxes;
}

} // namespace circuit
