/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "Circuit.h"
#include "GameAttribute.h"
#include "utils.h"

#include "Game.h"
#include "Map.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Log.h"

// ------------ delete begin
//#include "Engine.h"
//#include "Gui.h"
//#include "Lua.h"
#include "Drawer.h"
#include "Resource.h"
#include "AIColor.h"
#include "Debug.h"
#include "GraphDrawer.h"
#include "GraphLine.h"
extern "C" {
	#include "cluster/cluster.h"
}
//#include <iostream>
//#include <stack>
// ------------ delete end

#include "AIFloat3.h"

#include <string.h>
#include <functional>
#include <algorithm>

namespace circuit {

using namespace springai;

#define LOG(fmt, ...)	log->DoLog(utils::string_format(std::string(fmt), ##__VA_ARGS__).c_str())

CCircuit::CCircuit(springai::OOAICallback* callback) :
		initialized(false),
		callback(callback),
		log(callback->GetLog()),
		game(callback->GetGame()),
		map(callback->GetMap())
{
	isClusterInvoked = false;
	isClusterDone = false;
}

CCircuit::~CCircuit()
{
	if (initialized) {
		Release(0);
	}
}

int CCircuit::Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback)
{
	CGameAttribute::CreateInstance();

	// TODO: parse setupScript to get StartPosType
	//CGameSetup::StartPos_ChooseInGame
	gameAttribute.parseSetupScript(game->GetSetupScript(), map->GetWidth(), map->GetHeight());
	CStartBox& startBoxes = gameAttribute.GetStartBoxes();
	if (startBoxes.IsEmpty()) {
		CGameAttribute::DestroyInstance();
		return ERROR_INIT;
	}

	if (startBoxes.startPosType == CGameSetup::StartPos_ChooseInGame) {
		CalcStartPos(startBoxes[game->GetMyAllyTeam()]);
	}

//	GraphDrawer* drawer = callback->GetDebug()->GetGraphDrawer();
//	drawer->SetPosition(-0.6f, -0.6f);
//	drawer->SetSize(0.8f, 0.8f);
//	GraphLine* line = drawer->GetGraphLine();
//	line->SetColor(0, AIColor(0.9, 0.1, 0.1));
//	line->SetLabel(0, "label");

	initialized = true;
	// signal: everything went OK
	return 0;
}

int CCircuit::Release(int reason)
{
	CGameAttribute::DestroyInstance();
	if (clusterThread.joinable()) {
		clusterThread.join();
	}

	initialized = false;
	// signal: everything went OK
	return 0;
}

int CCircuit::Update(int frame)
{
	if (frame == 120) {
//		LOG("HIT 300 frame");
		std::vector<springai::Unit*> units = callback->GetTeamUnits();
		if (units.size() > 0) {
//			LOG("found mah comm");
			Unit* commander = units.front();
			Unit* friendCommander = NULL;;
			std::vector<springai::Unit*> friendlies = callback->GetFriendlyUnits();
			for (Unit* unit : friendlies) {
				UnitDef* unitDef = unit->GetDef();
				if (strcmp(unitDef->GetName(), "armcom1") == 0) {
					if (commander->GetUnitId() != unit->GetUnitId()) {
//						LOG("found friendly comm");
						friendCommander = unit;
						break;
					} else {
//						LOG("found mah comm again");
					}
				}
			}

			if (friendCommander) {
				LOG("giving guard order");
				commander->Guard(friendCommander);
//				commander->Build(callback->GetUnitDefByName("armsolar"), commander->GetPos(), UNIT_COMMAND_BUILD_NO_FACING);
			}
//			map->GetDrawer()->AddPoint(commander->GetPos(), "Pinh!");

//			GraphLine* line = callback->GetDebug()->GetGraphDrawer()->GetGraphLine();
//			line->AddPoint(0, -0.9, -0.9);
//			line->AddPoint(0, 0.9, 0.9);
		}
	}

	if (frame % 30 == 0) {
		// TODO: Make it event based, e.g. when clusterization done it inserts onSuccess handler into queue.
		//       At this point queue checked and executed if required.
		//       It will require lock on queue.
		if (this->isClusterInvoked && this->isClusterDone.load()) {
			DrawConvexHulls(gameAttribute.GetMetalSpots().clusters);
			DrawCentroids(gameAttribute.GetMetalSpots().clusters, gameAttribute.GetMetalSpots().centroids);
			isClusterInvoked = false;
		}
	}

	// signal: everything went OK
	return 0;
}

int CCircuit::Message(int playerId, const char* message)
{
	size_t msgLength = strlen(message);

	if (msgLength == strlen("~стройсь") && strcmp(message, "~стройсь") == 0) {
		CalcStartPos(gameAttribute.GetStartBoxes()[game->GetMyAllyTeam()]);
	}

	else if (callback->GetSkirmishAIId() == 0) {

		if (msgLength == strlen("~кластер") && strcmp(message, "~кластер") == 0) {
			if (gameAttribute.IsMetalSpotsInitialized() && !isClusterInvoked) {
				isClusterInvoked = true;
				isClusterDone = false;
				ClearMetalClusters(gameAttribute.GetMetalSpots().clusters, gameAttribute.GetMetalSpots().centroids);
				// TODO: Implement worker thread pool.
				//       Read about std::async, std::bind, std::future.
				clusterThread.join();
				clusterThread = std::thread(&CCircuit::Clusterize, this, std::ref(gameAttribute.GetMetalSpots().spots));
//				clusterThread.detach();
			}
		} else if (msgLength == strlen("~делитель++") && strncmp(message, "~делитель", strlen("~делитель")) == 0) {	// Non ASCII comparison
			if (gameAttribute.IsMetalSpotsInitialized()) {
				int& divider = gameAttribute.GetMetalSpots().mexPerClusterAvg;
				if (strcmp(message + msgLength - 2, "++") == 0) {	// ASCII comparison
					if (divider < gameAttribute.GetMetalSpots().spots.size()) {
						gameAttribute.GetMetalSpots().mexPerClusterAvg++;
					}
				} else if (strcmp(message + msgLength - 2, "--") == 0) {
					if (divider > 1) {
						divider--;
					}
				}
				std::string msgText = utils::string_format("/Say Allies: <CircuitAI> Cluster divider = %i (avarage mexes per cluster)", divider);
				game->SendTextMessage(msgText.c_str(), 0);
			}
//		} else if (strncmp(message, "~selfd", 6) == 0) {
//			callback->GetTeamUnits()[0]->SelfDestruct();
		}
	}

	return 0; //signaling: OK
}

int CCircuit::LuaMessage(const char* inData)
{
	if (strncmp(inData, "METAL_SPOTS:", 12) == 0) {
		LOG(inData + 12);
		gameAttribute.parseMetalSpots(inData + 12);
//		LOG("size1: %i", gameAttribute.GetMetalSpots().size());

		std::vector<springai::Resource*> ress = callback->GetResources();
		for (auto& res : ress) {
			LOG(res->GetName());
		}

		std::vector<springai::AIFloat3> spots = map->GetResourceMapSpotsPositions(callback->GetResourceByName("Metal"));
		for (auto& spot : spots) {
			LOG("x:%f, z:%f, income:%f", spot.x, spot.z, spot.y);
		}
		LOG("size2: %i", spots.size());
		if (callback->GetSkirmishAIId() == 0 && !isClusterInvoked) {
			isClusterInvoked = true;
			isClusterDone = false;
			clusterThread = std::thread(&CCircuit::Clusterize, this, std::ref(gameAttribute.GetMetalSpots().spots));
		}
	}
	return 0; //signaling: OK
}

void CCircuit::CalcStartPos(const Box& box)
{
//	float x = (box[static_cast<int>(BoxEdges::LEFT)] + box[static_cast<int>(BoxEdges::RIGHT)] ) / 2.0;
//	float z = (box[static_cast<int>(BoxEdges::BOTTOM)] + box[static_cast<int>(BoxEdges::TOP)] ) / 2.0;
	int min, max, output;
	min = box[static_cast<int>(BoxEdges::LEFT)];
	max = box[static_cast<int>(BoxEdges::RIGHT)];
	output = min + (rand() % (int)(max - min + 1));
	float x = output;
	min = box[static_cast<int>(BoxEdges::TOP)];
	max = box[static_cast<int>(BoxEdges::BOTTOM)];
	output = min + (rand() % (int)(max - min + 1));
	float z = output;

	game->SendStartPosition(false, AIFloat3(x, map->GetElevationAt(x, z), z));
}

void CCircuit::Clusterize(const std::vector<Metal>& spots)
{
//	utils::sleep(5);	// for testing purposes
	// init params
	const int nclusters = spots.size() / gameAttribute.GetMetalSpots().mexPerClusterAvg;
	int nrows = spots.size();
	int ncols = 2; // (x, z, metal), ignore y
	const int transpose = 0;
	int npass = 1000;
	const char method = 'a';
	const char dist = 'e';
	int* clusterid = (int*)malloc(nrows * sizeof(int));

	double** data = (double**)malloc(nrows * sizeof(double*));
	int** mask = (int**)malloc(nrows * sizeof(int*));
	for (int i = 0; i < nrows; i++) {
		data[i] = (double*)malloc(ncols * sizeof(double));
		mask[i] = (int*)malloc(ncols * sizeof(int));

		data[i][0] = (double)spots[i].position.x;
		data[i][1] = (double)spots[i].position.z;
		data[i][2] = (double)spots[i].income;
		for (int j = 0; j < ncols; j++) {
			mask[i][j] = 1;
		}
	}

	double* weight = (double*)malloc(ncols * sizeof(double));
	for (int i = 0; i < ncols; i++) {
		weight[i] = 1.0;
	}

	int ifound = 0;
	double error;

	// clusterize
	kcluster(nclusters, nrows, ncols, data, mask, weight, transpose, npass, method, dist, clusterid, &error, &ifound);
	// get centroids
	double** cdata = (double**)malloc(nclusters * sizeof(double*));
	int** cmask = (int**)malloc(nclusters * sizeof(int*));
	for (int i = 0; i < nclusters; i++)	{
		cdata[i] = (double*)malloc(ncols * sizeof(double));
		cmask[i] = (int*)malloc(ncols * sizeof(int));
	}
	getclustercentroids(nclusters, nrows, ncols, data, mask, clusterid, cdata, cmask, 0, 'a');

	// save results
	{
		std::lock_guard<std::mutex> guard(clusterMutex);

		std::vector<std::vector<Metal>>& metalCluster = gameAttribute.GetMetalSpots().clusters;
		metalCluster.resize(nclusters);
		for (int i = 0; i < nrows; i++) {
			metalCluster[clusterid[i]].push_back(spots[i]);
		}
		std::vector<springai::AIFloat3>& centroids = gameAttribute.GetMetalSpots().centroids;
		for (int i = 0; i < nclusters; i++) {
			centroids.push_back(AIFloat3(cdata[i][0], metalCluster[i].size(), cdata[i][1]));
		}
	}
	isClusterDone = true;

//	DrawConvexHulls(gameAttribute.GetMetalSpots().clusters);
//	DrawCentroids(gameAttribute.GetMetalSpots().clusters, gameAttribute.GetMetalSpots().centroids);

	// clean up
	for (int i = 0; i < nclusters; i++) {
		free(cmask[i]);
		free(cdata[i]);
	}
	free(cmask);
	free(cdata);

	free(weight);
	for (int i = 0; i < nrows; i++) {
		free(data[i]);
		free(mask[i]);
	}
	free(data);
	free(mask);
	free(clusterid);
}

void CCircuit::DrawConvexHulls(const std::vector<std::vector<Metal>>& metalCluster)
{
	for (const std::vector<Metal>& vec : metalCluster) {
		if (vec.empty()) {
			continue;
		} else if (vec.size() == 1) {
//			map->GetDrawer()->AddPoint(vec[0].position, "Cluster 1");
		} else if (vec.size() == 2) {
//			map->GetDrawer()->AddPoint(vec[0].position, "Cluster 2");
//			map->GetDrawer()->AddPoint(vec[1].position, "Cluster 2");
			map->GetDrawer()->AddLine(vec[0].position, vec[1].position);
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
			std::function<float(const AIFloat3&, const AIFloat3&)> dist = [](const AIFloat3& p1, const AIFloat3& p2) -> float {
				float x = p1.x - p2.x;
				float z = p1.z - p2.z;
				return x * x + z * z;
			};

			// number of points
			int N = vec.size();
			// the array of points
			std::vector<AIFloat3> points(N + 1);
			// Find the bottom-most point
			int min = 1, i = 1;
			float zmin = vec[0].position.z;
			for (const Metal& spot : vec) {
				points[i] = spot.position;
				float z = spot.position.z;
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

			// A function used by library function qsort() to sort an array of
			// points with respect to the first point
			AIFloat3& p0 = points[1];
			auto compare = [&p0, orientation, dist](const AIFloat3& p1, const AIFloat3& p2) {
				// Find orientation
				int o = orientation(p0, p1, p2);
				if (o == 0) {
					return dist(p0, p1) < dist(p0, p2);
				}
				return o > 0;
			};
			// Sort n-1 points with respect to the first point. A point p1 comes
			// before p2 in sorted ouput if p2 has larger polar angle (in
			// counterclockwise direction) than p1
			std::sort(points.begin() + 2, points.end(), compare);

			// let points[0] be a sentinel point that will stop the loop
			points[0] = points[N];

			int M = 1; // Number of points on the convex hull.
			for (int i(2); i <= N; ++i) {
				while (orientation(points[M - 1], points[M], points[i]) <= 0) {
					if (M > 1) {
						M--;
					} else if (i == N) {
						break;
					} else {
						i++;
					}
				}
				swap(points[++M], points[i]);
			}

			// draw convex hull
			AIFloat3 start = points[0], end;
			for (int i = 1; i < M; i++) {
				end = points[i];
				map->GetDrawer()->AddLine(start, end);
				start = end;
			}
			end = points[0];
			map->GetDrawer()->AddLine(start, end);
		}
	}
}

void CCircuit::DrawCentroids(const std::vector<std::vector<Metal>>& metalCluster, const std::vector<springai::AIFloat3>& centroids)
{
	for (int i = 0; i < metalCluster.size(); i++) {
		std::string msgText = utils::string_format("%i mexes cluster", metalCluster[i].size());
		map->GetDrawer()->AddPoint(centroids[i], msgText.c_str());
	}
}

void CCircuit::ClearMetalClusters(std::vector<std::vector<Metal>>& metalCluster, std::vector<springai::AIFloat3>& centroids)
{
	for (auto& cluster : metalCluster) {
		for (auto& spot : cluster) {
			map->GetDrawer()->DeletePointsAndLines(spot.position);
		}
	}
	metalCluster.clear();

	for (auto& centroid : centroids) {
		map->GetDrawer()->DeletePointsAndLines(centroid);
	}
	centroids.clear();
}

} // namespace circuit
