/*
 * StartBox.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "StartBox.h"
#include "utils.h"

#include <map>
#include <regex>

namespace circuit {

CStartBox::CStartBox(const char* setupScript, int width, int height)
{
	std::map<int, Box> boxesMap;
	std::regex patternAlly("\\[allyteam(\\d)\\]\\s*\\{([^\\}]*)\\}");
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
		int i = 0;
		// 0 -> bottom
		// 1 -> left
		// 2 -> right
		// 3 -> top
		for (; iter != end && i < 4; ++iter, i++) {
			startbox[i] = utils::string_to_float(*iter);
		}

		float mapWidth = SQUARE_SIZE * width;
		float mapHeight = SQUARE_SIZE * height;
		startbox[static_cast<int>(BoxEdges::BOTTOM)] *= mapHeight;
		startbox[static_cast<int>(BoxEdges::LEFT)  ] *= mapWidth;
		startbox[static_cast<int>(BoxEdges::RIGHT) ] *= mapWidth;
		startbox[static_cast<int>(BoxEdges::TOP)   ] *= mapHeight;
		boxesMap[allyTeamId] = startbox;

		start = allyteam[0].second;
	}

	// Remap start boxes
	// @see rts/Game/GameSetup.cpp CGameSetup::Init
//	for (const std::map<int, Box>::value_type& kv : boxesMap) {
//	for (const std::pair<const int, std::array<float, 4>>& kv : boxesMap) {
	for (const auto& kv : boxesMap) {
		boxes.push_back(kv.second);
	}
}

CStartBox::~CStartBox()
{
}

bool CStartBox::IsEmpty()
{
	return boxes.empty();
}

const Box& CStartBox::operator[](int idx) const
{
	return boxes[idx];
}

} // namespace circuit
