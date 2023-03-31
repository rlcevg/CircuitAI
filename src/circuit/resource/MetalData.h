/*
 * MetalData.h
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_METALDATA_H_
#define SRC_CIRCUIT_STATIC_METALDATA_H_

#include "map/GridAnalyzer.h"
#include "util/Defines.h"
#include "util/math/Geometry.h"
#include "kdtree/nanoflann.hpp"
#include "lemon/smart_graph.h"

#include "AIFloat3.h"

#include <vector>
#include <atomic>
#include <memory>
#include <set>

namespace springai {
	class Resource;
}

namespace circuit {

class CCircuitAI;
class CMap;

template <class T> class CRagMatrix;

class CMetalData final: public bwem::IGrid {
public:
	using ClusterGraph = lemon::SmartGraph;
	using ClusterCostMap = ClusterGraph::EdgeMap<float>;

	struct SMetal {
		float income;
		springai::AIFloat3 position;
	};
	using Metals = std::vector<SMetal>;
	using PointPredicate = nanoflann::KNNCondResultSet<float, int>::Predicate;
	using MetalIndices = std::vector<int>;
	using IndicesDists = std::vector<std::pair<int, float>>;
	struct SCluster {
		MetalIndices idxSpots;
		springai::AIFloat3 position;  // geoCentr
		springai::AIFloat3 weightCentr;
		float income;
		float radius;
	};
	using Clusters = std::vector<SCluster>;

public:
	CMetalData();
	~CMetalData();
	void Init(const Metals&& spots);

	bool IsInitialized() const { return isInitialized; }
	bool IsEmpty() const { return spots.empty(); }

	bool IsClusterizing() const { return isClusterizing.load(); }
	void SetClusterizing(bool value) { isClusterizing = value; }

	float GetSpotMinIncome() const { return spotMinIncome; }
	float GetSpotAvgIncome() const { return spotAvgIncome; }
	float GetSpotMaxIncome() const { return spotMaxIncome; }
	float GetSpotStdDeviation() const { return spotStdDeviation; }

	float GetClusterMinIncome() const { return clusterMinIncome; }
	float GetClusterAvgIncome() const { return clusterAvgIncome; }
	float GetClusterMaxIncome() const { return clusterMaxIncome; }
	float GetClusterStdDeviation() const { return clusterStdDeviation; }

	const Metals& GetSpots() const { return spots; }

	const int FindNearestSpot(const springai::AIFloat3& pos) const;
	const int FindNearestSpot(const springai::AIFloat3& pos, PointPredicate& predicate) const;
	void FindSpotsInRadius(const springai::AIFloat3& pos, const float radius,
			CMetalData::IndicesDists& outIndices) const;

	const int FindNearestCluster(const springai::AIFloat3& pos) const;
	const int FindNearestCluster(const springai::AIFloat3& pos, PointPredicate& predicate) const;

	const Clusters& GetClusters() const { return clusters; }
	const ClusterGraph& GetClusterGraph() const { return clusterGraph; }
	const ClusterCostMap& GetClusterEdgeCosts() const { return clusterEdgeCosts; }

	/*
	 * Hierarchical clusterization. Not reusable. Metric: complete link. Thread-unsafe
	 */
	void Clusterize(float maxDistance, CRagMatrix<float>& distmatrix);
	void Statistics();

	void AnalyzeMap(CCircuitAI* circuit, CMap* map, springai::Resource* res, bwem::IGrid* ter, F3Vec& outSpots);
	virtual bool IsWalkable(int xSlope, int ySlope) const;  // x, y in slope map
	void MakeResourcePoints(CMap* map, springai::Resource* res, F3Vec& vectoredSpots);

	const SMetal& operator[](int idx) const { return spots[idx]; }

	static void TriangulateGraph(const std::vector<double>& coords,
			std::function<float (std::size_t A, std::size_t B)> distance,
			std::function<void (std::size_t A, std::size_t B)> addEdge);
	static void MakeConvexHull(const std::vector<double>& coords,
			std::function<void (std::size_t A, std::size_t B)> addEdge);
private:
	void BuildClusterGraph();

	bool isInitialized;
	ShortVec metalMap;
	bwem::IGrid* terrain;
	Metals spots;
	utils::SPointAdaptor<Metals> spotsAdaptor;
	using MetalTree = nanoflann::KDTreeSingleIndexAdaptor<
			nanoflann::L2_Simple_Adaptor<float, utils::SPointAdaptor<Metals> >,
			utils::SPointAdaptor<Metals>,
			2 /* dim */, int>;
	MetalTree metalTree;
	float spotMinIncome;
	float spotAvgIncome;
	float spotMaxIncome;
	float spotStdDeviation;

	Clusters clusters;
	utils::SPointAdaptor<Clusters> clustersAdaptor;
	using ClusterTree = nanoflann::KDTreeSingleIndexAdaptor<
			nanoflann::L2_Simple_Adaptor<float, utils::SPointAdaptor<Clusters> >,
			utils::SPointAdaptor<Clusters>,
			2 /* dim */, int>;
	ClusterTree clusterTree;
	float clusterMinIncome;
	float clusterAvgIncome;
	float clusterMaxIncome;
	float clusterStdDeviation;

	ClusterGraph clusterGraph;
	ClusterCostMap clusterEdgeCosts;

	std::atomic<bool> isClusterizing;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_METALDATA_H_
