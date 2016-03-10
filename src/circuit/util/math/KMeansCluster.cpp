/*
 * KMeansCluster.cpp
 *
 *  Created on: Mar 7, 2016
 *      Author: rlcevg
 */

#include "util/math/KMeansCluster.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CKMeansCluster::CKMeansCluster(const AIFloat3& initPos)
{
	means.push_back(initPos);
}

CKMeansCluster::~CKMeansCluster()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

/*
 * 2d only, ignores y component.
 * @see KAIK/AttackHandler::KMeansIteration for general reference
 */
void CKMeansCluster::Iteration(std::vector<AIFloat3> unitPositions, int newK)
{
	assert(newK > 0 && means.size() > 0);
	int numUnits = unitPositions.size();
	// change the number of means according to newK
	int oldK = means.size();
	means.resize(newK);
	// add a new means, just use one of the positions
	AIFloat3 newMeansPosition = unitPositions[0];
//	newMeansPosition.y = ai->cb->GetElevation(newMeansPosition.x, newMeansPosition.z) + K_MEANS_ELEVATION;

	for (int i = oldK; i < newK; i++) {
		means[i] = newMeansPosition;
	}

	// check all positions and assign them to means, complexity n*k for one iteration
	std::vector<int> unitsClosestMeanID(numUnits, -1);
	std::vector<int> numUnitsAssignedToMean(newK, 0);

	for (int i = 0; i < numUnits; i++) {
		AIFloat3 unitPos = unitPositions[i];
		float closestDistance = std::numeric_limits<float>::max();
		int closestIndex = -1;

		for (int m = 0; m < newK; m++) {
			AIFloat3 mean = means[m];
			float distance = unitPos.SqDistance2D(mean);

			if (distance < closestDistance) {
				closestDistance = distance;
				closestIndex = m;
			}
		}

		// position i is closest to the mean at closestIndex
		unitsClosestMeanID[i] = closestIndex;
		numUnitsAssignedToMean[closestIndex]++;
	}

	// change the means according to which positions are assigned to them
	// use meanAverage for indexes with 0 pos'es assigned
	// make a new means list
//	std::vector<AIFloat3> newMeans(newK, ZeroVector);
	std::vector<AIFloat3>& newMeans = means;
	std::fill(newMeans.begin(), newMeans.end(), ZeroVector);

	for (int i = 0; i < numUnits; i++) {
		int meanIndex = unitsClosestMeanID[i];
		 // don't divide by 0
		float num = std::max(1, numUnitsAssignedToMean[meanIndex]);
		newMeans[meanIndex] += unitPositions[i] / num;
	}

	// do a check and see if there are any empty means and set the height
	for (int i = 0; i < newK; i++) {
		// if a newmean is unchanged, set it to the new means pos instead of (0, 0, 0)
		if (newMeans[i] == ZeroVector) {
			newMeans[i] = newMeansPosition;
		}
		else {
			// get the proper elevation for the y-coord
//			newMeans[i].y = ai->cb->GetElevation(newMeans[i].x, newMeans[i].z) + K_MEANS_ELEVATION;
		}
	}

//	return newMeans;
}

} // namespace circuit
