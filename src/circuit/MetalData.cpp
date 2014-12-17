/*
 * MetalData.cpp
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#include "MetalData.h"
#include "RagMatrix.h"
#include "utils.h"

#include <functional>
#include <algorithm>

namespace circuit {

using namespace springai;

CMetalData::CMetalData() :
		initialized(false),
		pclusters(&clusters0),
		pcentroids(&centroids0),
		isClusterizing(false)
{
}

CMetalData::~CMetalData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CMetalData::Init(const Metals& spots)
{
	if (initialized) {
		metalTree.clear();
	}

	this->spots = spots;
	int i = 0;
    for (auto& spot : spots) {
    	point p(spot.position.x, spot.position.z);
        metalTree.insert(std::make_pair(p, i++));
    }
    initialized = true;
}

bool CMetalData::IsInitialized()
{
	return initialized;
}

bool CMetalData::IsEmpty()
{
	return spots.empty();
}

bool CMetalData::IsClusterizing()
{
	return isClusterizing.load();
}

void CMetalData::SetClusterizing(bool value)
{
	isClusterizing = value;
}

const CMetalData::Metals& CMetalData::GetSpots() const
{
	return spots;
}

const int CMetalData::FindNearestSpot(const AIFloat3& pos) const
{
	std::vector<MetalNode> result_n;
	metalTree.query(bgi::nearest(point(pos.x, pos.z), 1), std::back_inserter(result_n));

	if (!result_n.empty()) {
		return result_n.front().second;
	}
	return -1;
}

const int CMetalData::FindNearestSpot(const AIFloat3& pos, MetalPredicate& predicate) const
{
	std::vector<MetalNode> result_n;
	metalTree.query(bgi::nearest(point(pos.x, pos.z), 1) && bgi::satisfies(predicate), std::back_inserter(result_n));

	if (!result_n.empty()) {
		return result_n.front().second;
	}
	return -1;
}

const CMetalData::MetalIndices CMetalData::FindNearestSpots(const AIFloat3& pos, int num) const
{
	std::vector<MetalNode> result_n;
	metalTree.query(bgi::nearest(point(pos.x, pos.z), num), std::back_inserter(result_n));

	MetalIndices result;
	for (auto& node : result_n) {
		result.push_back(node.second);
	}
	return result;
}

const CMetalData::MetalIndices CMetalData::FindNearestSpots(const AIFloat3& pos, int num, MetalPredicate& predicate) const
{
	std::vector<MetalNode> result_n;
	metalTree.query(bgi::nearest(point(pos.x, pos.z), num) && bgi::satisfies(predicate), std::back_inserter(result_n));

	MetalIndices result;
	for (auto& node : result_n) {
		result.push_back(node.second);
	}
	return result;
}

const CMetalData::MetalIndices CMetalData::FindWithinDistanceSpots(const AIFloat3& pos, float maxDistance) const
{
	std::vector<MetalNode> returned_values;
	point sought = point(pos.x, pos.z);
	box enc_box(point(pos.x - maxDistance, pos.z - maxDistance), point(pos.x + maxDistance, pos.z + maxDistance));
	auto predicate = [&maxDistance, &sought](MetalNode const& v) {
		return bg::distance(v.first, sought) < maxDistance;
	};
	metalTree.query(bgi::within(enc_box) && bgi::satisfies(predicate), std::back_inserter(returned_values));

	MetalIndices result;
	for (auto& node : returned_values) {
		result.push_back(node.second);
	}
	return result;
}

const CMetalData::MetalIndices CMetalData::FindWithinRangeSpots(const AIFloat3& posFrom, const AIFloat3& posTo) const
{
	box query_box(point(posFrom.x, posFrom.z), point(posTo.x, posTo.z));
	std::vector<MetalNode> result_s;
	metalTree.query(bgi::within(query_box), std::back_inserter(result_s));

	MetalIndices result;
	for (auto& node : result_s) {
		result.push_back(node.second);
	}
	return result;
}

const int CMetalData::FindNearestCluster(const AIFloat3& pos) const
{
	std::vector<MetalNode> result_n;
	pclusterTree.load()->query(bgi::nearest(point(pos.x, pos.z), 1), std::back_inserter(result_n));

	if (!result_n.empty()) {
		return result_n.front().second;
	}
	return -1;
}

const int CMetalData::FindNearestCluster(const AIFloat3& pos, MetalPredicate& predicate) const
{
	std::vector<MetalNode> result_n;
	pclusterTree.load()->query(bgi::nearest(point(pos.x, pos.z), 1) && bgi::satisfies(predicate), std::back_inserter(result_n));

	if (!result_n.empty()) {
		return result_n.front().second;
	}
	return -1;
}

const CMetalData::MetalIndices CMetalData::FindNearestClusters(const AIFloat3& pos, int num, MetalPredicate& predicate) const
{
	std::vector<MetalNode> result_n;
	pclusterTree.load()->query(bgi::nearest(point(pos.x, pos.z), num) && bgi::satisfies(predicate), std::back_inserter(result_n));

	MetalIndices result;
	for (auto& node : result_n) {
		result.push_back(node.second);
	}
	return result;
}

void CMetalData::ClusterLock()
{
	clusterMutex.lock();
}

void CMetalData::ClusterUnlock()
{
	clusterMutex.unlock();
}

const std::vector<CMetalData::MetalIndices>& CMetalData::GetClusters() const
{
	return *pclusters.load();
}

const std::vector<AIFloat3>& CMetalData::GetCentroids() const
{
	return *pcentroids.load();
}

const std::vector<AIFloat3>& CMetalData::GetCostCentroids() const
{
	return *pcostCentroids.load();
}

void CMetalData::Clusterize(float maxDistance, std::shared_ptr<CRagMatrix> distMatrix)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	CRagMatrix& distmatrix = *distMatrix;
	int nrows = distmatrix.GetNrows();

	// Initialize cluster-element list
	std::vector<MetalIndices> iclusters;
	iclusters.reserve(nrows);
	for (int i = 0; i < nrows; i++) {
		MetalIndices cluster;
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
		MetalIndices& cluster = iclusters[js];
		cluster.reserve(cluster.size() + iclusters[is].size()); // preallocate memory
		cluster.insert(cluster.end(), iclusters[is].begin(), iclusters[is].end());
		iclusters[is] = iclusters[n - 1];
		iclusters.pop_back();
	}

	auto findCentroid = [this](MetalIndices& cluster) {
		if (cluster.size() == 1) {
			return spots[cluster[0]].position;
		}
		float distSq = spots[cluster[1]].position.SqDistance2D(spots[cluster[0]].position);
		int ir = 1, jr = 0;
		for (int i = 0; i < cluster.size(); i++) {
			for (int j = 0; j < cluster.size(); j++) {
				float temp = spots[cluster[i]].position.SqDistance2D(spots[cluster[j]].position);
				if (temp > distSq) {
					distSq = temp;
					ir = i;
					jr = j;
				}
			}
		}
		AIFloat3 pos = (spots[cluster[ir]].position + spots[cluster[jr]].position) / 2;
		return pos;
	};

	std::vector<MetalIndices>& clusters = (pclusters == &clusters0) ? clusters1 : clusters0;
	std::vector<AIFloat3>& centroids = (pcentroids == &centroids0) ? centroids1 : centroids0;
	std::vector<AIFloat3>& costCentroids = (pcostCentroids == &costCentroids0) ? costCentroids1 : costCentroids0;
	ClusterTree& clusterTree = (pclusterTree.load() == &clusterTree0) ? clusterTree1 : clusterTree0;
	int nclusters = iclusters.size();
	clusters.resize(nclusters);
	centroids.resize(nclusters);
	costCentroids.resize(nclusters);
	clusterTree.clear();
	for (int i = 0; i < nclusters; i++) {
		clusters[i].clear();
		AIFloat3 centr = ZeroVector;
		for (int j = 0; j < iclusters[i].size(); j++) {
			clusters[i].push_back(iclusters[i][j]);
			centr += spots[iclusters[i][j]].position;
		}
		centr /= iclusters[i].size();
		costCentroids[i] = centr;
        clusterTree.insert(std::make_pair(point(centr.x, centr.z), i));
        centroids[i] = findCentroid(clusters[i]);
	}

	{
//		std::lock_guard<std::mutex> guard(clusterMutex);
		clusterMutex.lock();
		pclusters = &clusters;
		pcentroids = &centroids;
		pcostCentroids = &costCentroids;
		pclusterTree = &clusterTree;
		clusterMutex.unlock();
	}

	isClusterizing = false;
}

//void CMetalData::DrawConvexHulls(Drawer* drawer)
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

//void CMetalData::ClearMetalClusters(Drawer* drawer)
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

const CMetalData::Metal& CMetalData::operator[](int idx) const
{
	return spots[idx];
}

} // namespace circuit
