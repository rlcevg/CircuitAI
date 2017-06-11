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
	enum class Condition: char {AIR = 0, NO_AIR, MAYBE, ALWAYS};

	using DirName = std::map<std::string, Direction>;
	using CondName = std::map<std::string, Condition>;
	static DirName& GetDirNames() { return dirNames; }
	static CondName& GetCondNames() { return condNames; }
	static DirName dirNames;
	static CondName condNames;

	CCircuitDef* cdef;
	IBuilderTask::BuildType buildType;
	springai::AIFloat3 offset;
	Direction direction;
	Condition condition;
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
