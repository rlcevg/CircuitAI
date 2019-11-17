/*
 * SpringCallback.h
 *
 *  Created on: Nov 8, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SPRING_SPRINGCALLBACK_H_
#define SRC_CIRCUIT_SPRING_SPRINGCALLBACK_H_

#include <vector>

namespace springai {
	class Unit;
}

namespace circuit {

class COOAICallback {
public:
	COOAICallback();
	virtual ~COOAICallback();

	void GetFriendlyUnits(std::vector<springai::Unit*>& units) const;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SPRING_SPRINGCALLBACK_H_
