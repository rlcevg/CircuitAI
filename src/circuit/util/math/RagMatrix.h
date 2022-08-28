/*
 * RagMatrix.h
 *
 *  Created on: Sep 7, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_RAGMATRIX_H_
#define SRC_CIRCUIT_UTIL_RAGMATRIX_H_

//#undef NDEBUG
#include <cassert>
#include <vector>

namespace circuit {

/*
 *    a b c d e
 *  a -
 *  b 1 -
 *  c 1 1 -
 *  d 1 1 1 -
 *  e 1 1 1 1 -
 */
template <class T>
class CRagMatrix final {
public:
	CRagMatrix() : nrows(0) {}
	CRagMatrix(int rows) {
		Resize(rows);
	}
	CRagMatrix(const CRagMatrix& matrix) : nrows(matrix.nrows) {
		data = matrix.data;
	}
	~CRagMatrix() {}

	int GetNrows() const { return nrows; }

	void Resize(int rows) {
		assert(rows > 0);  // rows == 1: empty matrix, less code for special cases, worse optimization.
		nrows = rows;
		int size = nrows * (nrows - 1) / 2;
		data.resize(size);
	}

	T FindClosestPair(int n, int& ir, int& jr) const {
		T distance = operator()(ir = 1, jr = 0);
		for (int i = 2; i < n; ++i) {
			for (int j = 0; j < i; ++j) {
				const T temp = operator()(i, j);
				if (temp < distance) {
					distance = temp;
					ir = i;
					jr = j;
				}
			}
		}
		return distance;
	}

	const T& operator()(int row, int column) const {
		assert((row >= 1 && row < nrows) && (column >= 0 && column < nrows - 1));
		int idx = (row * (row - 1)) / 2 + column;
		return data[idx];
	}
	T& operator()(int row, int column) {
		return const_cast<T&>(static_cast<const CRagMatrix&>(*this)(row, column));
	}

private:
	int nrows;
	std::vector<T> data;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_RAGMATRIX_H_
