/*
 * BuildChain.cpp
 *
 *  Created on: Aug 11, 2016
 *      Author: rlcevg
 */

#include "task/builder/BuildChain.h"

namespace circuit {

SBuildInfo::DirName SBuildInfo::dirNames = {
	{"left",  SBuildInfo::Direction::LEFT},
	{"right", SBuildInfo::Direction::RIGHT},
	{"front", SBuildInfo::Direction::FRONT},
	{"back",  SBuildInfo::Direction::BACK},
	{"none",  SBuildInfo::Direction::NONE},
};

SBuildInfo::CondName SBuildInfo::condNames = {
	{"air",    SBuildInfo::Condition::AIR},
	{"no_air", SBuildInfo::Condition::NO_AIR},
	{"energy", SBuildInfo::Condition::ENERGY},
	{"chance", SBuildInfo::Condition::CHANCE},
	{"always", SBuildInfo::Condition::ALWAYS},
};

} // namespace circuit
