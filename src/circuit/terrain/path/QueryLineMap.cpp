/*
 * QueryLineMap.cpp
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#include "terrain/path/QueryLineMap.h"
#include "map/ThreatMap.h"

namespace circuit {

using namespace springai;

CQueryLineMap::CQueryLineMap(const CPathFinder& pathfinder, int id)
		: IPathQuery(pathfinder, id, Type::LINE)
{
}

CQueryLineMap::~CQueryLineMap()
{
}

void CQueryLineMap::InitQuery(int threatXSize, int squareSize)
{
	this->threatXSize = threatXSize;
	this->squareSize = squareSize;
}

/*
 * WARNING: startPos, endPos must be correct
 */
bool CQueryLineMap::IsSafeLine(const AIFloat3& startPos, const AIFloat3& endPos) const
{
	int2 start(startPos.x / squareSize, startPos.z / squareSize);
	int2 end(endPos.x / squareSize, endPos.z / squareSize);
	// All octant line draw
	const int dx =  abs(end.x - start.x), sx = start.x < end.x ? 1 : -1;
	const int dy = -abs(end.y - start.y), sy = start.y < end.y ? 1 : -1;
	int err = dx + dy;  // error value e_xy
	for (int x = start.x, y = start.y;;) {
		const int e2 = 2 * err;
		if (e2 >= dy) {  // e_xy + e_x > 0
			if (x == end.x) break;
			err += dy; x += sx;
		}
		if (e2 <= dx) {  // e_xy + e_y < 0
			if (y == end.y) break;
			err += dx; y += sy;
		}

		int index2 = y * threatXSize + x;
		int index = (y + 1) * (threatXSize + 2) + x + 1;  // move-index, +2 for edges, index = (y + 1) * (threatXSize + 2) + x + 1;
		if (!canMoveArray[index] || (threatArray[index2] > THREAT_MIN)) {
			return false;
		}
	}
	return true;
}

} // namespace circuit
