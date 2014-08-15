/*
 * MetalSpot.cpp
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#include "MetalSpot.h"
#include "json/json.h"

namespace circuit {

using namespace springai;

CMetalSpot::CMetalSpot(const char* setupMetal) :
		mexPerClusterAvg(4)
{
	Json::Value root;
	Json::Reader json;

	if (!json.parse(setupMetal, root, false)) {
		return;
	}

	for (const Json::Value& object : root) {
		Metal spot;
		spot.income = object["metal"].asFloat();
		spot.position = AIFloat3(object["x"].asFloat(),
								 object["y"].asFloat(),
								 object["z"].asFloat());
		spots.push_back(spot);
	}
}

CMetalSpot::~CMetalSpot()
{
}

} // namespace circuit
