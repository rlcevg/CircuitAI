/*
 * GridLink.h
 *
 *  Created on: Oct 8, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_GRIDLINK_H_
#define SRC_CIRCUIT_RESOURCE_GRIDLINK_H_

namespace circuit {

class IGridLink {
protected:
	IGridLink();
public:
	virtual ~IGridLink();

	void SetBeingBuilt(bool value) { isBeingBuilt = value; }
	bool IsBeingBuilt() const { return isBeingBuilt; }
	bool IsFinished() const { return isFinished; }
	void SetValid(bool value) { isValid = value; }
	bool IsValid() const { return isValid; }

protected:
	bool isBeingBuilt;
	bool isFinished;
	bool isValid;
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_GRIDLINK_H_
