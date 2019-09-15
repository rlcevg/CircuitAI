/*
 * MetalData.cpp
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#include "resource/MetalData.h"
#include "util/math/HierarchCluster.h"
#include "util/math/EncloseCircle.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;
using namespace nanoflann;

CMetalData::CMetalData()
		: isInitialized(false)
		, spotsAdaptor(spots)
		, metalTree(2 /*dim*/, spotsAdaptor, KDTreeSingleIndexAdaptorParams(8 /*max leaf*/))
		, minIncome(std::numeric_limits<float>::max())
		, avgIncome(0.f)
		, maxIncome(0.f)
		, clustersAdaptor(clusters)
		, clusterTree(2 /*dim*/, clustersAdaptor, KDTreeSingleIndexAdaptorParams(2 /*max leaf*/))
		, isClusterizing(false)
{
}

CMetalData::~CMetalData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CMetalData::Init(const Metals& spots)
{
	this->spots = spots;
	for (auto& spot : spots) {
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

	KNNCondResultSet<float, int> resultSet(1, predicate);
	resultSet.init(&ret_index, &out_dist_sqr);

	if (metalTree.findNeighbors(resultSet, &query_pt[0], SearchParams())) {
		return ret_index;
	}
	return -1;
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

	KNNCondResultSet<float, int> resultSet(1, predicate);
	resultSet.init(&ret_index, &out_dist_sqr);

	if (clusterTree.findNeighbors(resultSet, &query_pt[0], SearchParams())) {
		return ret_index;
	}
	return -1;
}

void CMetalData::Clusterize(float maxDistance, std::shared_ptr<CRagMatrix> distMatrix)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);

	// Clusterize metal spots by distance to each other
	CHierarchCluster clust;
	const CHierarchCluster::Clusters& iclusters = clust.Clusterize(*distMatrix, maxDistance);

	// Fill cluster structures, calculate centers
	int nclusters = iclusters.size();
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
	}
	clusterTree.buildIndex();

	// NOTE: C++ poly2tri (https://code.google.com/p/poly2tri/) is a nice candidate for triangulation,
	//       but boost has voronoi_diagram. Also for boost "The library allows users to implement their own
	//       Voronoi diagram / Delaunay triangulation construction routines based on the Voronoi builder API".
	// Build Voronoi diagram out of clusters
	std::vector<vor_point> vorPoints;
	vorPoints.reserve(nclusters);
	for (SCluster& c : clusters) {
		vorPoints.push_back(vor_point(c.position.x, c.position.z));
	}
	clustVoronoi.clear();
	boost::polygon::construct_voronoi(vorPoints.begin(), vorPoints.end(), &clustVoronoi);

	// Convert voronoi_diagram to "Delaunay" in BGL (Boost Graph Library)
	std::map<std::size_t, std::set<std::size_t>> verts;
	Graph g(nclusters);
	unsigned edgeCount = 0;  // counts each edge twice
	std::vector<std::pair<std::size_t, std::size_t>> edges;
	edges.reserve(clustVoronoi.edges().size() / 2);
	for (auto& edge : clustVoronoi.edges()) {
		std::size_t idx0 = edge.cell()->source_index();
		std::size_t idx1 = edge.twin()->cell()->source_index();

		auto it = verts.find(idx0);
		if (it != verts.end()) {
			it->second.insert(idx1);
		} else {
			std::set<std::size_t> v;
			v.insert(idx1);
			verts[idx0] = v;
		}

		if ((edgeCount++ % 2 == 0)/* && edge.is_finite()*/) {  // FIXME: No docs says that odd edge is a twin of even
			edges.push_back(std::make_pair(idx0, idx1));
		}
	}
	auto badEdge = [&verts, this](std::size_t A, std::size_t B) {
		for (std::size_t C : verts[A]) {
			std::set<std::size_t>& vs = verts[C];
			if (vs.find(B) != vs.end()) {
				float AB = clusters[A].position.distance(clusters[B].position);
				float BC = clusters[B].position.distance(clusters[C].position);
				float CA = clusters[C].position.distance(clusters[A].position);
				if (AB > (BC + CA) * 0.9f) {
					return true;
				}
			}
		}
		return false;
	};
	int edgeIndex = 0;
	for (std::pair<std::size_t, std::size_t>& e : edges) {
		std::size_t A = e.first, B = e.second;
		if (badEdge(A, B)) {
			continue;
		}
		EdgeDesc edgeId;
		bool ok;
		std::tie(edgeId, ok) = boost::add_edge(A, B, g);
		if (ok) {
			const CMetalData::SCluster& clA = clusters[A];
			const CMetalData::SCluster& clB = clusters[B];
			SEdge& edge = g[edgeId];
			edge.index = edgeIndex++;
			edge.weight = clA.position.distance(clB.position) / (clA.income + clB.income) * (clA.idxSpots.size() + clB.idxSpots.size());
			edge.center = (clA.position + clB.position) * 0.5f;
		}
	}
	clusterGraph = g;

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

} // namespace circuit
