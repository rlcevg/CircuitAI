/*
 * GameAttribute.h
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_GAMEATTRIBUTE_H_
#define SRC_CIRCUIT_STATIC_GAMEATTRIBUTE_H_

#include "setup/SetupData.h"
#include "resource/MetalData.h"
#include "terrain/TerrainData.h"
#include "util/MaskHandler.h"

#include <unordered_set>

namespace circuit {

class CCircuitAI;

class CGameAttribute {
public:
	using Circuits = std::unordered_set<CCircuitAI*>;

	CGameAttribute();
	virtual ~CGameAttribute();

	void Init(unsigned int seed);
	bool IsInitialized() const { return isInitialized; }

	void SetGameEnd(bool value);
	bool IsGameEnd() const { return isGameEnd; }
	void RegisterAI(CCircuitAI* circuit) { circuits.insert(circuit); }
	void UnregisterAI(CCircuitAI* circuit) { circuits.erase(circuit); }

	const Circuits& GetCircuits() const { return circuits; }
	CSetupData& GetSetupData() { return setupData; }
	CMetalData& GetMetalData() { return metalData; }
	CTerrainData& GetTerrainData() { return terrainData; }
	CMaskHandler& GetSideMasker() { return sideMasker; }
	CMaskHandler& GetRoleMasker() { return roleMasker; }

private:
	bool isInitialized;
	bool isGameEnd;
	Circuits circuits;
	CSetupData setupData;
	CMetalData metalData;
	CTerrainData terrainData;
	CMaskHandler sideMasker;
	CMaskHandler roleMasker;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_GAMEATTRIBUTE_H_
