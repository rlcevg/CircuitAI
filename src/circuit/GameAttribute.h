/*
 * GameAttribute.h
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#ifndef GAMEATTRIBUTE_H_
#define GAMEATTRIBUTE_H_

#include "StartBoxManager.h"
#include "MetalSpotManager.h"

#include "GameRulesParam.h"

#include <memory>

namespace circuit {

class CGameAttribute {
public:
	CGameAttribute();
	virtual ~CGameAttribute();

	void ParseSetupScript(const char* setupScript, int width, int height);
	void ParseMetalSpots(const char* metalJson);
	void ParseMetalSpots(std::vector<springai::GameRulesParam*>& metalParams);
	bool HasStartBoxes(bool checkEmpty = true);
	bool HasMetalSpots(bool checkEmpty = true);
	CStartBoxManager& GetStartBoxManager();
	CMetalSpotManager& GetMetalSpotManager();

private:
	std::shared_ptr<CStartBoxManager> startBoxManager;
	std::shared_ptr<CMetalSpotManager> metalSpotManager;
};

} // namespace circuit

#endif // GAMEATTRIBUTE_H_
