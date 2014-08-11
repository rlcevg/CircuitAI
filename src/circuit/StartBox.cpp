/*
 * StartBox.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "StartBox.h"
#include "utils.h"

#include <regex>

namespace circuit {

CStartBox* CStartBox::singleton = NULL;
uint CStartBox::counter = 0;

CStartBox::CStartBox(const char* setupScript, int width, int height)
{
	// TODO: replace map with array?
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
		float* startbox = new float[4];
		int i = 0;
		// 0 -> bottom
		// 1 -> left
		// 2 -> right
		// 3 -> top
		for(; iter != end && i < 4; ++iter, i++ ) {
			startbox[i] = utils::string_to_float(*iter);
		}

		float mapWidth = SQUARE_SIZE * width;
		float mapHeight = SQUARE_SIZE * height;
		startbox[static_cast<int>(BoxEdges::BOTTOM)] *= mapHeight;
		startbox[static_cast<int>(BoxEdges::LEFT)  ] *= mapWidth;
		startbox[static_cast<int>(BoxEdges::RIGHT) ] *= mapWidth;
		startbox[static_cast<int>(BoxEdges::TOP)   ] *= mapHeight;
		boxes[allyTeamId] = startbox;

		start = allyteam[0].second;
	}
}

CStartBox::~CStartBox()
{
//	for (std::pair<const int, float*>& x: startBoxes) {
	for (auto& box: boxes) {
		delete [] box.second;
	}
	boxes.clear();
}

void CStartBox::CreateInstance(const char* setupScript, int width, int height)
{
	if (singleton == NULL) {
		singleton = new CStartBox(setupScript, width, height);
	}
	counter++;
}

void CStartBox::DestroyInstance()
{
	if (counter <= 1) {
		if (singleton != NULL) {
			// SafeDelete
			CStartBox* tmp = singleton;
			singleton = NULL;
			delete tmp;
		}
		counter = 0;
	} else {
		counter--;
	}
}

const float* CStartBox::operator[](int idx) const
{
//	return boxes[idx];
	return boxes.find(idx)->second;
}

} // namespace circuit
