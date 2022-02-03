#include "BalanceMetrics.h"

#include "stdafx.h"

namespace BWTA
{
	void balanceAnalysis()
	{
		// TODO check if map is analyzed
		// TODO check if distance transform is procesed
		// TODO check if buildChokeNodes was called (MapData::chokeNodes was populated)
		
		LOG("Starting map balance analysis");
		// **************************************************
		// METRIC 1
		// --------------------------------------------------
		// Starting location openness
		// **************************************************
		//log("METRIC 1 ------------------------------");

		double meanOpenness = 0;
		double standardDeviationOpenness = 0;
		double meanArea = 0;
		double standardDeviationArea = 0;
		std::set<BWTA::BaseLocation*> startLocationsTmp = BWTA::getStartLocations();

		// copy the set to vector for a non-random access
		std::vector<BWTA::BaseLocation*> startLocations(startLocationsTmp.size());
		std::copy(startLocationsTmp.begin(), startLocationsTmp.end(), startLocations.begin());

		for (const auto& startLocation : startLocations) {
			const BWTA::Region* region = startLocation->getRegion();
			//meanOpenness += regionMaxDistance[region];
			meanOpenness += region->getMaxDistance();
			meanArea += region->getPolygon().getArea();
			//log("Openness: " << regionMaxDistance[region] << "\t" << "Area: " << region->getPolygon().getArea());
		}

		meanOpenness = meanOpenness / startLocations.size();
		meanArea = meanArea / startLocations.size();
		//log("Mean Openness: " << meanOpenness);
		//log("Mean Area: " << meanArea);

		for (const auto& startLocation : startLocations) {
			const BWTA::Region* region = startLocation->getRegion();
			//standardDeviationOpenness += pow(regionMaxDistance[region]-meanOpenness,2); 
			standardDeviationOpenness += pow(region->getMaxDistance()-meanOpenness,2); 
			standardDeviationArea += pow(region->getPolygon().getArea()-meanArea,2); 
		}

		standardDeviationOpenness = sqrt(standardDeviationOpenness/startLocations.size());
		standardDeviationArea = sqrt(standardDeviationArea/startLocations.size());
// 		log("Standard Deviation Openness: " << standardDeviationOpenness);
// 		log("Standard Deviation Area: " << standardDeviationArea);
		LOG(standardDeviationArea);
		LOG(meanArea);
		LOG(standardDeviationOpenness);
		LOG(meanOpenness);

		// **************************************************
		// METRIC 2
		// --------------------------------------------------
		// Distance (A* and euclidean) between starting locations
		// **************************************************
		//log("METRIC 2 ------------------------------");

		double meanAstar = 0;
		double standardDeviationAstar = 0;
		double meanEuclidean = 0;
		double standardDeviationEuclidean = 0;
		double distanceA, distanceE;
		double samples = 0;

		int startSize = startLocations.size();
		for(int i = 0; i < startSize ; i++) {
			for(int j = 0 ; j < startSize ; j++) {
				if(i < j) {
					distanceA = BWTA::getGroundDistance(BWAPI::TilePosition(startLocations[i]->getPosition()), BWAPI::TilePosition(startLocations[j]->getPosition()));
					distanceE = startLocations[i]->getPosition().getDistance(startLocations[j]->getPosition());

					meanAstar += distanceA;
					meanEuclidean += distanceE;
					samples++;
					//log("A*: " << distanceA << "\t" << "Euclidean: " << distanceE);
				}
			}
		}

		meanAstar = meanAstar / samples;
		meanEuclidean = meanEuclidean / samples;
// 		log("Mean A*: " << meanAstar);
// 		log("Mean Euclidean: " << meanEuclidean);

		for(int i = 0; i < startSize ; i++) {
			for(int j = 0 ; j < startSize ; j++) {
				if(i < j) {
					distanceA = BWTA::getGroundDistance(BWAPI::TilePosition(startLocations[i]->getPosition()), BWAPI::TilePosition(startLocations[j]->getPosition()));
					distanceE = startLocations[i]->getPosition().getDistance(startLocations[j]->getPosition());

					standardDeviationAstar += pow(distanceA-meanAstar,2); 
					standardDeviationEuclidean += pow(distanceE-meanEuclidean,2);
				}
			}
		}

		standardDeviationAstar = sqrt(standardDeviationAstar/samples);
		standardDeviationEuclidean = sqrt(standardDeviationEuclidean/samples);
// 		log("Standard Deviation A*: " << standardDeviationAstar);
// 		log("Standard Deviation Euclidean: " << standardDeviationEuclidean);
// 		log("Total Standard Deviation: " << standardDeviationAstar+standardDeviationEuclidean);
		LOG(standardDeviationAstar);
		LOG(meanAstar);
		LOG(standardDeviationEuclidean);
		LOG(meanEuclidean);

		// **************************************************
		// METRIC 3
		// --------------------------------------------------
		// Average distance starting-exp1-exp2
		// **************************************************
		//log("METRIC 3 ------------------------------");

		BaseExpansions baseExpansions;
		double meanExp1 = 0;
		double standardDeviationExp1 = 0;
		double meanExp2 = 0;
		double standardDeviationExp2 = 0;
		double distance;

		for (std::vector<BWTA::BaseLocation*>::const_iterator it = startLocations.begin(); it != startLocations.end(); ++it) {
			std::set<BWTA::BaseLocation*> allBases = BWTA::getBaseLocations();
			allBases.erase(*it);
			baseExpansions[*it].dist1 = (std::numeric_limits<double>::max)();
			baseExpansions[*it].dist2 = (std::numeric_limits<double>::max)();
			for(std::set<BWTA::BaseLocation*>::iterator i=allBases.begin();i!=allBases.end();++i) {
				// calculate distance
				distance = BWTA::getGroundDistance(BWAPI::TilePosition((*i)->getPosition()), BWAPI::TilePosition((*it)->getPosition()));
				// insert to map
				if (distance > -1 && baseExpansions[*it].dist2 > distance) {
					baseExpansions[*it].dist2 = distance;
					if (baseExpansions[*it].dist1 > baseExpansions[*it].dist2) {
						std::swap(baseExpansions[*it].dist1,baseExpansions[*it].dist2);
					}
				}
			}
			
			meanExp1 += baseExpansions[*it].dist1;
			meanExp2 += baseExpansions[*it].dist2;
			//log("Dist Exp1: " << baseExpansions[*it].dist1 << "\t" << "Dist Exp2: " << baseExpansions[*it].dist2);
		}

		meanExp1 = meanExp1 / startLocations.size();
		meanExp2 = meanExp2 / startLocations.size();
// 		log("Mean Exp1: " << meanExp1);
// 		log("Mean Exp2: " << meanExp2);

		for (std::vector<BWTA::BaseLocation*>::const_iterator it = startLocations.begin(); it != startLocations.end(); ++it) {
			standardDeviationExp1 += pow(baseExpansions[*it].dist1-meanExp1,2); 
			standardDeviationExp2 += pow(baseExpansions[*it].dist2-meanExp2,2);
		}

		standardDeviationExp1 = sqrt(standardDeviationExp1/startLocations.size());
		standardDeviationExp2 = sqrt(standardDeviationExp2/startLocations.size());
// 		log("Standard Deviation Exp1: " << standardDeviationExp1);
// 		log("Standard Deviation Exp2: " << standardDeviationExp2);
// 		log("Total Standard Deviation: " << standardDeviationExp1+standardDeviationExp2);
		LOG(standardDeviationExp1+standardDeviationExp2);
		LOG(meanExp1+meanExp2);

		// **************************************************
		// METRIC 4
		// --------------------------------------------------
		// Average distance starting-all expansions
		// **************************************************
		//log("METRIC 4 ------------------------------");

		// get all expansions
		std::set<BWTA::BaseLocation*> allExpansions = BWTA::getBaseLocations();
		for (std::vector<BWTA::BaseLocation*>::const_iterator it = startLocations.begin(); it != startLocations.end(); ++it) {
			allExpansions.erase(*it);
		}

		// TODO insted of 4 use startSize
		double meanExp[4] = {0,0,0,0};
		double standardDeviationExp[4] = {0,0,0,0};
		int id = 0;

		for (std::vector<BWTA::BaseLocation*>::const_iterator it = startLocations.begin(); it != startLocations.end(); ++it) {
			for(std::set<BWTA::BaseLocation*>::iterator i=allExpansions.begin();i!=allExpansions.end();++i) {
				distance = BWTA::getGroundDistance(BWAPI::TilePosition((*i)->getPosition()), BWAPI::TilePosition((*it)->getPosition()));
				meanExp[id] += distance;
			}
			
			meanExp[id] = meanExp[id] / allExpansions.size();
			//log("Mean All Exp Base"<< id <<": " << meanExp[id]);
			id++;
		}

		id = 0;
		for (std::vector<BWTA::BaseLocation*>::const_iterator it = startLocations.begin(); it != startLocations.end(); ++it) {
			for(std::set<BWTA::BaseLocation*>::iterator i=allExpansions.begin();i!=allExpansions.end();++i) {
				distance = BWTA::getGroundDistance(BWAPI::TilePosition((*i)->getPosition()), BWAPI::TilePosition((*it)->getPosition()));
				standardDeviationExp[id] += pow(distance-meanExp[id],2);
			}
			standardDeviationExp[id] = sqrt(standardDeviationExp[id]/allExpansions.size());
			//log("Standard Deviation All Exp Base"<< id <<": " << standardDeviationExp[id]);
			id++;
		}

		// standard deviation of the standard deviation
		double mean2 = 0;
		double standardDeviation2 = 0;

		for (int j=0;j<startSize;++j) {
			mean2 += standardDeviationExp[j];
		}

		mean2 = mean2/startSize;
		for (int j=0;j<startSize;++j) {
			standardDeviation2 += pow(standardDeviationExp[j]-mean2,2);
		}
		standardDeviation2 = sqrt(standardDeviation2/startSize);
		//log("Standard Deviation of Standad Deviation: " << standardDeviation2);
		LOG(standardDeviation2);
		LOG(mean2);


		// **************************************************
		// METRIC 5
		// --------------------------------------------------
		// Choke point symmetry
		// **************************************************
		//log("METRIC 5 ------------------------------");

		double meanChokeDis[6] = {0,0,0,0,0,0};
		double meanChokeWidth[6] = {0,0,0,0,0,0};
		id = 0;
		double dist1;
		double dist2;
		std::list<Chokepoint*> path;


		for(int i = 0; i < startSize ; i++) {
			for(int j = 0 ; j < startSize ; j++) {
				if(i < j) {
					path = getShortestPath2(BWAPI::TilePosition(startLocations[i]->getPosition()), BWAPI::TilePosition(startLocations[j]->getPosition()));
					int iterations = (int)floor((float)path.size()/2.0f);
					//log("Path size: " << path.size() << " Iterations: " << iterations);
					std::list<Chokepoint*>::iterator firstChoke=path.begin();
					std::list<Chokepoint*>::reverse_iterator lastChoke=path.rbegin();

					for (int k=0;k<=iterations;++k) {
						meanChokeWidth[id] += pow((*firstChoke)->getWidth()- (*lastChoke)->getWidth(),2);
						dist1 = BWTA::getGroundDistance(BWAPI::TilePosition(startLocations[i]->getPosition()), BWAPI::TilePosition((*firstChoke)->getCenter()));
						dist2 = BWTA::getGroundDistance(BWAPI::TilePosition(startLocations[j]->getPosition()), BWAPI::TilePosition((*lastChoke)->getCenter()));
						meanChokeDis[id] += pow(dist1-dist2,2);
						//log("Tmp choke width: " << meanChokeWidth[id] << "\t" << "Tmp choke dist: " << meanChokeDis[id]);
						firstChoke++;
						lastChoke++;
					}

					meanChokeWidth[id] = sqrt(meanChokeWidth[id]/iterations);
					meanChokeDis[id] = sqrt(meanChokeDis[id]/iterations);

					//log("StDev choke width: " << meanChokeWidth[id] << "\t" << "StDev choke dist: " << meanChokeDis[id]);
					id++;
				}
			}
		}

		// standard deviation of the standard deviation
		samples = 6; // TODO warning, looks like there is an error in BWTA A*
		double meanChokeWidth2 = 0;
		double standardDeviationChokeWidth2 = 0;
		double meanChokeDist2 = 0;
		double standardDeviationChokeDist2 = 0;

		for (int j=0;j<samples;++j) {
			meanChokeWidth2 += meanChokeWidth[j];
			meanChokeDist2 += meanChokeDis[j];
		}

		meanChokeWidth2 = meanChokeWidth2/samples;
		meanChokeDist2 = meanChokeDist2/samples;
		for (int j=0;j<samples;++j) {
			standardDeviationChokeWidth2 += pow(meanChokeWidth[j]-meanChokeWidth2,2);
			standardDeviationChokeDist2 += pow(meanChokeDis[j]-meanChokeDist2,2);
		}
		standardDeviationChokeWidth2 = sqrt(standardDeviationChokeWidth2/samples);
		standardDeviationChokeDist2 = sqrt(standardDeviationChokeDist2/samples);
// 		log("Standard Deviation of Standad Deviation (width): " << standardDeviationChokeWidth2);
// 		log("Standard Deviation of Standad Deviation (dist): " << standardDeviationChokeDist2);
		LOG(standardDeviationChokeDist2);
		LOG(standardDeviationChokeWidth2);

		// **************************************************
		// METRIC 6
		// --------------------------------------------------
		// Average diff in openness
		// **************************************************
// 		log("METRIC 6 ------------------------------");
// 		log("For this meric we need the threshold between 'big' and 'small' openness :(");
	}
}