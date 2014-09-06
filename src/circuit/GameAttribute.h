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
#include <map>
#include <string.h>

namespace springai {
	class GameRulesParam;
	class Game;
	class Map;
	class UnitDef;
	class Pathing;
}

namespace circuit {

class CSetupManager;
class CMetalManager;
class CScheduler;
enum class StartPosType: char;

class CGameAttribute {
public:
	CGameAttribute();
	virtual ~CGameAttribute();

	void ParseSetupScript(const char* setupScript, int width, int height);
	bool HasStartBoxes(bool checkEmpty = true);
	bool CanChooseStartPos();
	void PickStartPos(springai::Game* game, springai::Map* map, StartPosType type);
	CSetupManager& GetSetupManager();

	void ParseMetalSpots(const char* metalJson);
	void ParseMetalSpots(const std::vector<springai::GameRulesParam*>& metalParams);
	bool HasMetalSpots(bool checkEmpty = true);
	bool HasMetalClusters();
	void ClusterizeMetal(std::shared_ptr<CScheduler> scheduler, float maxDistance, int pathType, springai::Pathing* pathing);
	CMetalManager& GetMetalManager();

	void InitUnitDefs(std::vector<springai::UnitDef*>& unitDefs);
	bool HasUnitDefs();
	springai::UnitDef* GetUnitDefByName(const char* name);

private:
	std::shared_ptr<CSetupManager> setupManager;
	std::shared_ptr<CMetalManager> metalManager;

	struct cmp_str {
	   bool operator()(char const *a, char const *b) {
	      return strcmp(a, b) < 0;
	   }
	};
	std::map<const char*, springai::UnitDef*, cmp_str> definitions;
};

} // namespace circuit

#endif // GAMEATTRIBUTE_H_
