/*
 * MetalManager.cpp
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#include "MetalManager.h"
#include "RagMatrix.h"
#include "utils.h"

#include "Map.h"
#include "Drawer.h"

#include <functional>
#include <algorithm>

namespace circuit {

using namespace springai;

CMetalManager::CMetalManager(std::vector<Metal>& spots) :
		spots(spots),
		pclusters(&clusters0),
		distMatrix(nullptr),
		isClusterizing(false)
{
	unsigned i = 0;
    for (auto& spot : spots) {
    	point p(spot.position.x, spot.position.z);
        metalTree.insert(std::make_pair(p, i++));
    }
}

CMetalManager::~CMetalManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CMetalManager::IsEmpty()
{
	return spots.empty();
}

bool CMetalManager::IsClusterizing()
{
	return isClusterizing.load();
}

void CMetalManager::SetClusterizing(bool value)
{
	isClusterizing = value;
}

const CMetalManager::Metal CMetalManager::FindNearestSpot(AIFloat3& pos) const
{
	std::vector<MetalNode> result_n;
	metalTree.query(bgi::nearest(point(pos.x, pos.z), 1), std::back_inserter(result_n));

	if (!result_n.empty()) {
		return spots[result_n.front().second];
	}
	Metal spot;
	spot.position = -RgtVector;
	return spot;
}

const CMetalManager::Metal CMetalManager::FindNearestSpot(AIFloat3& pos, MetalPredicate& predicate) const
{
	std::vector<MetalNode> result_n;
	metalTree.query(bgi::nearest(point(pos.x, pos.z), 1) && bgi::satisfies(predicate), std::back_inserter(result_n));

	if (!result_n.empty()) {
		return spots[result_n.front().second];
	}
	Metal spot;
	spot.position = -RgtVector;
	return spot;
}

const CMetalManager::Metals CMetalManager::FindNearestSpots(AIFloat3& pos, int num) const
{
	Metals result;

	std::vector<MetalNode> result_n;
	metalTree.query(bgi::nearest(point(pos.x, pos.z), num), std::back_inserter(result_n));

	for (auto& node : result_n) {
		result.push_back(spots[node.second]);
	}
	return result;
}

const CMetalManager::Metals CMetalManager::FindWithinDistanceSpots(AIFloat3& pos, float maxDistance) const
{
	Metals result;

	std::vector<MetalNode> returned_values;
	point sought = point(pos.x, pos.z);
	box enc_box(point(pos.x - maxDistance, pos.z - maxDistance), point(pos.x + maxDistance, pos.z + maxDistance));
	auto predicate = [&maxDistance, &sought](MetalNode const& v) {
		return bg::distance(v.first, sought) < maxDistance;
	};
	metalTree.query(bgi::within(enc_box) && bgi::satisfies(predicate), std::back_inserter(returned_values));

	for (auto& node : returned_values) {
		result.push_back(spots[node.second]);
	}
	return result;
}

const CMetalManager::Metals CMetalManager::FindWithinRangeSpots(AIFloat3& posFrom, AIFloat3& posTo) const
{
	Metals result;

	box query_box(point(posFrom.x, posFrom.z), point(posTo.x, posTo.z));
	std::vector<MetalNode> result_s;
	metalTree.query(bgi::within(query_box), std::back_inserter(result_s));

	for (auto& node : result_s) {
		result.push_back(spots[node.second]);
	}
	return result;
}

const CMetalManager::Metals& CMetalManager::GetSpots() const
{
	return spots;
}

const std::vector<CMetalManager::Metals>& CMetalManager::GetClusters()
{
	clusterMutex.lock();
	std::vector<Metals>& rclusters = *pclusters;
	clusterMutex.unlock();
	return rclusters;
}

void CMetalManager::SetDistMatrix(CRagMatrix& distmatrix)
{
	distMatrix = std::make_shared<CRagMatrix>(distmatrix);
}

void CMetalManager::Clusterize(float maxDistance, std::shared_ptr<CRagMatrix> distMatrix)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	std::vector<Metals>& clusters = (pclusters == &clusters0) ? clusters1 : clusters0;
	CRagMatrix& distmatrix = *distMatrix;
	int nrows = distmatrix.GetNrows();

	// Initialize cluster-element list
	using Cluster = std::vector<int>;
	std::vector<Cluster> iclusters(nrows);
	for (int i = 0; i < nrows; i++) {
		Cluster cluster;
		cluster.push_back(i);
		iclusters[i] = cluster;
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
		Cluster& cluster = iclusters[js];
		cluster.reserve(cluster.size() + iclusters[is].size()); // preallocate memory
		cluster.insert(cluster.end(), iclusters[is].begin(), iclusters[is].end());
		iclusters[is] = iclusters[n - 1];
		iclusters.pop_back();
	}

	// TODO: Find more about std::vector::emplace
	int nclusters = iclusters.size();
	clusters.resize(nclusters);
	for (int i = 0; i < nclusters; i++) {
		clusters[i].clear();
		for (int j = 0; j < iclusters[i].size(); j++) {
			clusters[i].push_back(spots[iclusters[i][j]]);
		}
	}
	{
//		std::lock_guard<std::mutex> guard(clusterMutex);
		clusterMutex.lock();
		pclusters = &clusters;
		clusterMutex.unlock();
	}

	isClusterizing = false;
}

void CMetalManager::DrawConvexHulls(Drawer* drawer)
{
	for (const std::vector<Metal>& vec : GetClusters()) {
		if (vec.empty()) {
			continue;
		} else if (vec.size() == 1) {
			drawer->AddPoint(vec[0].position, "Cluster 1");
		} else if (vec.size() == 2) {
			drawer->AddLine(vec[0].position, vec[1].position);
		} else {
			// !!! Graham scan !!!
			// Coord system:  *-----x
			//                |
			//                |
			//                z
			auto orientation = [](const AIFloat3& p1, const AIFloat3& p2, const AIFloat3& p3) {
				// orientation > 0 : counter-clockwise turn,
				// orientation < 0 : clockwise,
				// orientation = 0 : collinear
				return (p2.x - p1.x) * (p3.z - p1.z) - (p2.z - p1.z) * (p3.x - p1.x);
			};
			std::function<float(const AIFloat3&, const AIFloat3&)> qdist = [](const AIFloat3& p1, const AIFloat3& p2) -> float {
				float x = p1.x - p2.x;
				float z = p1.z - p2.z;
				return x * x + z * z;
			};
			// number of points
			int N = vec.size();
			// the array of points
			std::vector<AIFloat3> points(N + 1);
			// Find the bottom-most point
			int min = 1, i = 1;
			float zmin = vec[0].position.z;
			for (const Metal& spot : vec) {
				points[i] = spot.position;
				float z = spot.position.z;
				// Pick the bottom-most or chose the left most point in case of tie
				if ((z < zmin) || (zmin == z && points[i].x < points[min].x)) {
					zmin = z, min = i;
				}
				i++;
			}
			auto swap = [](AIFloat3& p1, AIFloat3& p2) {
				AIFloat3 tmp = p1;
				p1 = p2;
				p2 = tmp;
			};
			swap(points[1], points[min]);

			// A function used to sort an array of
			// points with respect to the first point
			AIFloat3& p0 = points[1];
			auto compare = [&p0, orientation, qdist](const AIFloat3& p1, const AIFloat3& p2) {
				// Find orientation
				int o = orientation(p0, p1, p2);
				if (o == 0) {
					return qdist(p0, p1) < qdist(p0, p2);
				}
				return o > 0;
			};
			// Sort n-1 points with respect to the first point. A point p1 comes
			// before p2 in sorted output if p2 has larger polar angle (in
			// counterclockwise direction) than p1
			std::sort(points.begin() + 2, points.end(), compare);

			// let points[0] be a sentinel point that will stop the loop
			points[0] = points[N];

//			int M = 1; // Number of points on the convex hull.
//			for (int i(2); i <= N; ++i) {
//				while (orientation(points[M - 1], points[M], points[i]) <= 0) {
//					if (M > 1) {
//						M--;
//					} else if (i == N) {
//						break;
//					} else {
//						i++;
//					}
//				}
//				swap(points[++M], points[i]);
//			}

			// FIXME: Remove next DEBUG line
			int M = N;
			// draw convex hull
			AIFloat3 start = points[0], end;
			for (int i = 1; i < M; i++) {
				end = points[i];
				drawer->AddLine(start, end);
				start = end;
			}
			end = points[0];
			drawer->AddLine(start, end);
		}
	}
}

//void CMetalManager::DrawCentroids(Drawer* drawer)
//{
//	for (int i = 0; i < metalCluster.size(); i++) {
//		std::string msgText = utils::string_format("%i mexes cluster", metalCluster[i].size());
//		drawer->AddPoint(centroids[i], msgText.c_str());
//	}
//}

void CMetalManager::ClearMetalClusters(Drawer* drawer)
{
	for (auto& cluster : GetClusters()) {
		for (auto& spot : cluster) {
			drawer->DeletePointsAndLines(spot.position);
		}
	}
//	clusters.clear();
//
//	for (auto& centroid : centroids) {
//		drawer->DeletePointsAndLines(centroid);
//	}
//	centroids.clear();
}

const CMetalManager::Metal& CMetalManager::operator[](int idx) const
{
	return spots[idx];
}

} // namespace circuit
