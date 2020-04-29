/*
 * QueryCostMap.cpp
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#include "terrain/path/QueryCostMap.h"

namespace circuit {

using namespace springai;

CQueryCostMap::CQueryCostMap(const CPathFinder& pathfinder, int id)
		: IPathQuery(pathfinder, id)
{
}

CQueryCostMap::~CQueryCostMap()
{
}

void CQueryCostMap::InitQuery(const AIFloat3& startPos)
{
	this->startPos = startPos;
}

void CQueryCostMap::Prepare()
{
	// TODO: Cache to avoid memory allocations
	costMap.resize(pathfinder.GetPathMapXSize() * pathfinder.GetPathMapYSize(), -1.f);
//	std::fill(costMap.begin(), costMap.end(), -1.f);
}

/*
 * WARNING: endPos must be correct
 */
float CQueryCostMap::GetCostAt(const AIFloat3& endPos, int radius) const
{
	float pathCost = -1.f;
	radius /= pathfinder.GetSquareSize();

	int xm, ym;
	pathfinder.Pos2PathXY(endPos, &xm, &ym);

	auto minCost = pathfinder.IsInPathMap(xm, ym, radius)
	? std::function<float (float, int, int)>([this](float cost, int x, int y) -> float {
		const float costR = costMap[pathfinder.PathXY2PathIndex(x, y)];
		if (cost < 0.f) {
			return costR;
		}
		return (costR < 0.f) ? cost : std::min(cost, costR);
	})
	: std::function<float (float, int, int)>([this](float cost, int x, int y) -> float {
		if (!pathfinder.IsInPathMap(x, y)) {
			return cost;
		}
		const float costR = costMap[pathfinder.PathXY2PathIndex(x, y)];
		if (cost < 0.f) {
			return costR;
		}
		return (costR < 0.f) ? cost : std::min(cost, costR);
	});

	// test 8 points
	pathCost = minCost(pathCost, xm - radius, ym);
	pathCost = minCost(pathCost, xm + radius, ym);
	pathCost = minCost(pathCost, xm, ym - radius);
	pathCost = minCost(pathCost, xm, ym + radius);

	const int r2 = radius * ISQRT_2 + 1;
	pathCost = minCost(pathCost, xm - r2, ym - r2);
	pathCost = minCost(pathCost, xm + r2, ym - r2);
	pathCost = minCost(pathCost, xm - r2, ym + r2);
	pathCost = minCost(pathCost, xm + r2, ym + r2);

	return pathCost;
}

} // namespace circuit
