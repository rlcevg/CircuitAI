/*
 * LagrangeInterPol.h
 *
 *  Created on: Mar 21, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_LAGRANGEINTERPOL_H_
#define SRC_CIRCUIT_UTIL_LAGRANGEINTERPOL_H_

#include <vector>

namespace circuit {

class CLagrangeInterPol {
public:
	using Vector = std::vector<float>;

	CLagrangeInterPol(const Vector& X, const Vector& Y);
	virtual ~CLagrangeInterPol();

	float GetValueAt(float x);

private:
	std::vector<float> xs, ys, ts;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_LAGRANGEINTERPOL_H_
