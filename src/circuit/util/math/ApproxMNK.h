/*
 * ApproxMNK.h
 *
 *  Created on: Mar 22, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MATH_APPROXMNK_H_
#define SRC_CIRCUIT_UTIL_MATH_APPROXMNK_H_

#include <vector>

namespace circuit {

class CApproxMNK {
public:
	using Vector = std::vector<float>;

	CApproxMNK(unsigned int n, const Vector& X, const Vector& Y);
	virtual ~CApproxMNK();

	float GetValueAt(float x);

private:
	Vector coeffs;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_MATH_APPROXMNK_H_
