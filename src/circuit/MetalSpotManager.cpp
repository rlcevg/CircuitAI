/*
 * MetalSpotManager.cpp
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#include "MetalSpotManager.h"

#include <functional>
#include <algorithm>

namespace circuit {

using namespace springai;

CMetalSpotManager::CMetalSpotManager(std::vector<Metal>& spots) :
		mexPerClusterAvg(DEFAULT_MEXPERCLUSTER)
{
	SortSpotsRadial();
}

void CMetalSpotManager::SortSpotsRadial()
{
	if (spots.size() <= 1) {
		return;
	}

	auto orientation = [](const AIFloat3& p1, const AIFloat3& p2, const AIFloat3& p3) {
		// orientation > 0 : counter-clockwise turn,
		// orientation < 0 : clockwise,
		// orientation = 0 : collinear
		return (p2.x - p1.x) * (p3.z - p1.z) - (p2.z - p1.z) * (p3.x - p1.x);
	};
	std::function<float(const AIFloat3&, const AIFloat3&)> dist = [](const AIFloat3& p1, const AIFloat3& p2) -> float {
		float x = p1.x - p2.x;
		float z = p1.z - p2.z;
		return x * x + z * z;
	};

	// Find the bottom-most point
	int min = 0, i = 0;
	float zmin = spots[0].position.z;
	for (const Metal& spot : spots) {
		float z = spot.position.z;
		// Pick the bottom-most or chose the left most point in case of tie
		if ((z < zmin) || (zmin == z && spots[i].position.x < spots[min].position.x)) {
			zmin = z, min = i;
		}
		i++;
	}

	auto swap = [](Metal& m1, Metal& m2) {
		Metal tmp = m1;
		m1 = m2;
		m2 = tmp;
	};
	swap(spots[0], spots[min]);

	// A function used by sort() to sort an array of
	// points with respect to the first point
	AIFloat3& p0 = spots[0].position;
	auto compare = [&p0, orientation, dist](const Metal& m1, const Metal& m2) {
		// Find orientation
		int o = orientation(p0, m1.position, m2.position);
		if (o == 0) {
			return dist(p0, m1.position) < dist(p0, m2.position);
		}
		return o > 0;
	};
	// Sort n-1 points with respect to the first point. A point p1 comes
	// before p2 in sorted output if p2 has larger polar angle (in
	// counterclockwise direction) than p1
	std::sort(spots.begin() + 1, spots.end(), compare);
}

CMetalSpotManager::~CMetalSpotManager()
{
}

bool CMetalSpotManager::IsEmpty()
{
	return spots.empty();
}

std::vector<Metal>& CMetalSpotManager::GetSpots()
{
	return spots;
}

} // namespace circuit
