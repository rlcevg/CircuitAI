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
public:
	using Metal = struct Metal {
		using value_type = float;
		float income;
		springai::AIFloat3 position;
		inline value_type operator[](int const N) const {
			return (N == 0) ? position.x : position.z;
		}
	};
	using Metals = std::vector<Metal>;

public:
	CMetalManager(std::vector<Metal>& spots);
	virtual ~CMetalManager();

	bool IsEmpty();
	bool IsClusterizing();
	void SetClusterizing(bool value);
	const Metal FindNearestSpot(springai::AIFloat3& pos) const;
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

private:
	// Note: Pointtree is also a very pretty candidate for range searches.
	// Because map coordinates are big enough we can use only integer part.
	// @see https://github.com/Warzone2100/warzone2100/blob/master/src/pointtree.cpp
	using point = bg::model::point<float, 2, bg::cs::cartesian>;
	using box = bg::model::box<CMetalManager::point>;
	using MetalNode = std::pair<point, unsigned>;  // spots indexer
	// TODO: Find out more about bgi::rtree, bgi::linear, bgi::quadratic, bgi::rstar
	using MetalTree = bgi::rtree<MetalNode, bgi::quadratic<16>>;
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
