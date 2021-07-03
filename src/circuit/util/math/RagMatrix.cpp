/*
 * RagMatrix.cpp
 *
 *  Created on: Sep 7, 2014
 *      Author: rlcevg
 */

#include "util/math/RagMatrix.h"
#include "util/Utils.h"

#include <cstring>  // std::memcpy
//#include <assert.h>

namespace circuit {

CRagMatrix::CRagMatrix(int nrows) :
		nrows(nrows)
{
//	assert(nrows > 1);  // ??
	int size = nrows * (nrows - 1) / 2;
	data = new float [size];
}

CRagMatrix::CRagMatrix(const CRagMatrix& matrix) :
		nrows(matrix.nrows)
{
	int size = nrows * (nrows - 1) / 2;
	data = new float [size];
	std::memcpy(data, matrix.data, sizeof(float) * size);
}

CRagMatrix::~CRagMatrix()
{
	delete[] data;
}

int CRagMatrix::GetNrows()
{
	return nrows;
}

float CRagMatrix::FindClosestPair(int n, int& ir, int&jr)
{
	float temp;
	float distance = operator()(1, 0);
	ir = 1;
	jr = 0;
	for (int i = 1; i < n; i++) {
		for (int j = 0; j < i; j++) {
			temp = operator()(i, j);
			if (temp < distance) {
				distance = temp;
				ir = i;
				jr = j;
			}
		}
	}
	return distance;
}

float& CRagMatrix::operator()(int row, int column) const
{
//	assert((row >= 1 && row < nrows) && (column >= 0 && column < nrows - 1));
	int idx = (row * (row - 1)) / 2 + column;
	return data[idx];
}

} // namespace circuit
