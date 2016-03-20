/*
 * HierarchCluster.h
 *
 *  Created on: Mar 21, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MATH_HIERARCHCLUSTER_H_
#define SRC_CIRCUIT_UTIL_MATH_HIERARCHCLUSTER_H_

#include <vector>

namespace circuit {

class CRagMatrix;

class CHierarchCluster {
public:
	using Clusters = std::vector<std::vector<int>>;

	CHierarchCluster();
	virtual ~CHierarchCluster();

	const Clusters& Clusterize(CRagMatrix& distmatrix, float maxDistance);
	const Clusters& GetClusters() const { return iclusters; }

private:
	Clusters iclusters;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_MATH_HIERARCHCLUSTER_H_
