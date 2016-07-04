/*
 * DGunAction.h
 *
 *  Created on: Jul 29, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_DGUNACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_DGUNACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CDGunAction: public IUnitAction {
public:
	CDGunAction(CCircuitUnit* owner, float range);
	virtual ~CDGunAction();

	virtual void Update(CCircuitAI* circuit);

private:
	float range;
	unsigned int updCount;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ACTION_DGUNACTION_H_
