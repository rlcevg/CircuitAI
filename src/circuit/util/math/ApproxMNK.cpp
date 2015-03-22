/*
 * ApproxMNK.cpp
 *
 *  Created on: Mar 22, 2015
 *      Author: rlcevg
 */

#include "util/math/ApproxMNK.h"
#include "util/math/GaussSolver.h"
#include "util/utils.h"

namespace circuit {

CApproxMNK::CApproxMNK(int n, const Vector& X, const Vector& Y)
{
	assert(n <= 4);  // for n > 4 don't use polynomial Fi(x)
	assert(X.size() == Y.size());
	assert(X.size() > n);
	/*
	 * weight = 1
	 * Fi(x) = x^i, i=[0, n]
	 */
	// TODO: Replace СЛАУ with orthogonal polynomials
	CGaussSolver::Matrix A(n + 1);
	CGaussSolver::Vector B(n + 1);
	Vector tX(X.size(), 1.0);
	A[0].resize(n + 1);
	for (int j = 0; j <= n; ++j) {
		float sumX = 0.0;
		float sumY = 0.0;
		for (int k = 0; k < X.size(); ++k) {
			sumX += tX[k];
			sumY += tX[k] * Y[k];
			tX[k] *= X[k];
		}
		A[0][j] = sumX;
		B[j] = sumY;
	}
	for (int i = 1; i <= n; ++i) {
		A[i].resize(n + 1);
		float sum = 0.0;
		for (int k = 0; k < X.size(); ++k) {
			sum += tX[k];
			tX[k] *= X[k];
		}
		A[i][n] = sum;
	}
	for (int i = 1; i <= n; ++i) {
		for (int j = 0; j < n; ++j) {
			A[i][j] = A[i - 1][j + 1];
		}
	}

	CGaussSolver solv;
	coeffs = solv.Solve(A, B);
}

CApproxMNK::~CApproxMNK()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

float CApproxMNK::GetValueAt(float x)
{
	float val = 0.0;
	float tx = 1.0;
	for (int i = 0; i < coeffs.size(); ++i) {
		val += coeffs[i] * tx;
		tx *= x;
	}
	return val;
}

} // namespace circuit
