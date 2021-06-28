/*
 * BuildChain.h
 *
 *  Created on: Aug 10, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_BUILDCHAIN_H_
#define SRC_CIRCUIT_TASK_BUILDER_BUILDCHAIN_H_

#include "task/builder/BuilderTask.h"

#include "AIFloat3.h"

#include <vector>

namespace circuit {

#define DIRECTION(x)	SBuildInfo::Direction::x

struct SBuildInfo {
	enum class Direction: char {LEFT = 0, RIGHT, FRONT, BACK, NONE};
	enum class Condition: char {AIR = 0, ENERGY, WIND, SENSOR, CHANCE, ALWAYS};

	using DirName = std::map<std::string, Direction>;
	using CondName = std::map<std::string, Condition>;
	using PrioName = std::map<std::string, IBuilderTask::Priority>;
	static DirName& GetDirNames() { return dirNames; }
	static CondName& GetCondNames() { return condNames; }
	static PrioName& GetPrioNames() { return prioNames; }
	static DirName dirNames;
	static CondName condNames;
	static PrioName prioNames;

	CCircuitDef* cdef;
	IBuilderTask::BuildType buildType;
	springai::AIFloat3 offset;
	Direction direction;
	Condition condition;
	IBuilderTask::Priority priority;
	float value;
};

struct SBuildChain {
	float energy;
	bool isMexEngy;
	bool isPylon;
	bool isPorc;
	bool isTerra;
	std::vector<std::vector<SBuildInfo>> hub;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BUILDCHAIN_H_
