/*
 * GaussSolver.cpp
 *
 *  Created on: Mar 21, 2015
 *      Author: rlcevg
 */

#include "util/math/GaussSolver.h"
#include "util/Utils.h"

#include <assert.h>
#include <math.h>

namespace circuit {

CGaussSolver::CGaussSolver()
{
}

CGaussSolver::~CGaussSolver()
{
}

const CGaussSolver::Vector& CGaussSolver::Solve(Matrix& A, Vector& B)
{
	/*
	 * Решение СЛАУ (системы линейных алгебраических уравнений)
	 * методом Гаусса с выбором главного элемента по всему полю
	 *
	 * A[0][0]*x[0] + A[0][1]*x[1] + A[0][2]*x[2] = B[0]
	 * A[1][0]*x[0] + A[1][1]*x[1] + A[1][2]*x[2] = B[1]
	 * A[2][0]*x[0] + A[2][1]*x[1] + A[2][2]*x[2] = B[2]
	 */
	assert(A.size() == B.size());
	int N = B.size();
	assert(N <= 200);  // Consider precision. Alternatively use iterative methods

	std::vector<int> MP(N);
	for (int i = 0; i < N; ++i) {
		MP[i] = i;
	}

	for (int k = 0; k < N - 1; ++k) {
		int h = k;
		int p = k;
		float max = std::fabs(A[h][p]);
		for (int i = k; i < N; ++i) {
			for (int j = k; j < N; ++j) {
				float tmp = std::fabs(A[i][j]);
				if (max < tmp) {
					h = i;
					p = j;
					max = tmp;
				}
			}
		}
		if (h != k) {
			for (int j = k; j < N; ++j) {
				std::swap(A[k][j], A[h][j]);
			}
			std::swap(B[k], B[h]);
		}
		if (p != k) {
			for (int i = k; i < N; ++i) {
				std::swap(A[i][k], A[i][p]);
			}
			std::swap(MP[k], MP[p]);
		}

		for (int i = k + 1; i < N; ++i) {
//			if (A[k][k] == 0) {  // FIXME: throw error
//				continue;
//			}
			float R = A[i][k] / A[k][k];
			for (int j = k + 1; j < N; ++j) {  // j = k should turn to 0 anyway, ignore it
				A[i][j] -= A[k][j] * R;
			}
			B[i] -= B[k] * R;
		}
	}

	Vector Z(N);
	Z[N - 1] = B[N - 1] / A[N - 1][N - 1];
	for (int i = N - 2; i >= 0; --i) {
		float S = 0.0;
		for (int j = i + 1; j < N; ++j) {
			S += A[i][j] * Z[j];
		}
		Z[i] = (B[i] - S) / A[i][i];
	}

	result.resize(N);
	for (int i = 0; i < N; ++i) {
		result[MP[i]] = Z[i];
	}

	return result;
}

const CGaussSolver::Vector& CGaussSolver::GetResult() const
{
	return result;
}

} // namespace circuit
