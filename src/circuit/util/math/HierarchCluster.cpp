/*
 * HierarchCluster.cpp
 *
 *  Created on: Mar 21, 2015
 *      Author: rlcevg
 */

#include "util/math/HierarchCluster.h"
#include "util/math/RagMatrix.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CHierarchCluster::CHierarchCluster()
{
}

CHierarchCluster::~CHierarchCluster()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

const CHierarchCluster::Clusters& CHierarchCluster::Clusterize(CRagMatrix& distmatrix, float maxDistance)
{
	int nrows = distmatrix.GetNrows();

	// Initialize cluster-element list
	iclusters.clear();
	iclusters.reserve(nrows);
	for (int i = 0; i < nrows; i++) {
		std::vector<int> cluster;
		cluster.push_back(i);
		iclusters.push_back(cluster);
	}

	for (int n = nrows; n > 1; n--) {
		// Find pair
		int is = 1;
		int js = 0;
		if (distmatrix.FindClosestPair(n, is, js) > maxDistance) {
			break;
		}

		// Fix the distances
		for (int j = 0; j < js; j++) {
			distmatrix(js, j) = std::max(distmatrix(is, j), distmatrix(js, j));
		}
		for (int j = js + 1; j < is; j++) {
			distmatrix(j, js) = std::max(distmatrix(is, j), distmatrix(j, js));
		}
		for (int j = is + 1; j < n; j++) {
			distmatrix(j, js) = std::max(distmatrix(j, is), distmatrix(j, js));
		}

		for (int j = 0; j < is; j++) {
			distmatrix(is, j) = distmatrix(n - 1, j);
		}
		for (int j = is + 1; j < n - 1; j++) {
			distmatrix(j, is) = distmatrix(n - 1, j);
		}

		// Merge clusters
		std::vector<int>& cluster = iclusters[js];
		cluster.reserve(cluster.size() + iclusters[is].size());  // preallocate memory
		cluster.insert(cluster.end(), iclusters[is].begin(), iclusters[is].end());
		iclusters[is] = iclusters[n - 1];
		iclusters.pop_back();
	}

	return iclusters;
}

} // namespace circuit
