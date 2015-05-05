/*
 * EnergyLink.h
 *
 *  Created on: Apr 29, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_
#define SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_

#include "unit/CircuitUnit.h"

#include <map>
#include <set>

namespace circuit {

class CEnergyLink {
public:
	struct SPylon {
		SPylon() : pos(-RgtVector), range(.0f) {}
		SPylon(const springai::AIFloat3& p, float r) : pos(p), range(r) {}
		springai::AIFloat3 pos;
		float range;
		std::set<SPylon*> neighbors;
	};

	CEnergyLink(const springai::AIFloat3& startPos, const springai::AIFloat3& endPos);
	virtual ~CEnergyLink();

	void AddPylon(CCircuitUnit::Id unitId, const springai::AIFloat3& pos, float range);
	int RemovePylon(CCircuitUnit::Id unitId);
	void CheckConnection();
	SPylon* GetConnectionHead();

	inline void SetBeingBuilt(bool value) { isBeingBuilt = value; }
	inline bool IsBeingBuilt() const { return isBeingBuilt; }
	inline bool IsFinished() const { return isFinished; }
	inline void SetValid(bool value) { isValid = value; }
	inline bool IsValid() const { return isValid; }
	void SetVertices(const springai::AIFloat3& startPos, const springai::AIFloat3& endPos);
	inline const springai::AIFloat3& GetStartPos() const { return startPos; }
	inline const springai::AIFloat3& GetEndPos() const { return endPos; }

private:
	std::map<CCircuitUnit::Id, SPylon*> pylons;  // owner
	bool isBeingBuilt;
	bool isFinished;
	bool isValid;

	springai::AIFloat3 startPos, endPos;
	std::set<SPylon*> startPylons;
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_
