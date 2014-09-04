/*
 * MetalManager.cpp
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#include "MetalManager.h"

#include "Pathing.h"
#include "Drawer.h"

#include <functional>
#include <algorithm>

namespace circuit {

using namespace springai;

CMetalManager::CMetalManager(std::vector<Metal>& spots) :
		spots(spots)
{
}

CMetalManager::~CMetalManager()
{
}

bool CMetalManager::IsEmpty()
{
	return spots.empty();
}

std::vector<Metal>& CMetalManager::GetSpots()
{
	return spots;
}

void CMetalManager::Clusterize(float maxDistance, int pathType, Pathing* pathing)
{
	int nrows = spots.size();

	// Create distance matrix
	float** distmatrix = new float* [nrows];
	distmatrix[0] = nullptr;
	for (int i = 1; i < nrows; i++) {
		distmatrix[i] = new float [i];
		for (int j = 0; j < i; j++) {
			float lenStartEnd = pathing->GetApproximateLength(spots[i].position, spots[j].position, pathType, 0.0f);
			float lenEndStart = pathing->GetApproximateLength(spots[j].position, spots[i].position, pathType, 0.0f);
			distmatrix[i][j] = (lenStartEnd + lenEndStart) / 2.0f;
		}
	}

	// Initialize cluster-element list
	using Cluster = std::vector<int>;
	std::vector<Cluster> clusters(nrows);
	for (int i = 0; i < nrows; i++) {
		Cluster cluster;
		cluster.push_back(i);
		clusters[i] = cluster;
	}

	auto find_closest_pair = [](int n, float** distmatrix, int* ip, int* jp) -> float {
		float temp;
		float distance = distmatrix[1][0];
		*ip = 1;
		*jp = 0;
		for (int i = 1; i < n; i++) {
			for (int j = 0; j < i; j++) {
				temp = distmatrix[i][j];
				if (temp < distance) {
					distance = temp;
					*ip = i;
					*jp = j;
				}
			}
		}
		return distance;
	};

	for (int n = nrows; n > 1; n--) {
		// Find pair
		int is = 1;
		int js = 0;
		if (find_closest_pair(n, distmatrix, &is, &js) > maxDistance) {
			break;
		}

		// Fix the distances
		for (int j = 0; j < js; j++) {
			distmatrix[js][j] = std::max(distmatrix[is][j], distmatrix[js][j]);
		}
		for (int j = js + 1; j < is; j++) {
			distmatrix[j][js] = std::max(distmatrix[is][j], distmatrix[j][js]);
		}
		for (int j = is + 1; j < n; j++) {
			distmatrix[j][js] = std::max(distmatrix[j][is], distmatrix[j][js]);
		}

		for (int j = 0; j < is; j++) {
			distmatrix[is][j] = distmatrix[n - 1][j];
		}
		for (int j = is + 1; j < n - 1; j++) {
			distmatrix[j][is] = distmatrix[n - 1][j];
		}

		// Merge clusters
		Cluster& cluster = clusters[js];
		cluster.reserve(cluster.size() + clusters[is].size()); // preallocate memory
		cluster.insert(cluster.end(), clusters[is].begin(), clusters[is].end());
		clusters[is] = clusters[n-1];
		clusters.pop_back();
	}
	int nclusters = clusters.size();
	this->clusters.resize(nclusters);
	for (int i = 0; i < nclusters; i++) {
		for (int j = 0; j < clusters[i].size(); j++) {
			this->clusters[i].push_back(spots[clusters[i][j]]);
		}
	}

	for (int i = 1; i < nrows; i++) {
		delete[] distmatrix[i];
	}
	delete[] distmatrix;
}

void CMetalManager::DrawConvexHulls(Drawer* drawer)
{
	for (const std::vector<Metal>& vec : clusters) {
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
			std::function<float(const AIFloat3&, const AIFloat3&)> dist = [](const AIFloat3& p1, const AIFloat3& p2) -> float {
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
			auto compare = [&p0, orientation, dist](const AIFloat3& p1, const AIFloat3& p2) {
				// Find orientation
				int o = orientation(p0, p1, p2);
				if (o == 0) {
					return dist(p0, p1) < dist(p0, p2);
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
	for (auto& cluster : clusters) {
		for (auto& spot : cluster) {
			drawer->DeletePointsAndLines(spot.position);
		}
	}
	clusters.clear();

//	for (auto& centroid : centroids) {
//		drawer->DeletePointsAndLines(centroid);
//	}
//	centroids.clear();
}

} // namespace circuit
