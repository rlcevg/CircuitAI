/*
 * UnitDetail.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef UNITDETAIL_H_
#define UNITDETAIL_H_

#include <memory>

namespace circuit {

class Unit;

class CUnitDetail {
public:
	CUnitDetail();
	virtual ~CUnitDetail();

private:
	Unit* unit;
	std::shared_ptr<CUnitTask> task;
};

} // namespace circuit

#endif // UNITDETAIL_H_
