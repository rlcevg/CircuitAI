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

#define MIN_COSTMOD	0.01f

class CEnergyLink {
public:
	struct SPylon {
		SPylon() : pos(-RgtVector), range(0.f) {}
		SPylon(const springai::AIFloat3& p, float r) : pos(p), range(r) {}
		springai::AIFloat3 pos;
		float range;
		std::set<SPylon*> neighbors;
	};

	CEnergyLink(int idx0, const springai::AIFloat3& P0, int idx1, const springai::AIFloat3& P1);
	virtual ~CEnergyLink();

	void AddPylon(ICoreUnit::Id unitId, const springai::AIFloat3& pos, float range);
	bool RemovePylon(ICoreUnit::Id unitId);
	void CheckConnection();
	SPylon* GetSourceHead() const { return source->head; }
	SPylon* GetTargetHead() const { return target->head; }

	void SetBeingBuilt(bool value) { isBeingBuilt = value; }
	bool IsBeingBuilt() const { return isBeingBuilt; }
	bool IsFinished() const { return isFinished; }
	void SetValid(bool value) { isValid = value; }
	bool IsValid() const { return isValid; }
	float GetCostMod() const { return costMod; }
	void SetSource(int index);
	const springai::AIFloat3& GetSourcePos() const { return source->pylon.pos; }
	const springai::AIFloat3& GetTargetPos() const { return target->pylon.pos; }

private:
	struct SVertex {
		SVertex(int index, const springai::AIFloat3& pos)
			: index(index), pylon(pos, 0.f), head(nullptr)
		{}
		const springai::AIFloat3& GetPos() const { return pylon.pos; }
		int index;
		SPylon pylon;
		SPylon* head;
	};
	SVertex *source, *target;  // owner

	std::map<ICoreUnit::Id, SPylon*> pylons;  // owner
	bool isBeingBuilt;
	bool isFinished;
	bool isValid;
	float invDistance;
	float costMod;
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_
