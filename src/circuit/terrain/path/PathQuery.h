/*
 * PathQuery.h
 *
 *  Created on: Apr 22, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATH_PATHQUERY_H_
#define SRC_CIRCUIT_TERRAIN_PATH_PATHQUERY_H_

#include "terrain/path/PathFinder.h"

namespace circuit {

class CCircuitUnit;

class IPathQuery {
public:
	enum class Type: char {NONE = 0, MULTI, SINGLE, COST, MAP, _SIZE_};
	enum class State: char {NONE = 0, PROCESS, READY, _SIZE_};

protected:
	IPathQuery(const CPathFinder& pathfinder, int id, Type type);
	virtual ~IPathQuery();

public:
	int GetId() const { return id; }
	Type GetType() const { return type; }

	void SetState(State value) { state.store(value); }
	State GetState() const { return state.load(); }

	void Init(const bool* canMoveArray, const float* threatArray,
			  NSMicroPather::CostFunc moveThreatFun, NSMicroPather::CostFunc moveFun,
			  CCircuitUnit* unit = nullptr);

	const bool* GetCanMoveArray() const { return canMoveArray; }
	const float* GetThreatArray() const { return threatArray; }
	NSMicroPather::CostFunc GetMoveFun() const { return moveFun; }
	NSMicroPather::CostFunc GetMoveThreatFun() const { return moveThreatFun; }

	CCircuitUnit* GetUnit() const { return unit; }

protected:
	const CPathFinder& pathfinder;  // NOTE: double-check threaded calls

	int id;
	Type type;
	std::atomic<State> state;

	const bool* canMoveArray;  // outdate after AREA_UPDATE_RATE
	const float* threatArray;  // outdate after THREAT_UPDATE_RATE
	NSMicroPather::CostFunc moveFun;  // AREA_UPDATE_RATE
	NSMicroPather::CostFunc moveThreatFun;  // THREAT_UPDATE_RATE + AREA_UPDATE_RATE

	CCircuitUnit* unit;  // optional, non-safe
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_PATHQUERY_H_
