/*
 * PathInfo.cpp
 *
 *  Created on: Apr 21, 2022
 *      Author: rlcevg
 */

#include "terrain/path/PathInfo.h"
#include "terrain/path/PathFinder.h"

namespace circuit {

using namespace springai;

void CPathInfo::PushPos(const AIFloat3& pos, CPathFinder* pathfinder)
{
	posPath.push_back(pos);
	path.push_back(pathfinder->Pos2PathIndex(pos));
}

} // namespace circuit
