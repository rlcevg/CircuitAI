/*
 * GaussSolver.h
 *
 *  Created on: Mar 21, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MATH_GAUSSSOLVER_H_
#define SRC_CIRCUIT_UTIL_MATH_GAUSSSOLVER_H_

#include <vector>

namespace circuit {

class CGaussSolver {
public:
	using Vector = std::vector<float>;
	using Matrix = std::vector<Vector>;

	CGaussSolver();
	virtual ~CGaussSolver();

	const Vector& Solve(Matrix& A, Vector& B);  // Changes A, B
	const Vector& GetResult() const;

private:
	Vector result;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_MATH_GAUSSSOLVER_H_
