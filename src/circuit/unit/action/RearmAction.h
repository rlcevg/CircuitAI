/*
 * RearmAction.h
 *
 *  Created on: Dec 31, 2023
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ACTION_REARMACTION_H_
#define SRC_CIRCUIT_UNIT_ACTION_REARMACTION_H_

#include "unit/action/UnitAction.h"

namespace circuit {

class CRearmAction: public IUnitAction {
public:
	CRearmAction(CCircuitUnit* owner);
	virtual ~CRearmAction();

	virtual void Update(CCircuitAI* circuit) override;
	virtual void OnStart() override;

private:
	unsigned int updCount;
};

} // namespace circuit

#endif /* SRC_CIRCUIT_UNIT_ACTION_REARMACTION_H_ */
