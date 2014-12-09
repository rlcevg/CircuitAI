/*
 * TerrainAnalyzer.h
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAINANALYZER_H_
#define SRC_CIRCUIT_TERRAINANALYZER_H_

#include "AIFloat3.h"

namespace springai {
	class UnitDef;
}

namespace circuit {

class CCircuitAI;

class CTerrainAnalyzer {
public:
	CTerrainAnalyzer(CCircuitAI* circuit);
	virtual ~CTerrainAnalyzer();

	int GetTerrainWidth();
	int GetTerrainHeight();
	springai::AIFloat3 FindBuildSiteSpace(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing);
	springai::AIFloat3 FindBuildSite(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing);

private:
	CCircuitAI* circuit;
	int terrainWidth;
	int terrainHeight;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAINANALYZER_H_
