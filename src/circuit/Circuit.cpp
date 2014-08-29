/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "Circuit.h"
#include "GameAttribute.h"
#include "Scheduler.h"
#include "utils.h"

#include "Game.h"
#include "Map.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Log.h"

// ------------ delete begin
extern "C" {
	#include "cluster/cluster.h"
}
// ------------ delete end

#include "AIFloat3.h"

namespace circuit {

using namespace springai;

#define LOG(fmt, ...)	log->DoLog(utils::string_format(std::string(fmt), ##__VA_ARGS__).c_str())

std::unique_ptr<CGameAttribute> CCircuit::gameAttribute(nullptr);
uint CCircuit::counterGA = 0;

CCircuit::CCircuit(springai::OOAICallback* callback) :
		initialized(false),
		callback(callback),
		log(callback->GetLog()),
		game(callback->GetGame()),
		map(callback->GetMap()),
		skirmishAIId(-1)
{
}

CCircuit::~CCircuit()
{
	printf("<DEBUG> Entering: %s,\t skirmishId: %i\n", __PRETTY_FUNCTION__, skirmishAIId);
	if (initialized) {
		Release(0);
	}
}

int CCircuit::Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback)
{
	this->skirmishAIId = skirmishAIId;
	CreateGameAttribute();
	scheduler = std::unique_ptr<CScheduler>(new CScheduler());

	if (!gameAttribute->HasStartBoxes(false)) {
		gameAttribute->ParseSetupScript(game->GetSetupScript(), map->GetWidth(), map->GetHeight());
	}
	// level 0: Check if GameRulesParams have metal spots
	if (!gameAttribute->HasMetalSpots(false)) {
		// TODO: Add metal zone maps support
		std::vector<springai::GameRulesParam*> gameRulesParams = game->GetGameRulesParams();
		gameAttribute->ParseMetalSpots(gameRulesParams);

		// level 1: Schedule check of Map::GetResourceMapSpotsPositions
		if (!gameAttribute->HasMetalSpots(false)) {
			scheduler->RunTaskAt(std::make_shared<CTask>(&CCircuit::ParseEngineMetalSpots, this, 330), 300);
		}
	}
	scheduler->RunTaskAt(std::make_shared<CTask>(&CCircuit::ParseEngineMetalSpots, this, 330), 300);

	if (gameAttribute->HasStartBoxes()) {
		CStartBoxManager& startBoxes = gameAttribute->GetStartBoxManager();

		if (startBoxes.GetStartPosType() == CGameSetup::StartPos_ChooseInGame) {
			PickStartPos(startBoxes[game->GetMyAllyTeam()]);
		}
	}

//	std::vector<springai::AIFloat3> spots = map->GetResourceMapSpotsPositions(callback->GetResourceByName("Metal"));
//	for (auto& spot : spots) {
//		LOG("x:%f, z:%f, income:%f", spot.x, spot.z, spot.y);
//	}

	initialized = true;
	// signal: everything went OK
	return 0;
}

int CCircuit::Release(int reason)
{
	printf("<DEBUG> Entering: %s,\t skirmishId: %i\n", __PRETTY_FUNCTION__, callback->GetSkirmishAIId());
	DestroyGameAttribute();
	scheduler = nullptr;

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
		}
	}

	scheduler->ProcessTasks(frame);
//	if (frame % 30 == 0) {
//		// TODO: Make it event based, e.g. when clusterization done it inserts onSuccess handler into queue.
//		//       At this point queue checked and executed if required.
//		//       It will require lock on queue.
//		if (this->isClusterInvoked && this->isClusterDone.load()) {
////			DrawConvexHulls(gameAttribute.GetMetalSpots().clusters);
////			DrawCentroids(gameAttribute.GetMetalSpots().clusters, gameAttribute.GetMetalSpots().centroids);
//			isClusterInvoked = false;
//		}
//	}

	// signal: everything went OK
	return 0;
}

int CCircuit::Message(int playerId, const char* message)
{
/*	size_t msgLength = strlen(message);

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
*/
	return 0; //signaling: OK
}

int CCircuit::LuaMessage(const char* inData)
{
/*	if (strncmp(inData, "METAL_SPOTS:", 12) == 0) {
		LOG(inData + 12);
		gameAttribute.parseMetalSpots(inData + 12);
//		LOG("size1: %i", gameAttribute.GetMetalSpots().size());

//		if (callback->GetSkirmishAIId() == 0 && !isClusterInvoked) {
//			isClusterInvoked = true;
//			isClusterDone = false;
//			clusterThread = std::thread(&CCircuit::Clusterize, this, std::ref(gameAttribute.GetMetalSpots().spots));
//		}
	}*/
	return 0; //signaling: OK
}

void CCircuit::CreateGameAttribute()
{
	if (gameAttribute == nullptr) {
		gameAttribute = std::unique_ptr<CGameAttribute>(new CGameAttribute());
	}
	counterGA++;
}

void CCircuit::DestroyGameAttribute()
{
	if (counterGA <= 1) {
		if (gameAttribute != nullptr) {
			gameAttribute = nullptr;
			// deletes singleton here;
		}
		counterGA = 0;
	} else {
		counterGA--;
	}
}

void CCircuit::PickStartPos(const Box& box)
{
	int min, max;
	min = box[static_cast<int>(BoxEdges::LEFT)];
	max = box[static_cast<int>(BoxEdges::RIGHT)];
	float x = min + (rand() % (int)(max - min + 1));
	min = box[static_cast<int>(BoxEdges::TOP)];
	max = box[static_cast<int>(BoxEdges::BOTTOM)];
	float z = min + (rand() % (int)(max - min + 1));

	game->SendStartPosition(false, AIFloat3(x, map->GetElevationAt(x, z), z));
}

void CCircuit::ParseEngineMetalSpots(int i)
{
	LOG("Yo-Yo, Hi schedulllller!, %i", i);
}
//
//void CCircuit::Clusterize(const std::vector<Metal>& spots)
//{
////	utils::sleep(5);	// for testing purposes
//	// init params
//	const int nclusters = spots.size() / gameAttribute.GetMetalSpots().mexPerClusterAvg;
//	int nrows = spots.size();
//	int ncols = 2; // (x, z, metal), ignore y
//	const int transpose = 0;
//	int npass = 1000;
//	const char method = 'a';
//	const char dist = 'e';
//	int* clusterid = (int*)malloc(nrows * sizeof(int));
//
//	double** data = (double**)malloc(nrows * sizeof(double*));
//	int** mask = (int**)malloc(nrows * sizeof(int*));
//	for (int i = 0; i < nrows; i++) {
//		data[i] = (double*)malloc(ncols * sizeof(double));
//		mask[i] = (int*)malloc(ncols * sizeof(int));
//
//		data[i][0] = (double)spots[i].position.x;
//		data[i][1] = (double)spots[i].position.z;
//		data[i][2] = (double)spots[i].income;
//		for (int j = 0; j < ncols; j++) {
//			mask[i][j] = 1;
//		}
//	}
//
//	double* weight = (double*)malloc(ncols * sizeof(double));
//	for (int i = 0; i < ncols; i++) {
//		weight[i] = 1.0;
//	}
//
//	int ifound = 0;
//	double error;
//
//	// clusterize
//	kcluster(nclusters, nrows, ncols, data, mask, weight, transpose, npass, method, dist, clusterid, &error, &ifound);
//	// get centroids
//	double** cdata = (double**)malloc(nclusters * sizeof(double*));
//	int** cmask = (int**)malloc(nclusters * sizeof(int*));
//	for (int i = 0; i < nclusters; i++)	{
//		cdata[i] = (double*)malloc(ncols * sizeof(double));
//		cmask[i] = (int*)malloc(ncols * sizeof(int));
//	}
//	getclustercentroids(nclusters, nrows, ncols, data, mask, clusterid, cdata, cmask, transpose, method);
//
//	// save results
//	{
//		std::lock_guard<std::mutex> guard(clusterMutex);
//
//		std::vector<std::vector<Metal>>& metalCluster = gameAttribute.GetMetalSpots().clusters;
//		metalCluster.resize(nclusters);
//		for (int i = 0; i < nrows; i++) {
//			metalCluster[clusterid[i]].push_back(spots[i]);
//		}
//		std::vector<springai::AIFloat3>& centroids = gameAttribute.GetMetalSpots().centroids;
//		for (int i = 0; i < nclusters; i++) {
//			centroids.push_back(AIFloat3(cdata[i][0], metalCluster[i].size(), cdata[i][1]));
//		}
//	}
//	isClusterDone = true;
//
//	DrawConvexHulls(gameAttribute.GetMetalSpots().clusters);
//	DrawCentroids(gameAttribute.GetMetalSpots().clusters, gameAttribute.GetMetalSpots().centroids);
//
//	// clean up
//	for (int i = 0; i < nclusters; i++) {
//		free(cmask[i]);
//		free(cdata[i]);
//	}
//	free(cmask);
//	free(cdata);
//
//	free(weight);
//	for (int i = 0; i < nrows; i++) {
//		free(data[i]);
//		free(mask[i]);
//	}
//	free(data);
//	free(mask);
//	free(clusterid);
//
//	isClusterInvoked = false;	// DEBUUUUUUUUUG
//}
//
//void CCircuit::DrawConvexHulls(const std::vector<std::vector<Metal>>& metalCluster)
//{
//	for (const std::vector<Metal>& vec : metalCluster) {
//		if (vec.empty()) {
//			continue;
//		} else if (vec.size() == 1) {
////			map->GetDrawer()->AddPoint(vec[0].position, "Cluster 1");
//		} else if (vec.size() == 2) {
//			map->GetDrawer()->AddLine(vec[0].position, vec[1].position);
//		} else {
//			// !!! Graham scan !!!
//			// Coord system:  *-----x
//			//                |
//			//                |
//			//                z
//
//			// @see MetalSpotManager::SortSpotsRadial
//			auto orientation = [](const AIFloat3& p1, const AIFloat3& p2, const AIFloat3& p3) {
//				return (p2.x - p1.x) * (p3.z - p1.z) - (p2.z - p1.z) * (p3.x - p1.x);
//			};
//			auto swap = [](Metal& m1, Metal& m2) {
//				AIFloat3 tmp = m1;
//				m1 = m2;
//				m2 = tmp;
//			};
//
//			int N = vec.size();
//			int M = 2; // Number of points on the convex hull.
//			for (int i(3); i < N; ++i) {
//				while (orientation(vec[M - 1].position, vec[M].position, vec[i].position) <= 0) {
//					if (M > 1) {
//						M--;
//					} else if (i == N - 1) {
//						break;
//					} else {
//						i++;
//					}
//				}
//				swap(vec[++M], vec[i]);
//			}
//
////			int M = N;
//			// draw convex hull
//			Drawer* drawer = map->GetDrawer();
//			AIFloat3 start = vec[0].position, end;
//			for (int i = 1; i < M; i++) {
//				end = vec[i].position;
//				drawer->AddLine(start, end);
//				start = end;
//			}
//			end = vec[0].position;
//			drawer->AddLine(start, end);
//		}
//	}
//}
//
//void CCircuit::DrawCentroids(const std::vector<std::vector<Metal>>& metalCluster, const std::vector<springai::AIFloat3>& centroids)
//{
//	for (int i = 0; i < metalCluster.size(); i++) {
//		std::string msgText = utils::string_format("%i mexes cluster", metalCluster[i].size());
//		map->GetDrawer()->AddPoint(centroids[i], msgText.c_str());
//	}
//}
//
//void CCircuit::ClearMetalClusters(std::vector<std::vector<Metal>>& metalCluster, std::vector<springai::AIFloat3>& centroids)
//{
//	for (auto& cluster : metalCluster) {
//		for (auto& spot : cluster) {
//			map->GetDrawer()->DeletePointsAndLines(spot.position);
//		}
//	}
//	metalCluster.clear();
//
//	for (auto& centroid : centroids) {
//		map->GetDrawer()->DeletePointsAndLines(centroid);
//	}
//	centroids.clear();
//}

} // namespace circuit
