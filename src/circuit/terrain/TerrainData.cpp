/*
 * TerrainData.cpp
 *
 *  Created on: Dec 15, 2014
 *      Author: rlcevg
 */

#include "terrain/TerrainData.h"
#include "util/RagMatrix.h"
#include "util/utils.h"

#include <functional>
#include <algorithm>

// debug
#include "Map.h"
#include "Drawer.h"
#include "CircuitAI.h"

namespace circuit {

using namespace springai;

CTerrainData::CTerrainData() :
		ppoints(&points0),
		isClusterizing(false)
{
}

CTerrainData::~CTerrainData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CTerrainData::IsClusterizing()
{
	return isClusterizing.load();
}

void CTerrainData::SetClusterizing(bool value)
{
	isClusterizing = value;
}

void CTerrainData::ClusterLock()
{
	clusterMutex.lock();
}

void CTerrainData::ClusterUnlock()
{
	clusterMutex.unlock();
}

const std::vector<springai::AIFloat3>& CTerrainData::GetDefencePoints() const
{
	return *ppoints.load();
}

const std::vector<springai::AIFloat3>& CTerrainData::GetDefencePerimeter() const
{
	return *ppoints.load();
}

void CTerrainData::Clusterize(const std::vector<springai::AIFloat3>& wayPoints, float maxDistance, CCircuitAI* circuit)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	int nrows = wayPoints.size();
	CRagMatrix distmatrix(nrows);
	for (int i = 1; i < nrows; i++) {
		for (int j = 0; j < i; j++) {
			float dx = wayPoints[i].x - wayPoints[j].x;
			float dz = wayPoints[i].z - wayPoints[j].z;
			distmatrix(i, j) = dx * dx + dz * dz;
		}
	}

	// Initialize cluster-element list
	std::vector<std::vector<int>> iclusters;
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
		cluster.reserve(cluster.size() + iclusters[is].size()); // preallocate memory
		cluster.insert(cluster.end(), iclusters[is].begin(), iclusters[is].end());
		iclusters[is] = iclusters[n - 1];
		iclusters.pop_back();
	}

	std::vector<std::vector<int>> clusters;
	std::vector<AIFloat3> centroids;
	int nclusters = iclusters.size();
	clusters.resize(nclusters);
	centroids.resize(nclusters);
	for (int i = 0; i < nclusters; i++) {
		clusters[i].clear();
		AIFloat3 centr = ZeroVector;
		for (int j = 0; j < iclusters[i].size(); j++) {
			clusters[i].push_back(iclusters[i][j]);
			centr += wayPoints[iclusters[i][j]];
		}
		centr /= iclusters[i].size();
		centroids[i] = centr;
	}

	printf("nclusters: %i\n", nclusters);
	for (int i = 0; i < clusters.size(); i++) {
		printf("%i | ", clusters[i].size());
	}
	std::sort(clusters.begin(), clusters.end(), [](const std::vector<int>& a, const std::vector<int>& b){ return a.size() > b.size(); });
	int num = centroids.size();
	Drawer* drawer = circuit->GetMap()->GetDrawer();
	for (int i = 0; i < num; i++) {
		AIFloat3 centr = ZeroVector;
		for (int j = 0; j < clusters[i].size(); j++) {
			centr += wayPoints[clusters[i][j]];
		}
		centr /= clusters[i].size();
		drawer->AddPoint(centr, utils::string_format("%i", i).c_str());
	}
	delete drawer;

	isClusterizing = false;
}

//void CTerrainData::DrawConvexHulls(Drawer* drawer)
//{
//	for (const MetalIndices& indices : GetClusters()) {
//		if (indices.empty()) {
//			continue;
//		} else if (indices.size() == 1) {
//			drawer->AddPoint(spots[indices[0]].position, "Cluster 1");
//		} else if (indices.size() == 2) {
//			drawer->AddLine(spots[indices[0]].position, spots[indices[1]].position);
//		} else {
//			// !!! Graham scan !!!
//			// Coord system:  *-----x
//			//                |
//			//                |
//			//                z
//			auto orientation = [](const AIFloat3& p1, const AIFloat3& p2, const AIFloat3& p3) {
//				// orientation > 0 : counter-clockwise turn,
//				// orientation < 0 : clockwise,
//				// orientation = 0 : collinear
//				return (p2.x - p1.x) * (p3.z - p1.z) - (p2.z - p1.z) * (p3.x - p1.x);
//			};
//			// number of points
//			int N = indices.size();
//			// the array of points
//			std::vector<AIFloat3> points(N + 1);
//			// Find the bottom-most point
//			int min = 1, i = 1;
//			float zmin = spots[indices[0]].position.z;
//			for (const int idx : indices) {
//				points[i] = spots[idx].position;
//				float z = spots[idx].position.z;
//				// Pick the bottom-most or chose the left most point in case of tie
//				if ((z < zmin) || (zmin == z && points[i].x < points[min].x)) {
//					zmin = z, min = i;
//				}
//				i++;
//			}
//			auto swap = [](AIFloat3& p1, AIFloat3& p2) {
//				AIFloat3 tmp = p1;
//				p1 = p2;
//				p2 = tmp;
//			};
//			swap(points[1], points[min]);
//
//			// A function used to sort an array of
//			// points with respect to the first point
//			AIFloat3& p0 = points[1];
//			auto compare = [&p0, orientation](const AIFloat3& p1, const AIFloat3& p2) {
//				// Find orientation
//				int o = orientation(p0, p1, p2);
//				if (o == 0) {
//					return p0.SqDistance2D(p1) < p0.SqDistance2D(p2);
//				}
//				return o > 0;
//			};
//			// Sort n-1 points with respect to the first point. A point p1 comes
//			// before p2 in sorted output if p2 has larger polar angle (in
//			// counterclockwise direction) than p1
//			std::sort(points.begin() + 2, points.end(), compare);
//
//			// let points[0] be a sentinel point that will stop the loop
//			points[0] = points[N];
//
////			int M = 1; // Number of points on the convex hull.
////			for (int i(2); i <= N; ++i) {
////				while (orientation(points[M - 1], points[M], points[i]) <= 0) {
////					if (M > 1) {
////						M--;
////					} else if (i == N) {
////						break;
////					} else {
////						i++;
////					}
////				}
////				swap(points[++M], points[i]);
////			}
//
//			// FIXME: Remove next DEBUG line
//			int M = N;
//			// draw convex hull
//			AIFloat3 start = points[0], end;
//			for (int i = 1; i < M; i++) {
//				end = points[i];
//				drawer->AddLine(start, end);
//				start = end;
//			}
//			end = points[0];
//			drawer->AddLine(start, end);
//		}
//	}
//}

//void CMetalManager::DrawCentroids(Drawer* drawer)
//{
//	for (int i = 0; i < metalCluster.size(); i++) {
//		std::string msgText = utils::string_format("%i mexes cluster", metalCluster[i].size());
//		drawer->AddPoint(centroids[i], msgText.c_str());
//	}
//}

//void CTerrainData::ClearMetalClusters(Drawer* drawer)
//{
//	for (auto& cluster : GetClusters()) {
//		for (auto& idx : cluster) {
//			drawer->DeletePointsAndLines(spots[idx].position);
//		}
//	}
////	clusters.clear();
////
////	for (auto& centroid : centroids) {
////		drawer->DeletePointsAndLines(centroid);
////	}
////	centroids.clear();
//}

} // namespace circuit
