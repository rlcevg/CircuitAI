/*
 * MetalManager.h
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#ifndef METALMANAGER_H_
#define METALMANAGER_H_

#include "AIFloat3.h"

#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <vector>
#include <atomic>
#include <mutex>
#include <memory>

namespace springai {
	class Drawer;
}

namespace circuit {

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

class CRagMatrix;

class CMetalManager {
private:
	// Note: Pointtree is also a very pretty candidate for range searches.
	// Because map coordinates are big enough we can use only integer part.
	// @see https://github.com/Warzone2100/warzone2100/blob/master/src/pointtree.cpp
	using point = bg::model::point<float, 2, bg::cs::cartesian>;
	using box = bg::model::box<point>;

public:
	using Metal = struct Metal {
		float income;
		springai::AIFloat3 position;
	};
	using Metals = std::vector<Metal>;
	using MetalNode = std::pair<point, int>;  // spots indexer
	using MetalPredicate = std::function<bool (MetalNode const& v)>;
	using MetalIndices = std::vector<int>;

public:
	CMetalManager(std::vector<Metal>& spots);
	virtual ~CMetalManager();

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
	const std::vector<MetalIndices>& GetClusters();
	const std::vector<springai::AIFloat3>& GetCentroids();
	const int FindNearestCluster(const springai::AIFloat3& pos);
	const int FindNearestCluster(const springai::AIFloat3& pos, MetalPredicate& predicate);
	const MetalIndices FindNearestClusters(const springai::AIFloat3& pos, int num, MetalPredicate& predicate);

	void SetDistMatrix(CRagMatrix& distmatrix);
	/*
	 * Hierarchical clusterization. Not reusable. Metric: complete link
	 */
	void Clusterize(float maxDistance, std::shared_ptr<CRagMatrix> distmatrix);
	// debug, could be used for defence perimeter calculation
	void DrawConvexHulls(springai::Drawer* drawer);
//	void DrawCentroids(springai::Drawer* drawer);
	// debug
	void ClearMetalClusters(springai::Drawer* drawer);

	const Metal& operator[](int idx) const;

private:
	Metals spots;
	// TODO: Find out more about bgi::rtree, bgi::linear, bgi::quadratic, bgi::rstar, packing algorithm?
	using MetalTree = bgi::rtree<MetalNode, bgi::rstar<16, 4>>;
	MetalTree metalTree;

	// FIXME: Remove mutexes, clusterize only once. Or add ClusterLock()/ClusterUnlock() to interface.
	//        As for now search indices could belong to wrong clusters
	// Double buffer clusters as i don't want to copy vectors every time for safe use
	std::vector<MetalIndices> clusters0;
	std::vector<MetalIndices> clusters1;
	std::vector<MetalIndices>* pclusters;
	std::vector<springai::AIFloat3> centroids0;
	std::vector<springai::AIFloat3> centroids1;
	std::vector<springai::AIFloat3>* pcentroids;
	using ClusterTree = bgi::rtree<MetalNode, bgi::quadratic<16>>;
	ClusterTree clusterTree0;
	ClusterTree clusterTree1;
	std::atomic<ClusterTree*> pclusterTree;

	std::atomic<bool> isClusterizing;
	std::mutex clusterMutex;
	std::shared_ptr<CRagMatrix> distMatrix;
};

} // namespace circuit

#endif // METALMANAGER_H_
