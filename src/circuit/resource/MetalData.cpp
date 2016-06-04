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

// NOTE: micro-opt
static std::vector<CMetalData::MetalNode> result_n;

CMetalData::CMetalData()
		: initialized(false)
		, isClusterizing(false)
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
	result_n.reserve(num);
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
	result_n.reserve(num);
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
//	std::vector<MetalNode> result_n;
	clusterTree.query(bgi::nearest(point(pos.x, pos.z), 1), std::back_inserter(result_n));

	if (!result_n.empty()) {
		int result = result_n.front().second;
		result_n.clear();
		return result;
	}
	return -1;
}

const int CMetalData::FindNearestCluster(const AIFloat3& pos, MetalPredicate& predicate) const
{
//	std::vector<MetalNode> result_n;
	clusterTree.query(bgi::nearest(point(pos.x, pos.z), 1) && bgi::satisfies(predicate), std::back_inserter(result_n));

	if (!result_n.empty()) {
		int result = result_n.front().second;
		result_n.clear();
		return result;
	}
	return -1;
}

const CMetalData::MetalIndices CMetalData::FindNearestClusters(const AIFloat3& pos, int num) const
{
//	std::vector<MetalNode> result_n;
//	result_n.reserve(num);
	clusterTree.query(bgi::nearest(point(pos.x, pos.z), num), std::back_inserter(result_n));

	MetalIndices result;
	for (auto& node : result_n) {
		result.push_back(node.second);
	}
	result_n.clear();
	return result;
}

const CMetalData::MetalIndices CMetalData::FindNearestClusters(const AIFloat3& pos, int num, MetalPredicate& predicate) const
{
//	std::vector<MetalNode> result_n;
//	result_n.reserve(num);
	clusterTree.query(bgi::nearest(point(pos.x, pos.z), num) && bgi::satisfies(predicate), std::back_inserter(result_n));

	MetalIndices result;
	for (auto& node : result_n) {
		result.push_back(node.second);
	}
	result_n.clear();
	return result;
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
	clusterTree.clear();
	CEncloseCircle enclose;
	for (int i = 0; i < nclusters; ++i) {
		SCluster& c = clusters[i];
		c.idxSpots.clear();
		AIFloat3 centr = ZeroVector;
		std::vector<AIFloat3> points;
		points.reserve(iclusters[i].size());
		for (unsigned j = 0; j < iclusters[i].size(); ++j) {
			c.idxSpots.push_back(iclusters[i][j]);
			const AIFloat3& pos = spots[iclusters[i][j]].position;
			points.push_back(pos);
			centr += pos;
		}
		centr /= iclusters[i].size();
		c.weightCentr = centr;

		enclose.MakeCircle(points);
		c.geoCentr = enclose.GetCenter();
		c.geoCentr.y = centr.y;
		clusterTree.insert(std::make_pair(point(c.geoCentr.x, c.geoCentr.z), i));
	}

	// NOTE: C++ poly2tri (https://code.google.com/p/poly2tri/) is a nice candidate for triangulation,
	//       but boost has voronoi_diagram. Also for boost "The library allows users to implement their own
	//       Voronoi diagram / Delaunay triangulation construction routines based on the Voronoi builder API".
	// Build Voronoi diagram out of clusters
	std::vector<vor_point> vorPoints;
	vorPoints.reserve(nclusters);
	for (SCluster& c : clusters) {
		vorPoints.push_back(vor_point(c.geoCentr.x, c.geoCentr.z));
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
				float AB = clusters[A].geoCentr.distance(clusters[B].geoCentr);
				float BC = clusters[B].geoCentr.distance(clusters[C].geoCentr);
				float CA = clusters[C].geoCentr.distance(clusters[A].geoCentr);
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
			SEdge& edge = g[edgeId];
			edge.index = edgeIndex++;
			edge.weight = clusters[A].geoCentr.distance(clusters[B].geoCentr);
			edge.center = (clusters[A].geoCentr + clusters[B].geoCentr) * 0.5f;
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
