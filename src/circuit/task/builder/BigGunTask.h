/*
 * BigGunTask.h
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_BIGGUNTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_BIGGUNTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBBigGunTask: public IBuilderTask {
public:
	CBBigGunTask(IUnitModule* mgr, Priority priority,
				 CCircuitDef* buildDef, const springai::AIFloat3& position,
				 SResource cost, float shake, int timeout);
	CBBigGunTask(IUnitModule* mgr);  // Load
	virtual ~CBBigGunTask();

protected:
	virtual void Finish() override;

	virtual CAllyUnit* FindSameAlly(CCircuitUnit* builder, const std::vector<springai::Unit*>& friendlies) override;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BIGGUNTASK_H_
