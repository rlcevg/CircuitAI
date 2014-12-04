/*
 * BuilderManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_BUILDERMANAGER_H_
#define SRC_CIRCUIT_BUILDERMANAGER_H_

namespace circuit {

class CCircuitUnit;
class CBuilderTask;

class BuilderManager {
public:
	BuilderManager();
	virtual ~BuilderManager();

	void EnqueueTask(CBuilderTask *task);
	void DequeueTask(CBuilderTask *task);
	void AssignTask(CCircuitUnit *unit);
	void ResignTask(CCircuitUnit *unit);
};

} // namespace circuit

#endif // SRC_CIRCUIT_BUILDERMANAGER_H_
