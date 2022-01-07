/*
 * EnergyManager.h
 *
 *  Created on: Jun 25, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_ENERGYMANAGER_H_
#define SRC_CIRCUIT_RESOURCE_ENERGYMANAGER_H_

#include "resource/EnergyData.h"

namespace circuit {

class CCircuitAI;
class CEnergyData;

class CEnergyManager {
public:
	CEnergyManager(CCircuitAI* circuit, CEnergyData* energyData);
	~CEnergyManager();

private:
	void ParseGeoSpots();

public:
	void SetAuthority(CCircuitAI* authority) { circuit = authority; }

	const CEnergyData::Geos& GetSpots() const { return energyData->GetSpots(); }

	bool IsSpotValid(int index, const springai::AIFloat3& pos) const;

private:
	CCircuitAI* circuit;
	CEnergyData* energyData;
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYMANAGER_H_
