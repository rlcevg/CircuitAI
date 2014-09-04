/*
 * GameAttribute.h
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#ifndef GAMEATTRIBUTE_H_
#define GAMEATTRIBUTE_H_

#include <memory>
#include <vector>

namespace springai {
	class GameRulesParam;
}

namespace circuit {

class CSetupManager;
class CMetalManager;

class CGameAttribute {
public:
	CGameAttribute();
	virtual ~CGameAttribute();

	void ParseSetupScript(const char* setupScript, int width, int height);
	void ParseMetalSpots(const char* metalJson);
	void ParseMetalSpots(const std::vector<springai::GameRulesParam*>& metalParams);
	bool HasStartBoxes(bool checkEmpty = true);
	bool HasMetalSpots(bool checkEmpty = true);
	CSetupManager& GetSetupManager();
	CMetalManager& GetMetalManager();

private:
	std::shared_ptr<CSetupManager> setupManager;
	std::shared_ptr<CMetalManager> metalManager;
};

} // namespace circuit

#endif // GAMEATTRIBUTE_H_
