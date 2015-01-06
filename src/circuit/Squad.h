/*
 * Squad.h
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SQUAD_H_
#define SRC_CIRCUIT_SQUAD_H_

#include <set>

namespace circuit {

class CCircuitUnit;

class CSquad {
public:
	CSquad();
	virtual ~CSquad();

	std::set<CCircuitUnit*>& GetAssignees();

private:
	std::set<CCircuitUnit*> units;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SQUAD_H_
