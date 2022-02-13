/*
 * ConvexHull.cpp
 *
 *  Created on: Feb 12, 2022
 *      Author: rlcevg
 */

#include "util/math/ConvexHull.h"
#include "util/Utils.h"

#include "Drawer.h"

namespace utils {

using namespace circuit;
using namespace springai;

CConvexHull::CConvexHull(const CMetalData::Metals& spots, const CMetalData::Clusters& clusters)
		: spots(spots)
		, clusters(clusters)
{
}

CConvexHull::~CConvexHull()
{
}

void CConvexHull::DrawConvexHulls(Drawer* drawer)
{
	for (const CMetalData::SCluster& cluster : clusters) {
		const CMetalData::MetalIndices& indices = cluster.idxSpots;
		if (indices.empty()) {
			continue;
		} else if (indices.size() == 1) {
			drawer->AddPoint(spots[indices[0]].position, "Cluster 1");
		} else if (indices.size() == 2) {
			drawer->AddLine(spots[indices[0]].position, spots[indices[1]].position);
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
			// number of points
			int N = indices.size();
			// the array of points
			std::vector<AIFloat3> points(N + 1);
			// Find the bottom-most point
			int min = 1, i = 1;
			float zmin = spots[indices[0]].position.z;
			for (const int idx : indices) {
				points[i] = spots[idx].position;
				float z = spots[idx].position.z;
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
			auto compare = [&p0, orientation](const AIFloat3& p1, const AIFloat3& p2) {
				// Find orientation
				int o = orientation(p0, p1, p2);
				if (o == 0) {
					return p0.SqDistance2D(p1) < p0.SqDistance2D(p2);
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

			int M = N;  // FIXME: Remove this DEBUG line
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

} // namespace utils
