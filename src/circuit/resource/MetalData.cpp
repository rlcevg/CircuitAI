/*
 * MetalData.cpp
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#include "resource/MetalData.h"
#include "util/math/HierarchCluster.h"
#include "util/math/EncloseCircle.h"
#include "util/Utils.h"
#include "triangulate/delaunator.hpp"

#include <map>

namespace circuit {

using namespace springai;
using namespace nanoflann;

CMetalData::CMetalData()
		: isInitialized(false)
		, spotsAdaptor(spots)
		, metalTree(2 /*dim*/, spotsAdaptor, KDTreeSingleIndexAdaptorParams(4 /*max leaf*/))
		, minIncome(std::numeric_limits<float>::max())
		, avgIncome(0.f)
		, maxIncome(0.f)
		, clustersAdaptor(clusters)
		, clusterTree(2 /*dim*/, clustersAdaptor, KDTreeSingleIndexAdaptorParams(2 /*max leaf*/))
		, clusterEdgeCosts(clusterGraph)
		, isClusterizing(false)
{
}

CMetalData::~CMetalData()
{
}

void CMetalData::Init(const Metals& spots)
{
	this->spots = spots;
	for (const SMetal& spot : spots) {
		minIncome = std::min(minIncome, spot.income);
		maxIncome = std::max(maxIncome, spot.income);
		avgIncome += spot.income;
	}
	if (!spots.empty()) {
		avgIncome /= spots.size();
	}
	metalTree.buildIndex();

	isInitialized = true;
}

const int CMetalData::FindNearestSpot(const AIFloat3& pos) const
{
	float query_pt[2] = {pos.x, pos.z};
	int ret_index;
	float out_dist_sqr;

	if (metalTree.knnSearch(&query_pt[0], 1, &ret_index, &out_dist_sqr) > 0) {
		return ret_index;
	}
	return -1;
}

const int CMetalData::FindNearestSpot(const AIFloat3& pos, PointPredicate& predicate) const
{
	float query_pt[2] = {pos.x, pos.z};
	int ret_index;
	float out_dist_sqr;

	if (metalTree.knnSearch(&query_pt[0], 1, &ret_index, &out_dist_sqr, predicate) > 0) {
		return ret_index;
	}
	return -1;
}

void CMetalData::FindSpotsInRadius(const AIFloat3& pos, const float radius,
		CMetalData::IndicesDists& outIndices) const
{
	MetalIndices result;

	float query_pt[2] = {pos.x, pos.z};
	nanoflann::SearchParams searchParams;
	searchParams.sorted = false;

	metalTree.radiusSearch(&query_pt[0], radius, outIndices, searchParams);
}

const int CMetalData::FindNearestCluster(const AIFloat3& pos) const
{
	float query_pt[2] = {pos.x, pos.z};
	int ret_index;
	float out_dist_sqr;

	if (clusterTree.knnSearch(&query_pt[0], 1, &ret_index, &out_dist_sqr) > 0) {
		return ret_index;
	}
	return -1;
}

const int CMetalData::FindNearestCluster(const AIFloat3& pos, PointPredicate& predicate) const
{
	float query_pt[2] = {pos.x, pos.z};
	int ret_index;
	float out_dist_sqr;

	if (clusterTree.knnSearch(&query_pt[0], 1, &ret_index, &out_dist_sqr, predicate) > 0) {
		return ret_index;
	}
	return -1;
}

void CMetalData::Clusterize(float maxDistance, CRagMatrix& distMatrix)
{
	// Clusterize metal spots by distance to each other
	CHierarchCluster clust;
	const CHierarchCluster::Clusters& iclusters = clust.Clusterize(distMatrix, maxDistance);

	// Fill cluster structures, calculate centers
	const int nclusters = iclusters.size();
	clusterGraph.clear();
	clusterGraph.reserveNode(nclusters);
	clusters.resize(nclusters);
	CEncloseCircle enclose;
	for (int i = 0; i < nclusters; ++i) {
		SCluster& c = clusters[i];
		c.income = 0.f;
		c.idxSpots.clear();
		AIFloat3 centr = ZeroVector;
		std::vector<AIFloat3> points;
		points.reserve(iclusters[i].size());
		for (unsigned j = 0; j < iclusters[i].size(); ++j) {
			c.idxSpots.push_back(iclusters[i][j]);
			const SMetal& spot = spots[iclusters[i][j]];
			points.push_back(spot.position);
			centr += spot.position;
			c.income += spot.income;
		}
		centr /= iclusters[i].size();
		c.weightCentr = centr;

		enclose.MakeCircle(points);
		c.position = enclose.GetCenter();
		c.position.y = centr.y;
		c.radius = enclose.GetRadius();

		clusterGraph.addNode();
	}
	clusterTree.buildIndex();

	BuildClusterGraph();

	isClusterizing = false;
}

void CMetalData::TriangulateGraph(const std::vector<double>& coords,
		std::function<float (std::size_t A, std::size_t B)> distance,
		std::function<void (std::size_t A, std::size_t B)> addEdge)
{
	try {
		delaunator::Delaunator d(coords);
		using DEdge = std::pair<std::size_t, std::size_t>;
		std::map<DEdge, std::set<std::size_t>> edges;
		auto adjacent = [&edges](std::size_t A, std::size_t B, std::size_t C) {
			if (A > B) {  // undirected edges (i < j)
				std::swap(A, B);
			}
			edges[std::make_pair(A, B)].insert(C);
		};
		for (std::size_t i = 0; i < d.triangles.size(); i += 3) {
			const std::size_t A = d.triangles[i + 0];
			const std::size_t B = d.triangles[i + 1];
			const std::size_t C = d.triangles[i + 2];
			adjacent(A, B, C);
			adjacent(B, C, A);
			adjacent(C, A, B);
		}
		auto badEdge = [&edges, distance](const DEdge e, const std::set<std::size_t>& vs) {
			float AB = distance(e.first, e.second);
			for (std::size_t C : vs) {
				float AC = distance(e.first, C);
				float BC = distance(e.second, C);
				if (AB > (BC + AC) * 0.9f) {
					return true;
				}
			}
			return false;
		};
		for (auto kv : edges) {
			const DEdge& e = kv.first;
			const std::set<std::size_t>& vs = kv.second;
			if (badEdge(e, vs)) {
				continue;
			}
			addEdge(e.first, e.second);
		}
	} catch (...) {
		MakeConvexHull(coords, addEdge);
	}
}

void CMetalData::MakeConvexHull(const std::vector<double>& coords,
		std::function<void (std::size_t A, std::size_t B)> addEdge)
{
	// !!! Graham scan !!!
	// Coord system:  *-----x
	//                |
	//                |
	//                z
	struct Point {
		size_t id;
		double x, z;
	};
	auto orientation = [](const Point& p1, const Point& p2, const Point& p3) {
		// orientation > 0 : counter-clockwise turn,
		// orientation < 0 : clockwise,
		// orientation = 0 : collinear
		return (p2.x - p1.x) * (p3.z - p1.z) - (p2.z - p1.z) * (p3.x - p1.x);
	};
	// number of points
	size_t N = coords.size() / 2;
	// the array of points
	std::vector<Point> points;
	points.reserve(N + 1);
	points.push_back({});  // sentinel
	// Find the bottom-most point
	size_t min = 1;
	double zmin = coords[1];
	for (size_t i = 0; i < N; ++i) {
		points.push_back({i, coords[i * 2 + 0], coords[i * 2 + 1]});
		float z = coords[i * 2 + 1];
		// Pick the bottom-most or chose the left most point in case of tie
		if ((z < zmin) || (zmin == z && points[i + 1].x < points[min].x)) {
			zmin = z, min = i + 1;
		}
	}
	std::swap(points[1], points[min]);

	// A function used to sort an array of
	// points with respect to the first point
	Point& p0 = points[1];
	auto compare = [&p0, orientation](const Point& p1, const Point& p2) {
		// Find orientation
		int o = orientation(p0, p1, p2);
		if (o == 0) {
			double sqDist1 = SQUARE(p1.x - p0.x) + SQUARE(p1.z - p0.z);
			double sqDist2 = SQUARE(p2.z - p0.x) + SQUARE(p2.z - p0.z);
			return sqDist1 < sqDist2;
		}
		return o > 0;
	};
	// Sort n-1 points with respect to the first point. A point p1 comes
	// before p2 in sorted output if p2 has larger polar angle (in
	// counterclockwise direction) than p1
	std::sort(points.begin() + 2, points.end(), compare);

	// let points[0] be a sentinel point that will stop the loop
	points[0] = points[N];

//	size_t M = 1; // Number of points on the convex hull.
//	for (int i(2); i <= N; ++i) {
//		while (orientation(points[M - 1], points[M], points[i]) <= 0) {
//			if (M > 1) {
//				M--;
//			} else if (i == N) {
//				break;
//			} else {
//				i++;
//			}
//		}
//		std::swap(points[++M], points[i]);
//	}
	size_t M = N;  // FIXME: Remove. Hull through all points

	// draw convex hull
	size_t start = points[0].id, end;
	for (size_t i = 1; i < M; ++i) {
		end = points[i].id;
		addEdge(start, end);
		start = end;
	}
//	end = points[0].id;
//	addEdge(start, end);
}

void CMetalData::BuildClusterGraph()
{
	auto addEdge = [this](std::size_t A, std::size_t B) -> void {
		ClusterGraph::Edge edge = clusterGraph.addEdge(clusterGraph.nodeFromId(A), clusterGraph.nodeFromId(B));
		const SCluster& clA = clusters[A];
		const SCluster& clB = clusters[B];
		clusterEdgeCosts[edge] = clA.position.distance(clB.position) / (clA.income + clB.income) * (clA.idxSpots.size() + clB.idxSpots.size());
	};
	if (clusters.size() < 2) {
		// do nothing
	} else if (clusters.size() < 3) {
		addEdge(0, 1);
	} else {
		// Clusters triangulation
		std::vector<double> coords;
		coords.reserve(clusters.size() * 2);  // 2D
		for (SCluster& c : clusters) {
			coords.push_back(c.position.x);
			coords.push_back(c.position.z);
		}
		TriangulateGraph(coords, [this](std::size_t A, std::size_t B) -> float {
			return clusters[A].position.distance(clusters[B].position);
		}, addEdge);
	}
}

} // namespace circuit
