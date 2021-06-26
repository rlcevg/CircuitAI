/*
 * EnergyData.h
 *
 *  Created on: Jun 25, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_ENERGYDATA_H_
#define SRC_CIRCUIT_RESOURCE_ENERGYDATA_H_

#include "AIFloat3.h"

#include <vector>

namespace circuit {

class CEnergyData {
public:
	using Geos = std::vector<springai::AIFloat3>;

	CEnergyData();
	~CEnergyData();
	void Init(const Geos&& spots);

	bool IsInitialized() const { return isInitialized; }
	bool IsEmpty() const { return spots.empty(); }

	const Geos& GetSpots() const { return spots; }

private:
	bool isInitialized;
	Geos spots;

};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYDATA_H_
