/*
 * FactoryTask.h
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FACTORYTASK_H_
#define SRC_CIRCUIT_TASK_FACTORYTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CBFactoryTask: public IBuilderTask {
public:
	CBFactoryTask(IUnitModule* mgr, Priority priority,
				  CCircuitDef* buildDef, CCircuitDef* reprDef, const springai::AIFloat3& position,
				  SResource cost, float shake, bool isPlop, int timeout);
	CBFactoryTask(IUnitModule* mgr);  // Load
	virtual ~CBFactoryTask();

	CCircuitDef* GetReprDef() const { return reprDef; }
	bool IsPlop() const { return isPlop; }
	void SetPosition(const springai::AIFloat3& pos) { position = pos; }

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;
protected:
	virtual void Cancel() override;

public:
	virtual void Activate() override;

private:
	virtual void FindBuildSite(CCircuitUnit* builder, const springai::AIFloat3& pos, float searchRadius) override;

	virtual bool Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

	CCircuitDef* reprDef;
	bool isPlop;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FACTORYTASK_H_
