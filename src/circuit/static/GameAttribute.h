/*
 * GameAttribute.h
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_GAMEATTRIBUTE_H_
#define SRC_CIRCUIT_STATIC_GAMEATTRIBUTE_H_

#include "static/SetupData.h"
#include "static/MetalData.h"
#include "static/TerrainData.h"

#include <unordered_set>

namespace circuit {

class CCircuitAI;

class CGameAttribute {
public:
	using Circuits = std::unordered_set<CCircuitAI*>;

	CGameAttribute();
	virtual ~CGameAttribute();

	void SetGameEnd(bool value);
	inline bool IsGameEnd() const { return gameEnd; }
	inline void RegisterAI(CCircuitAI* circuit) { circuits.insert(circuit); }
	inline void UnregisterAI(CCircuitAI* circuit) { circuits.erase(circuit); }

	inline const Circuits& GetCircuits() const { return circuits; }
	inline CSetupData& GetSetupData() { return setupData; }
	inline CMetalData& GetMetalData() { return metalData; }
	inline CTerrainData& GetTerrainData() { return terrainData; }

private:
	bool gameEnd;
	Circuits circuits;
	CSetupData setupData;
	CMetalData metalData;
	CTerrainData terrainData;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_GAMEATTRIBUTE_H_
