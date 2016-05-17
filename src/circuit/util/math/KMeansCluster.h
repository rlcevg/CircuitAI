/*
 * KMeansCluster.h
 *
 *  Created on: Mar 7, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MATH_KMEANSCLUSTER_H_
#define SRC_CIRCUIT_UTIL_MATH_KMEANSCLUSTER_H_

#include <vector>

namespace springai {
	class AIFloat3;
}

namespace circuit {

class CKMeansCluster {
public:
	CKMeansCluster(const springai::AIFloat3& initPos);
	virtual ~CKMeansCluster();

	void Iteration(const std::vector<springai::AIFloat3>& unitPositions, int newK);
	const std::vector<springai::AIFloat3>& GetMeans() const { return means; }

private:
	std::vector<springai::AIFloat3> means;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_MATH_KMEANSCLUSTER_H_
