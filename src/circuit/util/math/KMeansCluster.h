/*
 * KMeansCluster.h
 *
 *  Created on: Mar 7, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MATH_KMEANSCLUSTER_H_
#define SRC_CIRCUIT_UTIL_MATH_KMEANSCLUSTER_H_

#include "AIFloat3.h"

#include <vector>

namespace circuit {

class CKMeansCluster {
public:
	CKMeansCluster(const springai::AIFloat3& initPos);
	virtual ~CKMeansCluster();

	const std::vector<springai::AIFloat3>& GetMeans() const { return means; }
	void Iteration(std::vector<springai::AIFloat3> unitPositions, int newK);

private:
	std::vector<springai::AIFloat3> means;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_MATH_KMEANSCLUSTER_H_
