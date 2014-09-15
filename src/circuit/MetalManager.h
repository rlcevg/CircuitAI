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
	using MetalNode = std::pair<point, unsigned>;  // spots indexer
	using MetalPredicate = std::function<bool (MetalNode const& v)>;

public:
	CMetalManager(std::vector<Metal>& spots);
	virtual ~CMetalManager();

	bool IsEmpty();
	bool IsClusterizing();
	void SetClusterizing(bool value);
	const Metal FindNearestSpot(springai::AIFloat3& pos) const;
	const Metal FindNearestSpot(springai::AIFloat3& pos, MetalPredicate& predicate) const;
	const Metals FindNearestSpots(springai::AIFloat3& pos, int num) const;
	const Metals FindWithinDistanceSpots(springai::AIFloat3& pos, float maxDistance) const;
	const Metals FindWithinRangeSpots(springai::AIFloat3& posFrom, springai::AIFloat3& posTo) const;
	const Metals& GetSpots() const;
	const std::vector<Metals>& GetClusters();

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
	// TODO: Find out more about bgi::rtree, bgi::linear, bgi::quadratic, bgi::rstar, packing algorithm?
	using MetalTree = bgi::rtree<MetalNode, bgi::rstar<16, 4>>;
	MetalTree metalTree;
	Metals spots;

	// TODO: Use KDTree for clusters/centroids also
	// Double buffer clusters as i don't want to copy vectors every time for safe use
	std::vector<Metals> clusters0;
	std::vector<Metals> clusters1;
	std::vector<Metals>* pclusters;
//	std::vector<springai::AIFloat3> centroids;

	std::atomic<bool> isClusterizing;
	std::mutex clusterMutex;
	std::shared_ptr<CRagMatrix> distMatrix;
};

} // namespace circuit

#endif // METALMANAGER_H_
