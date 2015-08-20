/*
 * MetalData.h
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_METALDATA_H_
#define SRC_CIRCUIT_STATIC_METALDATA_H_

#include "AIFloat3.h"

#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/polygon/voronoi.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <vector>
#include <atomic>
#include <memory>
#include <set>

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

namespace circuit {

class CRagMatrix;

class CMetalData {
private:
	// Note: Pointtree is also a very pretty candidate for range searches.
	// Because map coordinates are big enough we can use only integer part.
	// @see https://github.com/Warzone2100/warzone2100/blob/master/src/pointtree.cpp
	using point = bg::model::point<float, 2, bg::cs::cartesian>;
	using box = bg::model::box<point>;
	using vor_point = boost::polygon::point_data<int>;
	using vor_diagram = boost::polygon::voronoi_diagram<double>;

public:
	struct SEdge {
		SEdge() : index(-1), weight(.0f) {}
		SEdge(int i, float w) : index(i), weight(w) {}
		int index;
		float weight;
		springai::AIFloat3 center;
	};
	using Graph = boost::adjacency_list<boost::hash_setS, boost::vecS, boost::undirectedS, boost::no_property, SEdge>;
	using VertexDesc = boost::graph_traits<Graph>::vertex_descriptor;
	using EdgeDesc = boost::graph_traits<Graph>::edge_descriptor;

	struct SMetal {
		float income;
		springai::AIFloat3 position;
	};
	using Metals = std::vector<SMetal>;
	using MetalNode = std::pair<point, int>;  // spots indexer
	using MetalPredicate = std::function<bool (MetalNode const& v)>;
	using MetalIndices = std::vector<int>;
	struct SCluster {
		MetalIndices idxSpots;
		springai::AIFloat3 geoCentr;
		springai::AIFloat3 weightCentr;
	};
	using Clusters = std::vector<SCluster>;

public:
	CMetalData();
	virtual ~CMetalData();
	void Init(const Metals& spots);

	bool IsInitialized();
	bool IsEmpty();

	bool IsClusterizing();
	void SetClusterizing(bool value);

	const Metals& GetSpots() const;
	const int FindNearestSpot(const springai::AIFloat3& pos) const;
	const int FindNearestSpot(const springai::AIFloat3& pos, MetalPredicate& predicate) const;
	const MetalIndices FindNearestSpots(const springai::AIFloat3& pos, int num) const;
	const MetalIndices FindNearestSpots(const springai::AIFloat3& pos, int num, MetalPredicate& predicate) const;
	const MetalIndices FindWithinDistanceSpots(const springai::AIFloat3& pos, float maxDistance) const;
	const MetalIndices FindWithinRangeSpots(const springai::AIFloat3& posFrom, const springai::AIFloat3& posTo) const;

	const int FindNearestCluster(const springai::AIFloat3& pos) const;
	const int FindNearestCluster(const springai::AIFloat3& pos, MetalPredicate& predicate) const;
	const MetalIndices FindNearestClusters(const springai::AIFloat3& pos, int num) const;
	const MetalIndices FindNearestClusters(const springai::AIFloat3& pos, int num, MetalPredicate& predicate) const;

	const CMetalData::Clusters& GetClusters() const;
	const CMetalData::Graph& GetGraph() const;

	/*
	 * Hierarchical clusterization. Not reusable. Metric: complete link. Thread-unsafe
	 */
	void Clusterize(float maxDistance, std::shared_ptr<CRagMatrix> distmatrix);

	// debug, could be used for defence perimeter calculation
//	void DrawConvexHulls(springai::Drawer* drawer);
//	void DrawCentroids(springai::Drawer* drawer);
//	void ClearMetalClusters(springai::Drawer* drawer);

	const SMetal& operator[](int idx) const;

private:
	bool initialized;
	Metals spots;
	// TODO: Find out more about bgi::rtree, bgi::linear, bgi::quadratic, bgi::rstar, packing algorithm?
	using MetalTree = bgi::rtree<MetalNode, bgi::rstar<16, 4>>;
	MetalTree metalTree;

	Clusters clusters;
	using ClusterTree = bgi::rtree<MetalNode, bgi::quadratic<16>>;
	ClusterTree clusterTree;

	vor_diagram clustVoronoi;  // TODO: Do not save?
	Graph clusterGraph;

	std::atomic<bool> isClusterizing;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_METALDATA_H_
