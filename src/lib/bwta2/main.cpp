#if defined(WIN32) || defined(_WINDOWS) || defined(_MSCV) || defined(_MSC_VER)

#include <windows.h>

#include "../OfflineExtractor/MapFileParser.h"
#include "../OfflineExtractor/wallingAnalysis.h"
#include "functions.h"

std::vector<BWAPI::TilePosition> buildTiles;
std::vector<BWAPI::TilePosition> outsideTiles;
std::vector<BWAPI::TilePosition> walkableTiles;
std::vector<BWAPI::TilePosition> supplyTiles;
std::vector<BWAPI::TilePosition> barracksTiles;
BWTA::BaseLocation* homeBase;
BWTA::Region* homeRegion;
bool optimizeGap = true;
void analyzeChoke(BWTA::Chokepoint* choke);
bool canBuildHere(BWAPI::TilePosition position, BWAPI::UnitType type); // simplier version of BWAPI::canBuildHere()
void initClingoProgramSource();
BWAPI::TilePosition findClosestTile(const std::vector<BWAPI::TilePosition>& tiles, BWAPI::TilePosition targetTile);
BWAPI::TilePosition findFarthestTile(const std::vector<BWAPI::TilePosition>& tiles, BWAPI::TilePosition targetTile);

int main (int argc, char * argv[])
{
	// Check the number of parameters
	if (argc < 2) {
		// Tell the user how to run the program
		std::cerr << "Usage: " << argv[0] << " [mapFile]" << std::endl;
		return 1;
	}

	std::cout << "=======================" << std::endl;
	std::cout << "      OFFLINE BWTA     " << std::endl;
	std::cout << "=======================" << std::endl;

	// Off line map file parsing
	bool ok = BWTA::parseMapFile(argv[1]);
	if (!ok) return 0;

	// Normal procedure to analyze map
	std::cout << "All info loaded, analyzing map as regular..." << std::endl;
	BWTA::analyze();
	std::cout << "DONE" << std::endl;


	// Generate the grids around all chokepoints:
/*	std::ofstream fileTxt("logs/output.txt"); 
	const std::set<BWTA::Chokepoint*> chokePoints = BWTA::getChokepoints();
	for(std::set<BWTA::Chokepoint*>::const_iterator c = chokePoints.begin(); c!=chokePoints.end();++c) {
		BWTA::Chokepoint *cp = (*c);
		BWTA::RectangleArray<int> chokeGrid = BWTA::getChokeGrid(cp);
		// Print grid
		BWAPI::TilePosition center = BWAPI::TilePosition(cp->getCenter());
		int radius = int(cp->getWidth()/TILE_SIZE);
		BWAPI::TilePosition region1 = BWAPI::TilePosition(cp->getRegions().first->getCenter());
		BWAPI::TilePosition region2 = BWAPI::TilePosition(cp->getRegions().second->getCenter());
		fileTxt << "Chokepoint:\n";
		fileTxt << "Region 1: " << radius+region1.x-center.x << "," << radius+region1.y-center.y << std::endl;
		fileTxt << "Region 2: " << radius+region2.x-center.x << "," << radius+region2.y-center.y << std::endl;
		BWAPI::TilePosition side1 = BWAPI::TilePosition(cp->getSides().first);
		BWAPI::TilePosition side2 = BWAPI::TilePosition(cp->getSides().second);
		int s1x = radius + side1.x - center.x;
		int s1y = radius + side1.y - center.y;
// 		fileTxt << "Side 1: " << s1x << "," << s1y << " is " << chokeGrid[s1x][s1y] << std::endl;
		int s2x = radius+side2.x-center.x;
		int s2y = radius+side2.y-center.y;
// 		fileTxt << "Side 2: " << s2x << "," << s2y << " is " << chokeGrid[s2x][s2y] << std::endl;
		BWTA::generateWallStartingPoints(chokeGrid, s1x, s1y, s2x, s2y, &fileTxt);
		for (unsigned int y = 0; y < chokeGrid.getHeight(); ++y) {
			for (unsigned int x = 0; x < chokeGrid.getWidth(); ++x) {
				fileTxt << chokeGrid[x][y];
			}
			fileTxt << "\n";
		}
	}
	fileTxt.close();
*/


	// Test Certicky's walling algorithm
	// =================================

	// get the first startLocation as home
	homeBase = *BWTA::getStartLocations().begin();
	homeRegion = homeBase->getRegion();

	// iterate through all chokepoints and look for the one with the smallest gap (least width)
	double minWidth = std::numeric_limits<double>::max();
	BWTA::Chokepoint* smallestChokepoint = nullptr;
	for (const auto& chokepoint : homeRegion->getChokepoints()) {
		if (chokepoint->getWidth() < minWidth) {
			minWidth = chokepoint->getWidth();
			smallestChokepoint = chokepoint;
		}
	}

	analyzeChoke(smallestChokepoint);
	initClingoProgramSource();
// 	runASPSolver();




	BWTA::cleanMemory();
	return 0;
}

void analyzeChoke(BWTA::Chokepoint* choke)
{
	int x = choke->getCenter().x; 
	int y = choke->getCenter().y;
	int tileX = x / BWAPI::TILEPOSITION_SCALE;
	int tileY = y / BWAPI::TILEPOSITION_SCALE;
	const int TILE_TO_WALK = BWAPI::TILEPOSITION_SCALE / BWAPI::WALKPOSITION_SCALE;

	// analysis of build tiles near choke points
	int maxDist = 10;
	for (int i = tileX - maxDist; i <= tileX + maxDist; ++i){
		for (int j = tileY - maxDist; j <= tileY + maxDist; ++j){
			BWAPI::TilePosition currentTile(i, j);
			// if the tile is in home region
			if (BWTA::getRegion(currentTile) == homeRegion){
				// and it is buildable, add it to the buildTiles
				if (BWTA::MapData::buildability[i][j]){
					buildTiles.push_back(currentTile);
				}
			} else { // if the tile is outside the home region
				// and it is walkable, add it to the outside tiles
				if (BWTA::MapData::walkability[i*TILE_TO_WALK + 1][j*TILE_TO_WALK + 2] &&
					BWTA::MapData::walkability[i*TILE_TO_WALK + 1][j*TILE_TO_WALK + 1] &&
					BWTA::MapData::walkability[i*TILE_TO_WALK + 2][j*TILE_TO_WALK + 1] &&
					BWTA::MapData::walkability[i*TILE_TO_WALK + 2][j*TILE_TO_WALK + 2]
					)
					outsideTiles.push_back(currentTile);
			}
		}
	}

	maxDist += 4;
	for (int i = tileX - maxDist; i <= tileX + maxDist; ++i){
		for (int j = tileY - maxDist; j <= tileY + maxDist; ++j){
			if (BWTA::MapData::walkability[i*TILE_TO_WALK + 1][j*TILE_TO_WALK + 2] &&
				BWTA::MapData::walkability[i*TILE_TO_WALK + 1][j*TILE_TO_WALK + 1] &&
				BWTA::MapData::walkability[i*TILE_TO_WALK + 2][j*TILE_TO_WALK + 1] &&
				BWTA::MapData::walkability[i*TILE_TO_WALK + 2][j*TILE_TO_WALK + 2]
				)
				walkableTiles.push_back(BWAPI::TilePosition(i, j));
		}
	}

	// analyze buildable tiles for buildings that we will use to wall in (barracks and supply depot)
	for (const auto& tile : buildTiles) {
		if (canBuildHere(tile, BWAPI::UnitTypes::Terran_Supply_Depot))
			supplyTiles.push_back(tile);
		if (canBuildHere(tile, BWAPI::UnitTypes::Terran_Barracks))
			barracksTiles.push_back(tile);
	}
}

bool canBuildHere(BWAPI::TilePosition position, BWAPI::UnitType type)
{
	// lt = left top, rb = right bottom
	BWAPI::TilePosition lt = position;
	BWAPI::TilePosition rb = lt + type.tileSize();

	// Tile buildability check
	for (int x = lt.x; x < rb.x; ++x) {
		for (int y = lt.y; y < rb.y; ++y) {
			if (!BWTA::MapData::buildability[x][y]) return false;
		}
	}

	return true;
}

void initClingoProgramSource()
{
	std::ofstream file;

	file.open("logs/ASPwall.txt");
	if (file.is_open()){

		file << "% Building / Unit types" << std::endl
			<< "buildingType(marineType).	" << std::endl
			<< "buildingType(barracksType)." << std::endl
			<< "buildingType(supplyDepotType).	" << std::endl << std::endl

			<< "% Size specifications" << std::endl
			<< "width(marineType,1).	height(marineType,1)." << std::endl
			<< "width(barracksType,4).	height(barracksType,3)." << std::endl
			<< "width(supplyDepotType,3). 	height(supplyDepotType,2)." << std::endl << std::endl

			<< "costs(supplyDepotType, 100)." << std::endl
			<< "costs(barracksType, 150)." << std::endl

			<< "% Gaps" << std::endl
			<< "leftGap(barracksType,16). 	rightGap(barracksType,7).	topGap(barracksType,8). 	bottomGap(barracksType,15)." << std::endl
			<< "leftGap(marineType,0). 		rightGap(marineType,0). 	topGap(marineType,0). 		bottomGap(marineType,0)." << std::endl
			<< "leftGap(supplyDepotType,10).		 rightGap(supplyDepotType,9). 	topGap(supplyDepotType,10). 		bottomGap(supplyDepotType,5)." << std::endl << std::endl

			<< "% Facts" << std::endl
			<< "building(marine1).	type(marine1, marineType)." << std::endl
			<< "building(barracks1).	type(barracks1, barracksType)." << std::endl
			<< "building(barracks2).	type(barracks2, barracksType)." << std::endl
			<< "building(supplyDepot1).	type(supplyDepot1, supplyDepotType)." << std::endl
			<< "building(supplyDepot2).	type(supplyDepot2, supplyDepotType)." << std::endl
			<< "building(supplyDepot4).	type(supplyDepot4, supplyDepotType)." << std::endl
			<< "building(supplyDepot3).	type(supplyDepot3, supplyDepotType)." << std::endl << std::endl

			<< "% Constraint: two units/buildings cannot occupy the same tile" << std::endl
			<< ":- occupiedBy(B1, X, Y), occupiedBy(B2, X, Y), B1 != B2." << std::endl << std::endl

			<< "% Tiles occupied by buildings" << std::endl
			<< "occupiedBy(B,X2,Y2) :- place(B, X1, Y1)," << std::endl
			<< "						type(B, BT), width(BT,Z), height(BT, Q)," << std::endl
			<< "						X2 >= X1, X2 < X1+Z, Y2 >= Y1, Y2 < Y1+Q," << std::endl
			<< "						walkableTile(X2, Y2)." << std::endl
			<< "						" << std::endl << std::endl

			<< "% Gaps between two adjacent tiles, occupied by buildings." << std::endl
			<< "verticalGap(X1,Y1,X2,Y2,G) :-" << std::endl
			<< "	occupiedBy(B1,X1,Y1), occupiedBy(B2,X2,Y2)," << std::endl
			<< "	B1 != B2, X1=X2, Y1=Y2-1, G=S1+S2," << std::endl
			<< "	type(B1,T1), type(B2,T2), bottomGap(T1,S1), topGap(T2,S2)." << std::endl
			<< "	" << std::endl
			<< "verticalGap(X1,Y1,X2,Y2,G) :-" << std::endl
			<< "	occupiedBy(B1,X1,Y1), occupiedBy(B2,X2,Y2)," << std::endl
			<< "	B1 != B2, X1=X2, Y1=Y2+1, G=S1+S2," << std::endl
			<< "	type(B1,T1), type(B2,T2), bottomGap(T2,S2), topGap(T1,S1)." << std::endl
			<< "	" << std::endl
			<< "horizontalGap(X1,Y1,X2,Y2,G) :-" << std::endl
			<< "	occupiedBy(B1,X1,Y1), occupiedBy(B2,X2,Y2)," << std::endl
			<< "	B1 != B2, X1=X2-1, Y1=Y2, G=S1+S2," << std::endl
			<< "	type(B1,T1), type(B2,T2), rightGap(T1,S1), leftGap(T2,S2)." << std::endl << std::endl

			<< "horizontalGap(X1,Y1,X2,Y2,G) :-" << std::endl
			<< "	occupiedBy(B1,X1,Y1), occupiedBy(B2,X2,Y2)," << std::endl
			<< "	B1 != B2, X1=X2+1, Y1=Y2, G=S1+S2," << std::endl
			<< "	type(B1,T1), type(B2,T2), rightGap(T2,S2), leftGap(T1,S1)." << std::endl << std::endl

			<< "cost(B, C) :- place(B, X, Y), type(B, BT), costs(BT, COST), C=COST." << std::endl

			///////////////
			<< "% Tile information" << std::endl;

		for (auto& tile : walkableTiles)
			file << "walkableTile(" << tile.x << ", " << tile.y << ")." << std::endl;
		for (auto& tile : barracksTiles)
			file << "buildable(barracksType, " << tile.x << ", " << tile.y << ")." << std::endl;
		for (auto& tile : supplyTiles)
			file << "buildable(supplyDepotType, " << tile.x << ", " << tile.y << ")." << std::endl;
			
		file << std::endl;

		////////////////////////

		BWAPI::TilePosition insideBase = findClosestTile(buildTiles, homeBase->getTilePosition());
		BWAPI::TilePosition outsideBase = findFarthestTile(outsideTiles, homeBase->getTilePosition());
		file << "insideBase(" << insideBase.x << ", " << insideBase.y << ").\t";
		file << "outsideBase(" << outsideBase.x << ", " << outsideBase.y << ")." << std::endl << std::endl

			<< "% Constraint: Inside of the base must not be reachable." << std::endl
			<< ":- insideBase(X2,Y2), outsideBase(X1,Y1), canReach(X2,Y2)." << std::endl << std::endl

			<< "% Reachability between tiles." << std::endl
			<< "blocked(X,Y) :- occupiedBy(B,X,Y), building(B), walkableTile(X,Y)." << std::endl
			<< "canReach(X,Y) :- outsideBase(X,Y)." << std::endl << std::endl

			<< "canReach(X2,Y) :-" << std::endl
			<< "	canReach(X1,Y), X1=X2+1, walkableTile(X1,Y), walkableTile(X2,Y)," << std::endl
			<< "	not blocked(X1,Y), not blocked(X2,Y)." << std::endl
			<< "canReach(X2,Y) :-" << std::endl
			<< "	canReach(X1,Y), X1=X2-1, walkableTile(X1,Y), walkableTile(X2,Y)," << std::endl
			<< "	not blocked(X1,Y), not blocked(X2,Y)." << std::endl
			<< "canReach(X,Y2) :-" << std::endl
			<< "	canReach(X,Y1), Y1=Y2+1, walkableTile(X,Y1), walkableTile(X,Y2)," << std::endl
			<< "	not blocked(X,Y1), not blocked(X,Y2)." << std::endl
			<< "canReach(X,Y2) :-" << std::endl
			<< "	canReach(X,Y1), Y1=Y2-1, walkableTile(X,Y1), walkableTile(X,Y2)," << std::endl
			<< "	not blocked(X,Y1), not blocked(X,Y2)." << std::endl
			<< "canReach(X2,Y2) :-" << std::endl
			<< "	canReach(X1,Y1), X1=X2+1, Y1=Y2+1, walkableTile(X1,Y1), walkableTile(X2,Y2)," << std::endl
			<< "	not blocked(X1,Y1), not blocked(X2,Y2)." << std::endl
			<< "canReach(X2,Y2) :-" << std::endl
			<< "	canReach(X1,Y1), X1=X2-1, Y1=Y2+1, walkableTile(X1,Y1), walkableTile(X2,Y2)," << std::endl
			<< "	not blocked(X1,Y1), not blocked(X2,Y2)." << std::endl
			<< "canReach(X2,Y2) :-" << std::endl
			<< "	canReach(X1,Y1), X1=X2+1, Y1=Y2-1, walkableTile(X1,Y1), walkableTile(X2,Y2)," << std::endl
			<< "	not blocked(X1,Y1), not blocked(X2,Y2)." << std::endl
			<< "canReach(X2,Y2) :-" << std::endl
			<< "	canReach(X1,Y1), X1=X2-1, Y1=Y2-1, walkableTile(X1,Y1), walkableTile(X2,Y2)," << std::endl
			<< "	not blocked(X1,Y1), not blocked(X2,Y2)." << std::endl << std::endl

			<< "% Using gaps to reach (walk on) blocked locations." << std::endl
			<< "enemyUnitX(32). enemyUnitY(32)." << std::endl
			<< "canReach(X1,Y1) :- horizontalGap(X1,Y1,X2,Y1,G), G >= S, X2=X1+1, canReach(X1,Y3), Y3=Y1+1, enemyUnitX(S)." << std::endl
			<< "canReach(X1,Y1) :- horizontalGap(X1,Y1,X2,Y1,G), G >= S, X2=X1-1, canReach(X1,Y3), Y3=Y1+1, enemyUnitX(S)." << std::endl
			<< "canReach(X1,Y1) :- horizontalGap(X1,Y1,X2,Y1,G), G >= S, X2=X1+1, canReach(X1,Y3), Y3=Y1-1, enemyUnitX(S)." << std::endl
			<< "canReach(X1,Y1) :- horizontalGap(X1,Y1,X2,Y1,G), G >= S, X2=X1-1, canReach(X1,Y3), Y3=Y1-1, enemyUnitX(S)." << std::endl
			<< "canReach(X1,Y1) :- verticalGap(X1,Y1,X1,Y2,G), G >= S, Y2=Y1+1, canReach(X3,Y1), X3=X1-1, enemyUnitY(S)." << std::endl
			<< "canReach(X1,Y1) :- verticalGap(X1,Y1,X1,Y2,G), G >= S, Y2=Y1-1, canReach(X3,Y1), X3=X1-1, enemyUnitY(S)." << std::endl
			<< "canReach(X1,Y1) :- verticalGap(X1,Y1,X1,Y2,G), G >= S, Y2=Y1+1, canReach(X3,Y1), X3=X1+1, enemyUnitY(S)." << std::endl
			<< "canReach(X1,Y1) :- verticalGap(X1,Y1,X1,Y2,G), G >= S, Y2=Y1-1, canReach(X3,Y1), X3=X1+1, enemyUnitY(S)." << std::endl

			<< "% Generate all the potential placements." << std::endl
			<< "1{place(barracks1,X,Y) : buildable(barracksType,X,Y)}1." << std::endl
			<< "0{place(barracks2,X,Y) : buildable(barracksType,X,Y)}1." << std::endl
			<< "1{place(supplyDepot1,X,Y) : buildable(supplyDepotType,X,Y)}1." << std::endl
			<< "0{place(supplyDepot2,X,Y) : buildable(supplyDepotType,X,Y)}1." << std::endl
			<< "0{place(supplyDepot3,X,Y) : buildable(supplyDepotType,X,Y)}1." << std::endl
			<< "0{place(supplyDepot4,X,Y) : buildable(supplyDepotType,X,Y)}1." << std::endl << std::endl

			<< "% Optimization criterion" << std::endl;

		if (optimizeGap){
			file << "#minimize {G@1: horizontalGap(X1,Y1,X2,Y2,G)}." << std::endl
				<< "#minimize {G@1: verticalGap(X1,Y1,X2,Y2,G)}." << std::endl
				<< "#minimize {1@2: place(supplyDepot2,X,Y)}." << std::endl
				<< "#minimize {1@2: place(supplyDepot3,X,Y)}." << std::endl
				<< "#minimize {1@2: place(supplyDepot4,X,Y)}." << std::endl
				<< "#minimize {1@2: place(barracks2,X,Y)}. " << std::endl << std::endl;
		} else
			file << "#minimize {C: cost(B, C)}." << std::endl << std::endl;


		file << "#show place/3.";
		file.close();

		std::cout << "ASP Solver Contents Ready!" << std::endl;

		if (optimizeGap) std::cout << "Optimization: GAP" << std::endl;
		else std::cout << "Optimization: COST" << std::endl;
	} else {
		std::cout << "Error Opening File" << std::endl;
	}
}

BWAPI::TilePosition findClosestTile(const std::vector<BWAPI::TilePosition>& tiles, BWAPI::TilePosition targetTile)
{
	BWAPI::TilePosition returnTile;
	double minDist = std::numeric_limits<double>::max();
	for (const auto& tile : tiles) {
		double actualDistance = targetTile.getApproxDistance(tile);
		if (actualDistance <= minDist) { // WARNING: omitted to check if there is a path between tiles
			minDist = actualDistance;
			returnTile = tile;
		}
	}

	return returnTile;
}

BWAPI::TilePosition findFarthestTile(const std::vector<BWAPI::TilePosition>& tiles, BWAPI::TilePosition targetTile)
{
	BWAPI::TilePosition returnTile;
	double minDist = 0;
	for (const auto& tile : tiles) {
		double actualDistance = targetTile.getApproxDistance(tile);
		if (actualDistance >= minDist) { // WARNING: omitted to check if there is a path between tiles
			minDist = actualDistance;
			returnTile = tile;
		}
	}

	return returnTile;
}

#endif
