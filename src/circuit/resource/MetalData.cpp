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

#include "spring/SpringMap.h"

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

void CMetalData::Init(const Metals&& spots)
{
	this->spots = spots;
	for (SMetal& spot : this->spots) {
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

	metalTree.radiusSearch(&query_pt[0], SQUARE(radius), outIndices, searchParams);
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

void CMetalData::Clusterize(float maxDistance, CRagMatrix<float>& distMatrix)
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

void CMetalData::MakeResourcePoints(CMap* map, Resource* res, F3Vec& vectoredSpots)
{
	map->GetResourceMap(res, metalMap);

	int mapWidth = map->GetWidth() / 2;
	int mapHeight = map->GetHeight() / 2;

	int totalCells = mapHeight * mapWidth;
	float extractorRadius = map->GetExtractorRadius(res);
	int xtractorRadius = static_cast<int>(extractorRadius / (SQUARE_SIZE * 2));
	int doubleRadius = xtractorRadius * 2;
	int squareRadius = xtractorRadius * xtractorRadius;

	decltype(metalMap) rexArrayA(totalCells, 0);
	decltype(metalMap) rexArrayB(totalCells, 0);

	std::vector<int> tempAverage(totalCells, 0);

	float maxWorth = map->GetMaxResource(res);
	int totalResources = 0;
	int maxResource = 0;

	// if more spots than this are found the map is considered a resource-map (eg. speed-metal), tweak as needed
	int maxSpots = 200000;
	int tempResources = 0;
	int coordX = 0;
	int coordZ = 0;
	// from 0-255, the minimum percentage of resources a spot needs to have from
	// the maximum to be saved, prevents crappier spots in between taken spaces
	// (they are still perfectly valid and will generate resources mind you!)
	int minIncomeForSpot = 25;  // 25/255 ~ 10%
	AIFloat3 bufferSpot;

	std::vector<int> xend(doubleRadius + 1);

	for (int a = 0; a < doubleRadius + 1; a++) {
		float z = a - xtractorRadius;
		float floatsqrradius = squareRadius;
		xend[a] = int(math::sqrt(floatsqrradius - z * z));
	}

	// load up the resource values in each pixel
	double totalResourcesDouble  = 0;

	for (int i = 0; i < totalCells; i++) {
		// count the total resources so you can work out
		// an average of the whole map
		totalResourcesDouble +=  rexArrayA[i] = metalMap[i];
	}

	// do the average
//	float averageIncome = totalResourcesDouble / totalCells;
	int numSpotsFound = 0;

	// if the map does not have any resource (quick test), just stop
	if (totalResourcesDouble < 0.9)
		return;

	// Now work out how much resources each spot can make
	// by adding up the resources from nearby spots
	for (int y = 0; y < mapHeight; y++) {
		for (int x = 0; x < mapWidth; x++) {
			totalResources = 0;

			// first spot needs full calculation
			if (x == 0 && y == 0)
				for (int sy = y - xtractorRadius, a = 0;  sy <= y + xtractorRadius;  sy++, a++) {
					if (sy >= 0 && sy < mapHeight){
						for (int sx = x - xend[a]; sx <= x + xend[a]; sx++) {
							if (sx >= 0 && sx < mapWidth) {
								// get the resources from all pixels around the extractor radius
								totalResources += rexArrayA[sy * mapWidth + sx];
							}
						}
					}
				}

			// quick calc test
			if (x > 0) {
				totalResources = tempAverage[y * mapWidth + x - 1];
				for (int sy = y - xtractorRadius, a = 0;  sy <= y + xtractorRadius;  sy++, a++) {
					if (sy >= 0 && sy < mapHeight) {
						const int addX = x + xend[a];
						const int remX = x - xend[a] - 1;

						if (addX < mapWidth) {
							totalResources += rexArrayA[sy * mapWidth + addX];
						}
						if (remX >= 0) {
							totalResources -= rexArrayA[sy * mapWidth + remX];
						}
					}
				}
			} else if (y > 0) {
				// x == 0 here
				totalResources = tempAverage[(y - 1) * mapWidth];
				// remove the top half
				int a = xtractorRadius;

				for (int sx = 0; sx <= xtractorRadius;  sx++, a++) {
					if (sx < mapWidth) {
						const int remY = y - xend[a] - 1;

						if (remY >= 0) {
							totalResources -= rexArrayA[remY * mapWidth + sx];
						}
					}
				}

				// add the bottom half
				a = xtractorRadius;

				for (int sx = 0; sx <= xtractorRadius;  sx++, a++) {
					if (sx < mapWidth) {
						const int addY = y + xend[a];

						if (addY < mapHeight) {
							totalResources += rexArrayA[addY * mapWidth + sx];
						}
					}
				}
			}

			// set that spot's resource making ability
			// (divide by cells to values are small)
			tempAverage[y * mapWidth + x] = totalResources;

			if (maxResource < totalResources) {
				// find the spot with the highest resource value to set as the map's max
				maxResource = totalResources;
			}
		}
	}

	// make a list for the distribution of values
	std::vector<int> valueDist(256, 0);

	// this will get the total resources a rex placed at each spot would make
	for (int i = 0; i < totalCells; i++) {
		// scale the resources so any map will have values 0-255,
		// no matter how much resources it has
		rexArrayB[i] = tempAverage[i] * 255 / maxResource;

		int value = rexArrayB[i];
		valueDist[value]++;
	}

	// find the current best value
	int bestValue = 0;
	int numberOfValues = 0;
	int usedSpots = 0;

	for (int i = 255; i >= 0; i--) {
		if (valueDist[i] != 0) {
			bestValue = i;
			numberOfValues = valueDist[i];
			break;
		}
	}

	// make a list of the indexes of the best spots
	// (make sure that the list wont be too big)
	if (numberOfValues > 256) {
		numberOfValues = 256;
	}

	std::vector<int> bestSpotList(numberOfValues);

	for (int i = 0; i < totalCells; i++) {
		if (rexArrayB[i] == bestValue) {
			// add the index of this spot to the list
			bestSpotList[usedSpots] = i;
			usedSpots++;

			if (usedSpots == numberOfValues) {
				// the list is filled, stop the loop
				usedSpots = 0;
				break;
			}
		}
	}

	for (int a = 0; a < maxSpots; a++) {
		// reset temporary resources so it can find new spots
		tempResources = 0;
		// take the first spot
		int speedTempResources_x = 0;
		int speedTempResources_y = 0;
		int speedTempResources = 0;
		bool found = false;

		while (!found) {
			if (usedSpots == numberOfValues) {
				// the list is empty now, refill it

				// make a list of all the best spots
				for (int i = 0; i < 256; i++) {
					// clear the array
					valueDist[i] = 0;
				}

				// find the resource distribution
				for (int i = 0; i < totalCells; i++) {
					int value = rexArrayB[i];
					valueDist[value]++;
				}

				// find the current best value
				bestValue = 0;
				numberOfValues = 0;
				usedSpots = 0;

				for (int i = 255; i >= 0; i--) {
					if (valueDist[i] != 0) {
						bestValue = i;
						numberOfValues = valueDist[i];
						break;
					}
				}

				// make a list of the indexes of the best spots
				// (make sure that the list wont be too big)
				if (numberOfValues > 256) {
					numberOfValues = 256;
				}

				bestSpotList.clear();
				bestSpotList.resize(numberOfValues);

				for (int i = 0; i < totalCells; i++) {
					if (rexArrayB[i] == bestValue) {
						// add the index of this spot to the list
						bestSpotList[usedSpots] = i;
						usedSpots++;

						if (usedSpots == numberOfValues) {
							// the list is filled, stop the loop
							usedSpots = 0;
							break;
						}
					}
				}
			}

			// The list is not empty now.
			int spotIndex = bestSpotList[usedSpots];

			if (rexArrayB[spotIndex] == bestValue) {
				// the spot is still valid, so use it
				speedTempResources_x = spotIndex % mapWidth;
				speedTempResources_y = spotIndex / mapWidth;
				speedTempResources = bestValue;
				found = true;
			}

			// update the bestSpotList index
			usedSpots++;
		}

		coordX = speedTempResources_x;
		coordZ = speedTempResources_y;
		tempResources = speedTempResources;

		if (tempResources < minIncomeForSpot) {
			// if the spots get too crappy it will stop running the loops to speed it all up
			break;
		}

		// format resource coords to game-coords
		bufferSpot.x = coordX * (SQUARE_SIZE * 2) + SQUARE_SIZE;
		bufferSpot.z = coordZ * (SQUARE_SIZE * 2) + SQUARE_SIZE;
		// gets the actual amount of resource an extractor can make
		bufferSpot.y = tempResources * maxWorth * maxResource / 255;
		vectoredSpots.push_back(bufferSpot);

		numSpotsFound += 1;

		// small speedup of "wipes the resources around the spot so it is not counted twice"
		for (int sy = coordZ - xtractorRadius, a = 0;  sy <= coordZ + xtractorRadius;  sy++, a++) {
			if (sy >= 0 && sy < mapHeight) {
				int clearXStart = coordX - xend[a];
				int clearXEnd = coordX + xend[a];

				if (clearXStart < 0) {
					clearXStart = 0;
				}
				if (clearXEnd >= mapWidth) {
					clearXEnd = mapWidth - 1;
				}

				for (int xClear = clearXStart; xClear <= clearXEnd; xClear++) {
					// wipes the resources around the spot so it is not counted twice
					rexArrayA[sy * mapWidth + xClear] = 0;
					rexArrayB[sy * mapWidth + xClear] = 0;
					tempAverage[sy * mapWidth + xClear] = 0;
				}
			}
		}

		// redo the whole averaging process around the picked spot so other spots can be found around it
		for (int y = coordZ - doubleRadius; y <= coordZ + doubleRadius; y++) {
			if (y < 0 || y >= mapHeight) {
				continue;
			}
			for (int x = coordX - doubleRadius; x <= coordX + doubleRadius; x++) {
				if (x < 0 || x >= mapWidth) {
					continue;
				}
				totalResources = 0;

				// comment out for debug
				if (x == 0 && y == 0) {
					for (int sy = y - xtractorRadius, a = 0;  sy <= y + xtractorRadius;  sy++, a++) {
						if (sy >= 0 && sy < mapHeight) {
							for (int sx = x - xend[a]; sx <= x + xend[a]; sx++) {
								if (sx >= 0 && sx < mapWidth) {
									// get the resources from all pixels around the extractor radius
									totalResources += rexArrayA[sy * mapWidth + sx];
								}
							}
						}
					}
				}

				// quick calc test
				if (x > 0) {
					totalResources = tempAverage[y * mapWidth + x - 1];

					for (int sy = y - xtractorRadius, a = 0;  sy <= y + xtractorRadius;  sy++, a++) {
						if (sy >= 0 && sy < mapHeight) {
							int addX = x + xend[a];
							int remX = x - xend[a] - 1;

							if (addX < mapWidth) {
								totalResources += rexArrayA[sy * mapWidth + addX];
							}
							if (remX >= 0) {
								totalResources -= rexArrayA[sy * mapWidth + remX];
							}
						}
					}
				} else if (y > 0) {
					// x == 0 here
					totalResources = tempAverage[(y - 1) * mapWidth];
					// remove the top half
					int a = xtractorRadius;

					for (int sx = 0; sx <= xtractorRadius;  sx++, a++) {
						if (sx < mapWidth) {
							int remY = y - xend[a] - 1;

							if (remY >= 0) {
								totalResources -= rexArrayA[remY * mapWidth + sx];
							}
						}
					}

					// add the bottom half
					a = xtractorRadius;

					for (int sx = 0; sx <= xtractorRadius;  sx++, a++) {
						if (sx < mapWidth) {
							int addY = y + xend[a];

							if (addY < mapHeight) {
								totalResources += rexArrayA[addY * mapWidth + sx];
							}
						}
					}
				}

				tempAverage[y * mapWidth + x] = totalResources;
				// set that spot's resource amount
				rexArrayB[y * mapWidth + x] = totalResources * 255 / maxResource;
			}
		}
	}

	ShortVec().swap(metalMap);
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
				if (AB > (BC + AC) * 0.85f) {
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
